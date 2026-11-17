#include "gui/vj_fx_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "effects/vj_config.hpp"

#include <obs-frontend-api.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QScrollArea>
#include <QFileInfo>
#include <QSignalBlocker>
#include <algorithm>

namespace {

constexpr const char *VJ_DIALOG_STYLESHEET = R"(
QDialog {
    background-color: #11141a;
    color: #c8d0de;
    font-family: 'Segoe UI', Arial, sans-serif;
}
QPushButton {
    background-color: #1f2733;
    color: #e0e6f0;
    border: 1px solid #2d3848;
    border-radius: 4px;
    font-weight: bold;
    font-size: 11px;
}
QPushButton:hover {
    background-color: #2b3648;
    border-color: #0088cc;
}
)";

} // namespace

static VjFxDialog *g_vj_fx_dialog = nullptr;

VjFxDialog::VjFxDialog(DeckPair *decks, QWidget *parent)
    : QDialog(parent), decks_(decks)
{
    setWindowTitle(QStringLiteral("VJ Visualizer Shader & Paraméterek"));
    setFixedSize(540, 520);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setStyleSheet(QString::fromUtf8(VJ_DIALOG_STYLESHEET));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 10, 12, 10);
    main_layout->setSpacing(8);

    // Title banner
    auto *title_lbl = new QLabel(QStringLiteral("⚡ VJ VISUALIZER SHADER & ZENEI MODULÁCIÓ"), this);
    title_lbl->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold; color: #00d2ff;"));
    main_layout->addWidget(title_lbl);

    // Deck switcher bar
    auto *deck_bar = new QHBoxLayout();
    deck_bar->setSpacing(8);

    deck_a_btn_ = new QPushButton(QStringLiteral("◄ DECK A (KÉK)"), this);
    deck_a_btn_->setFixedHeight(26);
    deck_a_btn_->setCursor(Qt::PointingHandCursor);
    connect(deck_a_btn_, &QPushButton::clicked, this, [this]() { selectDeck(0); });
    deck_bar->addWidget(deck_a_btn_, 1);

    deck_b_btn_ = new QPushButton(QStringLiteral("DECK B (NARANCS) ►"), this);
    deck_b_btn_->setFixedHeight(26);
    deck_b_btn_->setCursor(Qt::PointingHandCursor);
    connect(deck_b_btn_, &QPushButton::clicked, this, [this]() { selectDeck(1); });
    deck_bar->addWidget(deck_b_btn_, 1);

    main_layout->addLayout(deck_bar);

    // Track Title info
    track_title_lbl_ = new QLabel(this);
    track_title_lbl_->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #ffaa00; padding: 2px 0;"));
    main_layout->addWidget(track_title_lbl_);

    // Shader Selector Frame
    auto *shader_box = new QFrame(this);
    shader_box->setStyleSheet(QStringLiteral("background: #141922; border: 1px solid #222b3a; border-radius: 4px; padding: 6px;"));
    auto *shader_box_layout = new QHBoxLayout(shader_box);
    shader_box_layout->setContentsMargins(6, 4, 6, 4);

    auto *shader_lbl = new QLabel(QStringLiteral("Shader Kiválasztása:"), shader_box);
    shader_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #8da0b8;"));
    shader_box_layout->addWidget(shader_lbl);

    shader_combo_ = new QComboBox(shader_box);
    shader_combo_->setFixedHeight(24);
    shader_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1c2534; color: #fff; border: 1px solid #334460; font-size: 10px; font-weight: bold; padding: 2px 6px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #141a24; color: #fff; selection-background-color: #0077aa; }"));

    const auto &defs = getVjShaderDefinitions();
    for (const auto &d : defs) {
        shader_combo_->addItem(QStringLiteral("%1. %2 (%3)").arg(d.id + 1).arg(d.name).arg(d.category), d.id);
    }

    connect(shader_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (DeckPlayer *dp = currentDeck()) {
            dp->setVjShaderIdx(idx);
            rebuildParamControls();
        }
    });
    shader_box_layout->addWidget(shader_combo_, 1);
    main_layout->addWidget(shader_box);

    // Scrollable Parameters Container
    auto *scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setStyleSheet(QStringLiteral(
        "QScrollArea { border: 1px solid #1f2735; background: #0c0f14; border-radius: 4px; } "
        "QScrollBar:vertical { background: #12161f; width: 10px; margin: 0; } "
        "QScrollBar::handle:vertical { background: #263345; min-height: 20px; border-radius: 4px; } "
        "QScrollBar::handle:vertical:hover { background: #0088cc; }"));

    params_container_ = new QWidget(scroll_area);
    params_container_->setStyleSheet(QStringLiteral("background: transparent;"));
    params_layout_ = new QVBoxLayout(params_container_);
    params_layout_->setContentsMargins(6, 6, 6, 6);
    params_layout_->setSpacing(6);
    scroll_area->setWidget(params_container_);
    main_layout->addWidget(scroll_area, 1);

    // Bottom Action Row
    auto *bottom_row = new QHBoxLayout();

    auto *reset_btn = new QPushButton(QStringLiteral("↺ Paraméterek Alaphelyzetbe"), this);
    reset_btn->setFixedHeight(24);
    reset_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #242b38; color: #a0b0c8; border: 1px solid #364254; border-radius: 3px; font-size: 10px; font-weight: bold; padding: 2px 8px; } "
        "QPushButton:hover { background: #333d4e; color: #fff; border-color: #556680; }"));
    connect(reset_btn, &QPushButton::clicked, this, [this]() {
        if (DeckPlayer *dp = currentDeck()) {
            int s_idx = shader_combo_->currentIndex();
            dp->setVjConfig(getDefaultVjConfig(s_idx));
            rebuildParamControls();
        }
    });
    bottom_row->addWidget(reset_btn);

    bottom_row->addStretch(1);

    auto *close_btn = new QPushButton(QStringLiteral("BEZÁRÁS"), this);
    close_btn->setFixedWidth(90);
    close_btn->setFixedHeight(24);
    close_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #004466; color: #00d2ff; border: 1px solid #0088cc; border-radius: 3px; font-weight: bold; font-size: 10px; } "
        "QPushButton:hover { background: #006699; border-color: #00d2ff; color: #fff; }"));
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    bottom_row->addWidget(close_btn);

    main_layout->addLayout(bottom_row);

    selectDeck(0);
}

