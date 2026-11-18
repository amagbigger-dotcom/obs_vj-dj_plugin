#include "gui/vj_fx_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "effects/vj_config.hpp"

#include <obs-frontend-api.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QColorDialog>
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
    setFixedSize(560, 560);
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

    // Color & Modulation Frame
    auto *color_box = new QFrame(this);
    color_box->setStyleSheet(QStringLiteral("background: #141922; border: 1px solid #222b3a; border-radius: 4px; padding: 4px;"));
    auto *color_box_layout = new QHBoxLayout(color_box);
    color_box_layout->setContentsMargins(6, 3, 6, 3);
    color_box_layout->setSpacing(6);

    auto *color_lbl = new QLabel(QStringLiteral("Téma Szín:"), color_box);
    color_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #8da0b8;"));
    color_box_layout->addWidget(color_lbl);

    color_btn_ = new QPushButton(color_box);
    color_btn_->setFixedHeight(22);
    color_btn_->setFixedWidth(110);
    color_btn_->setCursor(Qt::PointingHandCursor);
    connect(color_btn_, &QPushButton::clicked, this, [this]() {
        DeckPlayer *dp = currentDeck();
        if (!dp) return;
        VjDeckConfig cfg = dp->vjConfig();
        QColor init_col(0, 210, 255);
        if (cfg.custom_color_r >= 0.0f) {
            init_col = QColor::fromRgbF(cfg.custom_color_r, cfg.custom_color_g, cfg.custom_color_b);
        } else {
            init_col = (current_deck_idx_ == 1) ? QColor(255, 120, 0) : QColor(0, 210, 255);
        }
        const QColor sel = QColorDialog::getColor(init_col, this, QStringLiteral("Válassz VJ Shader Színt"));
        if (sel.isValid()) {
            dp->setVjCustomColor(static_cast<float>(sel.redF()), static_cast<float>(sel.greenF()), static_cast<float>(sel.blueF()), cfg.color_mod_mode);
            updateColorUi();
        }
    });
    color_box_layout->addWidget(color_btn_);

    auto *default_color_btn = new QPushButton(QStringLiteral("↺ Alap"), color_box);
    default_color_btn->setFixedHeight(22);
    default_color_btn->setFixedWidth(50);
    default_color_btn->setToolTip(QStringLiteral("Visszaállítás az eredeti Deck színre"));
    default_color_btn->setStyleSheet(QStringLiteral("QPushButton { background: #202735; color: #9bb; font-size: 9px; padding: 1px 4px; }"));
    connect(default_color_btn, &QPushButton::clicked, this, [this]() {
        if (DeckPlayer *dp = currentDeck()) {
            VjDeckConfig cfg = dp->vjConfig();
            dp->setVjCustomColor(-1.0f, -1.0f, -1.0f, cfg.color_mod_mode);
            updateColorUi();
        }
    });
    color_box_layout->addWidget(default_color_btn);

    auto *mod_lbl = new QLabel(QStringLiteral("Moduláció:"), color_box);
    mod_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #8da0b8; margin-left: 6px;"));
    color_box_layout->addWidget(mod_lbl);

    color_mod_combo_ = new QComboBox(color_box);
    color_mod_combo_->setFixedHeight(22);
    color_mod_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1c2534; color: #fff; border: 1px solid #334460; font-size: 9px; font-weight: bold; padding: 1px 4px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #141a24; color: #fff; selection-background-color: #0077aa; }"));
    color_mod_combo_->addItem(QStringLiteral("🔒 Fix Szín"), 0);
    color_mod_combo_->addItem(QStringLiteral("🥁 Lábdob Pulzus"), 1);
    color_mod_combo_->addItem(QStringLiteral("📈 Zenei Tónus"), 2);
    color_mod_combo_->addItem(QStringLiteral("🌈 Színkör"), 3);

    connect(color_mod_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (DeckPlayer *dp = currentDeck()) {
            VjDeckConfig cfg = dp->vjConfig();
            dp->setVjCustomColor(cfg.custom_color_r, cfg.custom_color_g, cfg.custom_color_b, idx);
        }
    });
    color_box_layout->addWidget(color_mod_combo_, 1);

    main_layout->addWidget(color_box);

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
            updateColorUi();
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

    updateColorUi();
    rebuildParamControls();
}

