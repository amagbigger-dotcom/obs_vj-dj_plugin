#include "gui/crossfade_fx_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "effects/crossfade_fx_config.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <algorithm>

namespace {

constexpr const char *CF_FX_STYLESHEET = R"(
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
    font-size: 10px;
    padding: 3px 8px;
}
QPushButton:hover {
    background-color: #2b3648;
    border-color: #ffaa00;
}
)";

static CrossfadeFxDialog *g_cf_fx_dialog = nullptr;

} // namespace

CrossfadeFxDialog::CrossfadeFxDialog(DeckPair *decks, QWidget *parent)
    : QDialog(parent), decks_(decks)
{
    g_cf_fx_dialog = this;

    setWindowTitle(QStringLiteral("Crossfader Vizuális FX Paraméterek & Moduláció"));
    setFixedSize(480, 440);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setStyleSheet(QString::fromUtf8(CF_FX_STYLESHEET));

    setupUi();
    refresh();
}

CrossfadeFxDialog::~CrossfadeFxDialog()
{
    if (g_cf_fx_dialog == this) {
        g_cf_fx_dialog = nullptr;
    }
}

void CrossfadeFxDialog::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 10, 12, 10);
    main_layout->setSpacing(8);

    // Title banner
    auto *hdr_layout = new QHBoxLayout();
    hdr_layout->setContentsMargins(0, 0, 0, 0);
    hdr_layout->setSpacing(6);

    auto *icon_lbl = new QLabel(QStringLiteral("⚡"), this);
    icon_lbl->setStyleSheet(QStringLiteral("font-size: 13px; color: #ffaa00; font-weight: bold;"));
    hdr_layout->addWidget(icon_lbl);

    auto *title_lbl = new QLabel(QStringLiteral("CROSSFADER SHADER FX PARAMÉTEREK"), this);
    title_lbl->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #00d2ff;"));
    hdr_layout->addWidget(title_lbl, 1);

    reset_btn_ = new QPushButton(QStringLiteral("↺ Reset FX"), this);
    reset_btn_->setFixedHeight(22);
    reset_btn_->setToolTip(QStringLiteral("Aktuális effekt paramétereinek visszaállítása alapértelmezettre"));
    connect(reset_btn_, &QPushButton::clicked, this, [this]() {
        if (!decks_) return;
        const int cur_mode = decks_->crossfadeFxMode();
        decks_->resetCrossfadeFxParams(cur_mode);
        rebuildParamControls();
    });
    hdr_layout->addWidget(reset_btn_);

    main_layout->addLayout(hdr_layout);

    // FX Selector Row
    auto *fx_sel_frame = new QFrame(this);
    fx_sel_frame->setStyleSheet(QStringLiteral("background: #141922; border: 1px solid #222b3a; border-radius: 4px; padding: 4px;"));
    auto *fx_sel_layout = new QHBoxLayout(fx_sel_frame);
    fx_sel_layout->setContentsMargins(6, 4, 6, 4);
    fx_sel_layout->setSpacing(8);

    auto *sel_lbl = new QLabel(QStringLiteral("Aktív Crossfade Effekt:"), fx_sel_frame);
    sel_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #8da0b8;"));
    fx_sel_layout->addWidget(sel_lbl);

    fx_combo_ = new QComboBox(fx_sel_frame);
    fx_combo_->setFixedHeight(24);
    fx_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1c2534; color: #fff; border: 1px solid #334460; font-size: 10px; font-weight: bold; padding: 2px 6px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #141a24; color: #fff; selection-background-color: #0077aa; }"));

    const auto &defs = getCrossfadeFxDefinitions();
    for (size_t i = 0; i < defs.size(); ++i) {
        fx_combo_->addItem(QStringLiteral("%1: %2").arg(i).arg(QString::fromUtf8(defs[i].name)), static_cast<int>(i));
    }

    connect(fx_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (decks_) {
            decks_->setCrossfadeFxMode(idx);
        }
        rebuildParamControls();
    });
    fx_sel_layout->addWidget(fx_combo_, 1);

    main_layout->addWidget(fx_sel_frame);

    // Parameters Scroll Area
    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setStyleSheet(QStringLiteral(
        "QScrollArea { border: 1px solid #1f2735; background: #0c0f14; border-radius: 4px; } "
        "QScrollBar:vertical { background: #12161f; width: 10px; margin: 0; } "
        "QScrollBar::handle:vertical { background: #263345; min-height: 20px; border-radius: 4px; } "
        "QScrollBar::handle:vertical:hover { background: #0088cc; }"));

    params_container_ = new QWidget(scroll_area_);
    params_container_->setStyleSheet(QStringLiteral("background: transparent;"));
    params_layout_ = new QVBoxLayout(params_container_);
    params_layout_->setContentsMargins(6, 6, 6, 6);
    params_layout_->setSpacing(6);
    scroll_area_->setWidget(params_container_);

    main_layout->addWidget(scroll_area_, 1);

    // Bottom Action Row
    auto *bottom_row = new QHBoxLayout();
    auto *hint_lbl = new QLabel(QStringLiteral("Minden paraméter függetlenül vezérelhető Kézzel, Kick pulzussal vagy Zenei Tónussal."), this);
    hint_lbl->setStyleSheet(QStringLiteral("font-size: 8px; color: #64758d;"));
    bottom_row->addWidget(hint_lbl, 1);

    auto *close_btn = new QPushButton(QStringLiteral("BEZÁRÁS"), this);
    close_btn->setFixedWidth(90);
    close_btn->setFixedHeight(24);
    close_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #004466; color: #00d2ff; border: 1px solid #0088cc; border-radius: 3px; font-weight: bold; font-size: 10px; } "
        "QPushButton:hover { background: #006699; border-color: #00d2ff; color: #fff; }"));
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    bottom_row->addWidget(close_btn);

    main_layout->addLayout(bottom_row);
}