VjFxDialog::~VjFxDialog()
{
    if (g_vj_fx_dialog == this) {
        g_vj_fx_dialog = nullptr;
    }
}

DeckPlayer *VjFxDialog::currentDeck() const
{
    if (!decks_)
        return nullptr;
    return (current_deck_idx_ == 1) ? &decks_->deckB() : &decks_->deckA();
}

void VjFxDialog::selectDeck(int deck_idx)
{
    current_deck_idx_ = (deck_idx == 1) ? 1 : 0;

    if (current_deck_idx_ == 0) {
        deck_a_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #004466; color: #00d2ff; border: 2px solid #00d2ff; font-weight: bold; font-size: 11px; border-radius: 4px; padding: 4px 12px; }"));
        deck_b_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #161b22; color: #8da0b8; border: 1px solid #2d3848; font-weight: bold; font-size: 11px; border-radius: 4px; padding: 4px 12px; } "
            "QPushButton:hover { background: #252f3e; color: #ff9900; }"));
    } else {
        deck_a_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #161b22; color: #8da0b8; border: 1px solid #2d3848; font-weight: bold; font-size: 11px; border-radius: 4px; padding: 4px 12px; } "
            "QPushButton:hover { background: #252f3e; color: #00d2ff; }"));
        deck_b_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #663300; color: #ff9900; border: 2px solid #ff7700; font-weight: bold; font-size: 11px; border-radius: 4px; padding: 4px 12px; }"));
    }

    DeckPlayer *dp = currentDeck();
    if (dp) {
        const QString path = dp->filePath();
        if (!path.isEmpty()) {
            QFileInfo fi(path);
            track_title_lbl_->setText(QStringLiteral("🎵 Track: %1").arg(fi.fileName()));
        } else {
            track_title_lbl_->setText(QStringLiteral("⚠️ Nincs betöltött zeneszám (Alapértelmezett profil)"));
        }
        VjDeckConfig cfg = dp->vjConfig();
        const QSignalBlocker blocker(shader_combo_);
        const auto &defs = getVjShaderDefinitions();
        const int max_idx = defs.empty() ? 0 : static_cast<int>(defs.size()) - 1;
        shader_combo_->setCurrentIndex(std::clamp(cfg.shader_idx, 0, max_idx));
    }

    rebuildParamControls();
}