void VjFxDialog::updateColorUi()
{
    DeckPlayer *dp = currentDeck();
    if (!dp || !color_btn_ || !color_mod_combo_)
        return;

    const VjDeckConfig cfg = dp->vjConfig();
    const bool is_deck_b = (current_deck_idx_ == 1);
    QColor c(0, 210, 255);
    QString label_text;
    if (cfg.custom_color_r >= 0.0f) {
        c = QColor::fromRgbF(std::clamp(cfg.custom_color_r, 0.0f, 1.0f),
                             std::clamp(cfg.custom_color_g, 0.0f, 1.0f),
                             std::clamp(cfg.custom_color_b, 0.0f, 1.0f));
        label_text = c.name().toUpper();
    } else {
        c = is_deck_b ? QColor(255, 120, 0) : QColor(0, 210, 255);
        label_text = is_deck_b ? QStringLiteral("Deck B (Narancs)") : QStringLiteral("Deck A (Kék)");
    }

    const QString text_col = (c.lightness() > 140) ? QStringLiteral("#000") : QStringLiteral("#fff");
    color_btn_->setText(label_text);
    color_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border: 1px solid #556677; font-weight: bold; font-size: 9px; border-radius: 3px; } "
        "QPushButton:hover { border: 1px solid #ffffff; }"
    ).arg(c.name(), text_col));

    const QSignalBlocker blocker(color_mod_combo_);
    color_mod_combo_->setCurrentIndex(std::clamp(cfg.color_mod_mode, 0, 3));
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

    const auto &src_names = getModulationSourceNames();

    for (size_t i = 0; i < shader_def.params.size() && i < 8; ++i) {
        const auto &p_def = shader_def.params[i];
        const int p_idx = static_cast<int>(i);
        const float cur_val = cfg.params[i].value;
        const int cur_mod = cfg.params[i].mod_mode;
        const float cur_depth = cfg.params[i].mod_depth;

        auto *card_frame = new QFrame(params_container_);
        card_frame->setStyleSheet(QStringLiteral(
            "QFrame { background: #141922; border: 1px solid #222d3d; border-radius: 4px; padding: 3px; }"));
        auto *card_layout = new QVBoxLayout(card_frame);
        card_layout->setContentsMargins(6, 4, 6, 4);
        card_layout->setSpacing(4);

        // -------------------------------------------------------------
        // ROW 1: Alapérték (Base Value)
        // -------------------------------------------------------------
        auto *row1 = new QHBoxLayout();
        row1->setContentsMargins(0, 0, 0, 0);
        row1->setSpacing(6);

        // Param Name
        auto *name_lbl = new QLabel(QString::fromUtf8(p_def.name), card_frame);
        name_lbl->setFixedWidth(135);
        name_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #d0d8e8;"));
        row1->addWidget(name_lbl);

        // Stepper [-]
        auto *btn_minus = new QPushButton(QStringLiteral("-"), card_frame);
        btn_minus->setFixedSize(18, 18);
        btn_minus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #00d2ff; border-color: #00d2ff; }"));
        row1->addWidget(btn_minus);

        // Clickable numeric edit box
        auto *val_edit = new QLineEdit(QString::number(cur_val, 'f', 2), card_frame);
        val_edit->setFixedWidth(46);
        val_edit->setFixedHeight(18);
        val_edit->setAlignment(Qt::AlignCenter);
        val_edit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #0c1017; color: #00d2ff; border: 1px solid #253549; border-radius: 2px; font-family: monospace; font-size: 9px; font-weight: bold; } "
            "QLineEdit:focus { border: 1px solid #00d2ff; background: #121924; color: #fff; }"));
        row1->addWidget(val_edit);

        // Stepper [+]
        auto *btn_plus = new QPushButton(QStringLiteral("+"), card_frame);
        btn_plus->setFixedSize(18, 18);
        btn_plus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #00d2ff; border-color: #00d2ff; }"));
        row1->addWidget(btn_plus);

        // Base slider
        auto *base_slider = new QSlider(Qt::Horizontal, card_frame);
        base_slider->setRange(0, 1000);
        float norm = (p_def.max_val > p_def.min_val) ? ((cur_val - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
        norm = std::clamp(norm, 0.0f, 1.0f);
        base_slider->setValue(static_cast<int>(norm * 1000.0f));
        base_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: #202b3a; border-radius: 2px; } "
            "QSlider::sub-page:horizontal { background: #0088cc; border-radius: 2px; } "
            "QSlider::handle:horizontal { background: #00d2ff; width: 10px; margin: -3px 0; border-radius: 5px; }"));
        row1->addWidget(base_slider, 1);

        card_layout->addLayout(row1);

        // -------------------------------------------------------------
        // ROW 2: Modulációs Mátrix (Mod Source + Bipolar Depth)
        // -------------------------------------------------------------
        auto *row2 = new QHBoxLayout();
        row2->setContentsMargins(0, 0, 0, 0);
        row2->setSpacing(6);

        // Mod Source Combo
        auto *mod_combo = new QComboBox(card_frame);
        mod_combo->setFixedWidth(135);
        mod_combo->setFixedHeight(20);
        mod_combo->setStyleSheet(QStringLiteral(
            "QComboBox { background: #161e2a; color: #e0e6f0; border: 1px solid #263547; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 2px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: #10151f; color: #fff; selection-background-color: #006699; }"));
        for (size_t s = 0; s < src_names.size(); ++s) {
            mod_combo->addItem(QString::fromUtf8(src_names[s]), static_cast<int>(s));
        }
        mod_combo->setCurrentIndex(std::clamp(cur_mod, 0, static_cast<int>(src_names.size()) - 1));
        row2->addWidget(mod_combo);

        // Depth Stepper [-]
        auto *btn_d_minus = new QPushButton(QStringLiteral("-"), card_frame);
        btn_d_minus->setFixedSize(18, 18);
        btn_d_minus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #ffaa00; border-color: #ffaa00; }"));
        row2->addWidget(btn_d_minus);

        // Depth numeric edit box (e.g. "+50%", "±0%")
        auto formatDepthText = [](float d) -> QString {
            int pct = static_cast<int>(std::round(d * 100.0f));
            if (pct > 0) return QStringLiteral("+%1%").arg(pct);
            if (pct < 0) return QStringLiteral("%1%").arg(pct);
            return QStringLiteral("±0%");
        };

        auto *depth_edit = new QLineEdit(formatDepthText(cur_depth), card_frame);
        depth_edit->setFixedWidth(46);
        depth_edit->setFixedHeight(18);
        depth_edit->setAlignment(Qt::AlignCenter);
        depth_edit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #0c1017; color: #ffaa00; border: 1px solid #253549; border-radius: 2px; font-family: monospace; font-size: 9px; font-weight: bold; } "
            "QLineEdit:focus { border: 1px solid #ffaa00; background: #121924; color: #fff; }"));
        row2->addWidget(depth_edit);

        // Depth Stepper [+]
        auto *btn_d_plus = new QPushButton(QStringLiteral("+"), card_frame);
        btn_d_plus->setFixedSize(18, 18);
        btn_d_plus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #ffaa00; border-color: #ffaa00; }"));
        row2->addWidget(btn_d_plus);

        // Bipolar Depth Slider (-1000 .. +1000, 0 at center)
        auto *mod_slider = new QSlider(Qt::Horizontal, card_frame);
        mod_slider->setRange(-1000, 1000);
        mod_slider->setValue(static_cast<int>(std::clamp(cur_depth, -1.0f, 1.0f) * 1000.0f));
        mod_slider->setToolTip(QStringLiteral("Bipoláris Moduláció Mélysége: Középen 0% (Nincs), Jobbra: +, Balra: Invertált fázis -"));
        mod_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: #1c2534; border-radius: 2px; } "
            "QSlider::handle:horizontal { background: #ffaa00; width: 10px; margin: -3px 0; border-radius: 5px; } "
            "QSlider::handle:horizontal:hover { background: #ffcc00; }"));
        row2->addWidget(mod_slider, 1);

        card_layout->addLayout(row2);

        // -------------------------------------------------------------
        // SIGNALS & LOGIC
        // -------------------------------------------------------------
        auto applyValue = [this, p_idx, p_def, val_edit, base_slider](float v) {
            float val = std::clamp(v, p_def.min_val, p_def.max_val);
            val_edit->setText(QString::number(val, 'f', 2));
            float norm = (p_def.max_val > p_def.min_val) ? ((val - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
            {
                const QSignalBlocker b(base_slider);
                base_slider->setValue(static_cast<int>(std::clamp(norm, 0.0f, 1.0f) * 1000.0f));
            }
            if (DeckPlayer *d = currentDeck()) d->setVjParamValue(p_idx, val);
        };

        connect(base_slider, &QSlider::valueChanged, this, [p_def, applyValue](int sval) {
            float val = p_def.min_val + (static_cast<float>(sval) / 1000.0f) * (p_def.max_val - p_def.min_val);
            applyValue(val);
        });

        connect(btn_minus, &QPushButton::clicked, this, [this, p_idx, p_def, applyValue]() {
            DeckPlayer *d = currentDeck();
            float cur = d ? d->vjConfig().params[p_idx].value : p_def.def_val;
            applyValue(cur - p_def.step);
        });

        connect(btn_plus, &QPushButton::clicked, this, [this, p_idx, p_def, applyValue]() {
            DeckPlayer *d = currentDeck();
            float cur = d ? d->vjConfig().params[p_idx].value : p_def.def_val;
            applyValue(cur + p_def.step);
        });

        connect(val_edit, &QLineEdit::editingFinished, this, [val_edit, applyValue]() {
            bool ok = false;
            float v = val_edit->text().toFloat(&ok);
            if (ok) applyValue(v);
        });

        auto applyDepth = [this, p_idx, depth_edit, mod_slider, formatDepthText](float d) {
            float depth = std::clamp(d, -1.0f, 1.0f);
            depth_edit->setText(formatDepthText(depth));
            {
                const QSignalBlocker b(mod_slider);
                mod_slider->setValue(static_cast<int>(std::round(depth * 1000.0f)));
            }
            if (DeckPlayer *d = currentDeck()) d->setVjParamModDepth(p_idx, depth);
        };

        connect(mod_slider, &QSlider::valueChanged, this, [applyDepth](int sval) {
            applyDepth(static_cast<float>(sval) / 1000.0f);
        });

        connect(btn_d_minus, &QPushButton::clicked, this, [this, p_idx, applyDepth]() {
            DeckPlayer *d = currentDeck();
            float cur = d ? d->vjConfig().params[p_idx].mod_depth : 0.0f;
            applyDepth(cur - 0.05f);
        });

        connect(btn_d_plus, &QPushButton::clicked, this, [this, p_idx, applyDepth]() {
            DeckPlayer *d = currentDeck();
            float cur = d ? d->vjConfig().params[p_idx].mod_depth : 0.0f;
            applyDepth(cur + 0.05f);
        });

        connect(depth_edit, &QLineEdit::editingFinished, this, [depth_edit, applyDepth]() {
            QString txt = depth_edit->text().remove('%').trimmed();
            bool ok = false;
            float pct = txt.toFloat(&ok);
            if (ok) applyDepth(pct / 100.0f);
        });

        connect(mod_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, p_idx](int idx) {
            if (DeckPlayer *d = currentDeck()) {
                d->setVjParamMod(p_idx, idx);
            }
        });

        params_layout_->addWidget(card_frame);
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