void CrossfadeFxDialog::refresh()
{
    if (!decks_) return;
    const int cur_mode = decks_->crossfadeFxMode();
    if (cur_mode != last_fx_mode_) {
        last_fx_mode_ = cur_mode;
        {
            const QSignalBlocker blocker(fx_combo_);
            fx_combo_->setCurrentIndex(std::clamp(cur_mode, 0, fx_combo_->count() - 1));
        }
        rebuildParamControls();
    }
}

void CrossfadeFxDialog::rebuildParamControls()
{
    if (!decks_) return;

    const int fx_mode = std::clamp(decks_->crossfadeFxMode(), 0, 15);
    last_fx_mode_ = fx_mode;

    // Clear previous parameter rows
    QLayoutItem *item = nullptr;
    while ((item = params_layout_->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    const auto &defs = getCrossfadeFxDefinitions();
    if (fx_mode >= static_cast<int>(defs.size())) return;

    const auto &cur_def = defs[fx_mode];

    if (cur_def.params.empty()) {
        auto *empty_lbl = new QLabel(QStringLiteral("Ehhez az effekthez nincs külön beállítható paraméter."), params_container_);
        empty_lbl->setAlignment(Qt::AlignCenter);
        empty_lbl->setStyleSheet(QStringLiteral("color: #64758d; font-size: 11px; padding: 20px;"));
        params_layout_->addWidget(empty_lbl);
        return;
    }

    for (size_t i = 0; i < cur_def.params.size() && i < 8; ++i) {
        const auto &p_def = cur_def.params[i];
        const int p_idx = static_cast<int>(i);
        const float cur_val = decks_->crossfadeFxParamValue(fx_mode, p_idx);
        const int cur_mod = decks_->crossfadeFxParamMod(fx_mode, p_idx);

        auto *row_frame = new QFrame(params_container_);
        row_frame->setStyleSheet(QStringLiteral(
            "QFrame { background: #161b24; border: 1px solid #242f3f; border-radius: 4px; padding: 4px; }"));
        auto *row_layout = new QHBoxLayout(row_frame);
        row_layout->setContentsMargins(8, 4, 8, 4);
        row_layout->setSpacing(8);

        // Parameter Name Label
        auto *name_lbl = new QLabel(QString::fromUtf8(p_def.name), row_frame);
        name_lbl->setFixedWidth(145);
        name_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #d0d8e8;"));
        row_layout->addWidget(name_lbl);

        // Value Label
        auto *val_lbl = new QLabel(QString::number(cur_val, 'f', 2), row_frame);
        val_lbl->setFixedWidth(40);
        val_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        val_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace; color: #ffaa00; font-weight: bold;"));
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

        connect(slider, &QSlider::valueChanged, this, [this, fx_mode, p_idx, p_def, val_lbl](int sval) {
            float val = p_def.min_val + (static_cast<float>(sval) / 1000.0f) * (p_def.max_val - p_def.min_val);
            val_lbl->setText(QString::number(val, 'f', 2));
            if (decks_) {
                decks_->setCrossfadeFxParamValue(fx_mode, p_idx, val);
            }
        });
        row_layout->addWidget(slider, 1);

        // Modulation dropdown
        auto *mod_combo = new QComboBox(row_frame);
        mod_combo->setFixedWidth(135);
        mod_combo->setFixedHeight(22);
        mod_combo->setStyleSheet(QStringLiteral(
            "QComboBox { background: #1a222f; color: #e0e6f0; border: 1px solid #2d3e56; font-size: 9px; padding: 1px 4px; border-radius: 3px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: #12171f; color: #fff; selection-background-color: #006699; }"));
        mod_combo->addItem(QStringLiteral("✋ Kézi (Off)"), 0);
        mod_combo->addItem(QStringLiteral("🥁 Kick Pulzus"), 1);
        mod_combo->addItem(QStringLiteral("📈 Zenei Tónus"), 2);
        mod_combo->addItem(QStringLiteral("⚡ Mindkettő"), 3);
        mod_combo->setCurrentIndex(std::clamp(cur_mod, 0, 3));

        connect(mod_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, fx_mode, p_idx](int midx) {
            if (decks_) {
                decks_->setCrossfadeFxParamMod(fx_mode, p_idx, midx);
            }
        });
        row_layout->addWidget(mod_combo);

        params_layout_->addWidget(row_frame);
    }

    params_layout_->addStretch(1);
}