void VjFxDialog::rebuildParamControls()
{
    // Clear previous parameter widgets
    QLayoutItem *item = nullptr;
    while ((item = params_layout_->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    DeckPlayer *dp = currentDeck();
    if (!dp)
        return;

    VjDeckConfig cfg = dp->vjConfig();
    const auto &defs = getVjShaderDefinitions();
    if (cfg.shader_idx < 0 || cfg.shader_idx >= static_cast<int>(defs.size()))
        return;

    const auto &shader_def = defs[cfg.shader_idx];

    for (size_t i = 0; i < shader_def.params.size() && i < 8; ++i) {
        const auto &p_def = shader_def.params[i];
        const float cur_val = cfg.params[i].value;
        const int cur_mod = cfg.params[i].mod_mode;

        auto *row_frame = new QFrame(params_container_);
        row_frame->setStyleSheet(QStringLiteral(
            "QFrame { background: #161b24; border: 1px solid #242f3f; border-radius: 4px; padding: 4px; }"));
        auto *row_layout = new QHBoxLayout(row_frame);
        row_layout->setContentsMargins(8, 4, 8, 4);
        row_layout->setSpacing(8);

        // Param Name
        auto *name_lbl = new QLabel(QString::fromUtf8(p_def.name), row_frame);
        name_lbl->setFixedWidth(130);
        name_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #d0d8e8;"));
        row_layout->addWidget(name_lbl);

        // Numeric Value Label
        auto *val_lbl = new QLabel(QString::number(cur_val, 'f', 2), row_frame);
        val_lbl->setFixedWidth(40);
        val_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        val_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace; color: #00d2ff; font-weight: bold;"));
        row_layout->addWidget(val_lbl);

        // Slider
        auto *slider = new QSlider(Qt::Horizontal, row_frame);
        slider->setRange(0, 1000);
        float norm = (p_def.max_val > p_def.min_val) ? ((cur_val - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
        norm = std::clamp(norm, 0.0f, 1.0f);
        slider->setValue(static_cast<int>(norm * 1000.0f));
        slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: #263345; border-radius: 2px; } "
            "QSlider::sub-page:horizontal { background: #0088cc; border-radius: 2px; } "
            "QSlider::handle:horizontal { background: #00d2ff; width: 12px; margin: -4px 0; border-radius: 6px; }"));

        const int p_idx = static_cast<int>(i);
        connect(slider, &QSlider::valueChanged, this, [this, p_idx, p_def, val_lbl](int sval) {
            float val = p_def.min_val + (static_cast<float>(sval) / 1000.0f) * (p_def.max_val - p_def.min_val);
            val_lbl->setText(QString::number(val, 'f', 2));
            if (DeckPlayer *d = currentDeck()) {
                d->setVjParamValue(p_idx, val);
            }
        });
        row_layout->addWidget(slider, 1);

        // Modulation dropdown
        auto *mod_combo = new QComboBox(row_frame);
        mod_combo->setFixedWidth(145);
        mod_combo->setFixedHeight(22);
        mod_combo->setStyleSheet(QStringLiteral(
            "QComboBox { background: #1a222f; color: #e0e6f0; border: 1px solid #2d3e56; font-size: 9px; padding: 1px 4px; border-radius: 3px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: #12171f; color: #fff; selection-background-color: #006699; }"));
        mod_combo->addItem(QStringLiteral("✋ Kézi (Manual)"), 0);
        mod_combo->addItem(QStringLiteral("🥁 Zene Üteme (Pulse)"), 1);
        mod_combo->addItem(QStringLiteral("🌈 Tónusos (Spectral)"), 2);
        mod_combo->addItem(QStringLiteral("⚡ Mindkettő (Both)"), 3);
        mod_combo->setCurrentIndex(std::clamp(cur_mod, 0, 3));

        connect(mod_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, p_idx](int idx) {
            if (DeckPlayer *d = currentDeck()) {
                d->setVjParamMod(p_idx, idx);
            }
        });
        row_layout->addWidget(mod_combo);

        params_layout_->addWidget(row_frame);
    }
    params_layout_->addStretch(1);
}

void vdj_open_vj_fx_dialog(int target_deck)
{
    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    if (!g_vj_fx_dialog) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_vj_fx_dialog = new VjFxDialog(decks, main_window);
    }
    g_vj_fx_dialog->selectDeck(target_deck);
    g_vj_fx_dialog->show();
    g_vj_fx_dialog->raise();
    g_vj_fx_dialog->activateWindow();
}

void vdj_free_vj_fx_dialog()
{
    if (g_vj_fx_dialog) {
        g_vj_fx_dialog->close();
        delete g_vj_fx_dialog;
        g_vj_fx_dialog = nullptr;
    }
}
