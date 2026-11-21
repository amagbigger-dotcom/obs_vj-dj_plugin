#include "gui/vj_fx_dialog.hpp"
#include "gui/crossfade_fx_dialog.hpp"
#include "gui/lfo_dialog.hpp"
#include "gui/isf_converter_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "effects/vj_config.hpp"
#include "widget/dj_slider.hpp"

#include <obs-frontend-api.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QColorDialog>
#include <QScrollArea>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QTabWidget>
#include <algorithm>

namespace {

constexpr const char *VJ_DIALOG_STYLESHEET = R"(
QDialog {
    background-color: #11141a;
    color: #c8d0de;
    font-family: 'Segoe UI', Arial, sans-serif;
}
QTabWidget::pane {
    border: 1px solid #1f2735;
    background: #11141a;
    border-radius: 4px;
}
QTabBar::tab {
    background: #161b24;
    color: #8da0b8;
    border: 1px solid #253347;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    padding: 5px 14px;
    font-weight: bold;
    font-size: 10px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background: #202b3a;
    color: #00d2ff;
    border-color: #00d2ff;
}
QTabBar::tab:hover:!selected {
    background: #1c2433;
    color: #e0e6f0;
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
    border-color: #00d2ff;
}
)";

static VjFxDialog *g_vj_fx_dialog = nullptr;

} // namespace

VjFxDialog::VjFxDialog(DeckPair *decks, QWidget *parent)
    : QDialog(parent), decks_(decks)
{
    g_vj_fx_dialog = this;

    setWindowTitle(QStringLiteral("OBS VJ Video & Shader Master FX"));
    setFixedSize(540, 560);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setStyleSheet(QString::fromUtf8(VJ_DIALOG_STYLESHEET));

    setupUi();
    selectDeck(0);
}

VjFxDialog::~VjFxDialog()
{
    if (g_vj_fx_dialog == this) {
        g_vj_fx_dialog = nullptr;
    }
}

void VjFxDialog::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 8, 10, 8);
    main_layout->setSpacing(6);

    // Title banner
    auto *hdr_layout = new QHBoxLayout();
    hdr_layout->setContentsMargins(0, 0, 0, 0);

    auto *title_lbl = new QLabel(QStringLiteral("⚡ VIDEO & SHADER MASTER FX"), this);
    title_lbl->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #00d2ff;"));
    hdr_layout->addWidget(title_lbl, 1);

    main_layout->addLayout(hdr_layout);

    // Tab Widget
    tab_widget_ = new QTabWidget(this);

    // Tab 0: Shader
    QWidget *shader_tab = createShaderTabWidget();
    tab_widget_->addTab(shader_tab, QStringLiteral("⚡ VJ SHADER"));

    // Tab 1: Crossfade FX
    crossfade_tab_ = new CrossfadeFxTab(decks_, tab_widget_);
    tab_widget_->addTab(crossfade_tab_, QStringLiteral("⚙ CROSSFADE FX"));

    // Tab 2: LFO
    lfo_tab_ = new LfoTab(decks_, tab_widget_);
    tab_widget_->addTab(lfo_tab_, QStringLiteral("〰 LFO MOTOR"));

    connect(tab_widget_, &QTabWidget::currentChanged, this, [this](int idx) {
        if (lfo_tab_) {
            lfo_tab_->setTimerActive(idx == 2);
        }
        if (idx == 0) {
            updateColorUi();
            rebuildParamControls();
        } else if (idx == 1 && crossfade_tab_) {
            crossfade_tab_->refresh();
        } else if (idx == 2 && lfo_tab_) {
            lfo_tab_->refresh();
        }
    });

    main_layout->addWidget(tab_widget_, 1);

    // Bottom Action Row
    auto *bottom_row = new QHBoxLayout();
    bottom_row->setContentsMargins(0, 2, 0, 0);

    auto *hint_lbl = new QLabel(QStringLiteral("Paraméterek: [Kattintható érték / ± Gombok] | Moduláció: [Bipoláris -100%..+100%]"), this);
    hint_lbl->setStyleSheet(QStringLiteral("font-size: 8px; color: #64758d;"));
    bottom_row->addWidget(hint_lbl, 1);

    auto *close_btn = new QPushButton(QStringLiteral("BEZÁRÁS"), this);
    close_btn->setFixedWidth(90);
    close_btn->setFixedHeight(22);
    close_btn->setAutoDefault(false);
    close_btn->setDefault(false);
    close_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #004466; color: #00d2ff; border: 1px solid #0088cc; border-radius: 3px; font-weight: bold; font-size: 10px; } "
        "QPushButton:hover { background: #006699; border-color: #00d2ff; color: #fff; }"));
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    bottom_row->addWidget(close_btn);

    main_layout->addLayout(bottom_row);
}

QWidget *VjFxDialog::createShaderTabWidget()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // Deck switcher bar
    auto *deck_bar = new QHBoxLayout();
    deck_bar->setSpacing(6);

    deck_a_btn_ = new QPushButton(QStringLiteral("◄ DECK A (KÉK)"), container);
    deck_a_btn_->setFixedHeight(24);
    deck_a_btn_->setAutoDefault(false);
    deck_a_btn_->setDefault(false);
    deck_a_btn_->setCursor(Qt::PointingHandCursor);
    connect(deck_a_btn_, &QPushButton::clicked, this, [this]() { selectDeck(0); });
    deck_bar->addWidget(deck_a_btn_, 1);

    deck_b_btn_ = new QPushButton(QStringLiteral("DECK B (NARANCS) ►"), container);
    deck_b_btn_->setFixedHeight(24);
    deck_b_btn_->setAutoDefault(false);
    deck_b_btn_->setDefault(false);
    deck_b_btn_->setCursor(Qt::PointingHandCursor);
    connect(deck_b_btn_, &QPushButton::clicked, this, [this]() { selectDeck(1); });
    deck_bar->addWidget(deck_b_btn_, 1);

    layout->addLayout(deck_bar);

    // Track Title info
    track_title_lbl_ = new QLabel(container);
    track_title_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #ffaa00; padding: 1px 0;"));
    layout->addWidget(track_title_lbl_);

    // Shader Selector Frame
    auto *shader_box = new QFrame(container);
    shader_box->setStyleSheet(QStringLiteral("background: #141922; border: 1px solid #222b3a; border-radius: 4px; padding: 4px;"));
    auto *shader_box_layout = new QHBoxLayout(shader_box);
    shader_box_layout->setContentsMargins(6, 3, 6, 3);
    shader_box_layout->setSpacing(6);

    auto *shader_lbl = new QLabel(QStringLiteral("Shader:"), shader_box);
    shader_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #8da0b8;"));
    shader_box_layout->addWidget(shader_lbl);

    shader_combo_ = new QComboBox(shader_box);
    shader_combo_->setFixedHeight(22);
    shader_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1c2534; color: #fff; border: 1px solid #334460; font-size: 9px; font-weight: bold; padding: 1px 6px; border-radius: 3px; } "
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

    auto *isf_btn = new QPushButton(QStringLiteral("🪄 ISF"), shader_box);
    isf_btn->setFixedHeight(22);
    isf_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #104868, stop:1 #0088cc); "
        "border: 1px solid #00b4d8; color: #ffffff; font-weight: bold; font-size: 9px; border-radius: 3px; padding: 1px 8px; } "
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #14608c, stop:1 #00aaff); border-color: #00d2ff; }"
    ));
    isf_btn->setAutoDefault(false);
    isf_btn->setDefault(false);
    isf_btn->setCursor(Qt::PointingHandCursor);
    connect(isf_btn, &QPushButton::clicked, this, [this]() {
        vdj_open_isf_converter_dialog(this);
    });
    shader_box_layout->addWidget(isf_btn);

    layout->addWidget(shader_box);

    // Color & Color Mod Row
    auto *col_row = new QHBoxLayout();
    col_row->setSpacing(6);

    auto *col_lbl = new QLabel(QStringLiteral("Alapszín:"), container);
    col_lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: #8da0b8; font-weight: bold;"));
    col_row->addWidget(col_lbl);

    color_btn_ = new QPushButton(container);
    color_btn_->setFixedHeight(20);
    color_btn_->setAutoDefault(false);
    color_btn_->setDefault(false);
    connect(color_btn_, &QPushButton::clicked, this, [this]() {
        DeckPlayer *dp = currentDeck();
        if (!dp) return;
        const VjDeckConfig cfg = dp->vjConfig();
        const bool is_deck_b = (current_deck_idx_ == 1);
        QColor init_col = (cfg.custom_color_r >= 0.0f)
            ? QColor::fromRgbF(cfg.custom_color_r, cfg.custom_color_g, cfg.custom_color_b)
            : (is_deck_b ? QColor(255, 120, 0) : QColor(0, 210, 255));

        const QColor picked = QColorDialog::getColor(init_col, this, QStringLiteral("Egyedi VJ Shader Alapszín"));
        if (picked.isValid()) {
            dp->setVjCustomColor(static_cast<float>(picked.redF()),
                                 static_cast<float>(picked.greenF()),
                                 static_cast<float>(picked.blueF()),
                                 cfg.color_mod_mode);
            updateColorUi();
        }
    });
    col_row->addWidget(color_btn_, 1);

    auto *reset_col_btn = new QPushButton(QStringLiteral("↺ Reset"), container);
    reset_col_btn->setFixedHeight(20);
    reset_col_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #161a22; color: #7f8fa4; border: 1px solid #283344; font-size: 8px; font-weight: bold; padding: 1px 6px; } "
        "QPushButton:hover { background: #222a36; color: #fff; }"));
    reset_col_btn->setAutoDefault(false);
    reset_col_btn->setDefault(false);
    connect(reset_col_btn, &QPushButton::clicked, this, [this]() {
        if (DeckPlayer *dp = currentDeck()) {
            const VjDeckConfig cfg = dp->vjConfig();
            dp->setVjCustomColor(-1.0f, -1.0f, -1.0f, cfg.color_mod_mode);
            updateColorUi();
        }
    });
    col_row->addWidget(reset_col_btn);

    auto *cmod_lbl = new QLabel(QStringLiteral("Színmod:"), container);
    cmod_lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: #8da0b8; font-weight: bold; margin-left: 4px;"));
    col_row->addWidget(cmod_lbl);

    color_mod_combo_ = new QComboBox(container);
    color_mod_combo_->setFixedHeight(20);
    color_mod_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #161e2a; color: #e0e6f0; border: 1px solid #263547; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 2px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #10151f; color: #fff; selection-background-color: #006699; }"));
    color_mod_combo_->addItem(QStringLiteral("Fix szín"), 0);
    color_mod_combo_->addItem(QStringLiteral("Kick villanás"), 1);
    color_mod_combo_->addItem(QStringLiteral("Zenei spektrum"), 2);
    color_mod_combo_->addItem(QStringLiteral("LFO ciklus"), 3);

    connect(color_mod_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (DeckPlayer *dp = currentDeck()) {
            const VjDeckConfig cfg = dp->vjConfig();
            dp->setVjCustomColor(cfg.custom_color_r, cfg.custom_color_g, cfg.custom_color_b, idx);
        }
    });
    col_row->addWidget(color_mod_combo_, 1);

    layout->addLayout(col_row);

    // Scroll Area for Parameters
    auto *scroll = new QScrollArea(container);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { border: 1px solid #1f2735; background: #0c0f14; border-radius: 4px; } "
        "QScrollBar:vertical { background: #12161f; width: 10px; margin: 0; } "
        "QScrollBar::handle:vertical { background: #263345; min-height: 20px; border-radius: 4px; } "
        "QScrollBar::handle:vertical:hover { background: #0088cc; }"));

    params_container_ = new QWidget(scroll);
    params_container_->setStyleSheet(QStringLiteral("background: transparent;"));
    params_layout_ = new QVBoxLayout(params_container_);
    params_layout_->setContentsMargins(6, 6, 6, 6);
    params_layout_->setSpacing(6);
    scroll->setWidget(params_container_);

    layout->addWidget(scroll, 1);

    return container;
}

DeckPlayer *VjFxDialog::currentDeck() const
{
    if (!decks_) return nullptr;
    return (current_deck_idx_ == 1) ? &decks_->deckB() : &decks_->deckA();
}

void VjFxDialog::selectTab(int tab_idx)
{
    if (tab_widget_) {
        tab_widget_->setCurrentIndex(std::clamp(tab_idx, 0, 2));
    }
}

void VjFxDialog::selectDeck(int deck_idx)
{
    current_deck_idx_ = (deck_idx == 1) ? 1 : 0;

    if (current_deck_idx_ == 0) {
        deck_a_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #004466; color: #00d2ff; border: 2px solid #00d2ff; font-weight: bold; font-size: 10px; border-radius: 4px; padding: 2px 8px; }"));
        deck_b_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #161b22; color: #8da0b8; border: 1px solid #2d3848; font-weight: bold; font-size: 10px; border-radius: 4px; padding: 2px 8px; } "
            "QPushButton:hover { background: #252f3e; color: #ff9900; }"));
    } else {
        deck_a_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #161b22; color: #8da0b8; border: 1px solid #2d3848; font-weight: bold; font-size: 10px; border-radius: 4px; padding: 2px 8px; } "
            "QPushButton:hover { background: #252f3e; color: #00d2ff; }"));
        deck_b_btn_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #663300; color: #ff9900; border: 2px solid #ff7700; font-weight: bold; font-size: 10px; border-radius: 4px; padding: 2px 8px; }"));
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

void VjFxDialog::reloadShaderList()
{
    if (!shader_combo_) return;
    const QSignalBlocker blocker(shader_combo_);
    shader_combo_->clear();

    const auto &defs = getVjShaderDefinitions();
    for (const auto &d : defs) {
        shader_combo_->addItem(QStringLiteral("%1. %2 (%3)").arg(d.id + 1).arg(d.name).arg(d.category), d.id);
    }

    if (DeckPlayer *dp = currentDeck()) {
        const VjDeckConfig cfg = dp->vjConfig();
        const int max_idx = defs.empty() ? 0 : static_cast<int>(defs.size()) - 1;
        shader_combo_->setCurrentIndex(std::clamp(cfg.shader_idx, 0, max_idx));
    }
    rebuildParamControls();
}

void VjFxDialog::refreshCrossfade()
{
    if (crossfade_tab_) crossfade_tab_->refresh();
}

void VjFxDialog::refreshLfo()
{
    if (lfo_tab_) lfo_tab_->refresh();
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
        "QPushButton { background-color: %1; color: %2; border: 1px solid #556677; font-weight: bold; font-size: 8px; border-radius: 3px; } "
        "QPushButton:hover { border: 1px solid #ffffff; }"
    ).arg(c.name(), text_col));

    const QSignalBlocker blocker(color_mod_combo_);
    color_mod_combo_->setCurrentIndex(std::clamp(cfg.color_mod_mode, 0, 3));
}

void VjFxDialog::rebuildParamControls()
{
    QLayoutItem *item = nullptr;
    while ((item = params_layout_->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    DeckPlayer *dp = currentDeck();
    if (!dp) return;

    const VjDeckConfig cfg = dp->vjConfig();
    const auto &defs = getVjShaderDefinitions();
    if (cfg.shader_idx < 0 || cfg.shader_idx >= static_cast<int>(defs.size()))
        return;

    const auto &cur_def = defs[cfg.shader_idx];
    const auto &src_names = getModulationSourceNames();

    for (int p_idx = 0; p_idx < 8; ++p_idx) {
        const auto &p_def = cur_def.params[p_idx];
        const float cur_val = cfg.params[p_idx].value;
        const int cur_mod = cfg.params[p_idx].mod_mode;
        const float cur_depth = cfg.params[p_idx].mod_depth;

        auto *card_frame = new QFrame(params_container_);
        card_frame->setStyleSheet(QStringLiteral(
            "QFrame { background: #141922; border: 1px solid #222d3d; border-radius: 4px; padding: 3px; }"));
        auto *card_layout = new QVBoxLayout(card_frame);
        card_layout->setContentsMargins(6, 4, 6, 4);
        card_layout->setSpacing(4);

        // Row 1: Base Value
        auto *row1 = new QHBoxLayout();
        row1->setContentsMargins(0, 0, 0, 0);
        row1->setSpacing(6);

        auto *name_lbl = new QLabel(QString::fromUtf8(p_def.name), card_frame);
        name_lbl->setFixedWidth(135);
        name_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #d0d8e8;"));
        row1->addWidget(name_lbl);

        auto *btn_minus = new QPushButton(QStringLiteral("-"), card_frame);
        btn_minus->setFixedSize(18, 18);
        btn_minus->setAutoDefault(false);
        btn_minus->setDefault(false);
        btn_minus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #00d2ff; border-color: #00d2ff; }"));
        row1->addWidget(btn_minus);

        auto *val_edit = new QLineEdit(QString::number(cur_val, 'f', 2), card_frame);
        val_edit->setFixedWidth(46);
        val_edit->setFixedHeight(18);
        val_edit->setAlignment(Qt::AlignCenter);
        val_edit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #0c1017; color: #00d2ff; border: 1px solid #253549; border-radius: 2px; font-family: monospace; font-size: 9px; font-weight: bold; } "
            "QLineEdit:focus { border: 1px solid #00d2ff; background: #121924; color: #fff; }"));
        row1->addWidget(val_edit);

        auto *btn_plus = new QPushButton(QStringLiteral("+"), card_frame);
        btn_plus->setFixedSize(18, 18);
        btn_plus->setAutoDefault(false);
        btn_plus->setDefault(false);
        btn_plus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #00d2ff; border-color: #00d2ff; }"));
        row1->addWidget(btn_plus);

        float def_norm = (p_def.max_val > p_def.min_val) ? ((p_def.def_val - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
        def_norm = std::clamp(def_norm, 0.0f, 1.0f);
        auto *base_slider = new DJSlider(Qt::Horizontal, static_cast<int>(def_norm * 1000.0f), card_frame);
        base_slider->setRange(0, 1000);
        float norm = (p_def.max_val > p_def.min_val) ? ((cur_val - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
        norm = std::clamp(norm, 0.0f, 1.0f);
        base_slider->setValue(static_cast<int>(norm * 1000.0f));
        base_slider->setToolTip(QStringLiteral("Alapérték (Dupla kattintás: Alaphelyzet: %1)").arg(p_def.def_val));
        base_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: #202b3a; border-radius: 2px; } "
            "QSlider::sub-page:horizontal { background: #0088cc; border-radius: 2px; } "
            "QSlider::handle:horizontal { background: #00d2ff; width: 10px; margin: -3px 0; border-radius: 5px; }"));
        row1->addWidget(base_slider, 1);

        card_layout->addLayout(row1);

        // Row 2: Modulation Matrix
        auto *row2 = new QHBoxLayout();
        row2->setContentsMargins(0, 0, 0, 0);
        row2->setSpacing(6);

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

        auto *depth_edit = new QLineEdit(QStringLiteral("%1%").arg(static_cast<int>(std::round(cur_depth * 100.0f))), card_frame);
        depth_edit->setFixedWidth(46);
        depth_edit->setFixedHeight(18);
        depth_edit->setAlignment(Qt::AlignCenter);
        depth_edit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #0c1017; color: #ffaa00; border: 1px solid #352515; border-radius: 2px; font-family: monospace; font-size: 8px; font-weight: bold; } "
            "QLineEdit:focus { border: 1px solid #ffaa00; background: #20150a; color: #fff; }"));
        row2->addWidget(depth_edit);

        auto *depth_slider = new DJSlider(Qt::Horizontal, 0, card_frame);
        depth_slider->setRange(-1000, 1000);
        depth_slider->setValue(static_cast<int>(cur_depth * 1000.0f));
        depth_slider->setToolTip(QStringLiteral("Modulációs Mélység (-100%..+100%, Duplaklikk: 0%)"));
        depth_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: #202b3a; border-radius: 2px; } "
            "QSlider::handle:horizontal { background: #ffaa00; width: 10px; margin: -3px 0; border-radius: 5px; }"));
        row2->addWidget(depth_slider, 1);

        card_layout->addLayout(row2);

        // Value updates
        auto applyVal = [this, p_idx, p_def, val_edit, base_slider](float v) {
            v = std::clamp(v, p_def.min_val, p_def.max_val);
            if (DeckPlayer *d = currentDeck()) d->setVjParamValue(p_idx, v);
            val_edit->setText(QString::number(v, 'f', 2));
            const QSignalBlocker b(base_slider);
            float n = (p_def.max_val > p_def.min_val) ? ((v - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
            base_slider->setValue(static_cast<int>(std::clamp(n, 0.0f, 1.0f) * 1000.0f));
        };

        connect(base_slider, &QSlider::valueChanged, this, [applyVal, p_def](int iv) {
            float n = static_cast<float>(iv) / 1000.0f;
            applyVal(p_def.min_val + n * (p_def.max_val - p_def.min_val));
        });

        connect(btn_minus, &QPushButton::clicked, this, [this, p_idx, p_def, applyVal]() {
            if (DeckPlayer *d = currentDeck()) {
                const float step = (p_def.max_val - p_def.min_val) * 0.05f;
                applyVal(d->vjConfig().params[p_idx].value - step);
            }
        });

        connect(btn_plus, &QPushButton::clicked, this, [this, p_idx, p_def, applyVal]() {
            if (DeckPlayer *d = currentDeck()) {
                const float step = (p_def.max_val - p_def.min_val) * 0.05f;
                applyVal(d->vjConfig().params[p_idx].value + step);
            }
        });

        connect(val_edit, &QLineEdit::editingFinished, this, [val_edit, applyVal]() {
            bool ok = false;
            float parsed = val_edit->text().toFloat(&ok);
            if (ok) applyVal(parsed);
            val_edit->clearFocus();
        });

        auto applyDepth = [this, p_idx, depth_edit, depth_slider](float d) {
            d = std::clamp(d, -1.0f, 1.0f);
            if (DeckPlayer *dr = currentDeck()) dr->setVjParamModDepth(p_idx, d);
            depth_edit->setText(QStringLiteral("%1%").arg(static_cast<int>(std::round(d * 100.0f))));
            const QSignalBlocker b(depth_slider);
            depth_slider->setValue(static_cast<int>(d * 1000.0f));
        };

        connect(depth_slider, &QSlider::valueChanged, this, [applyDepth](int iv) {
            applyDepth(static_cast<float>(iv) / 1000.0f);
        });

        connect(depth_edit, &QLineEdit::editingFinished, this, [depth_edit, applyDepth]() {
            QString s = depth_edit->text().remove(QLatin1Char('%')).trimmed();
            bool ok = false;
            int pct = s.toInt(&ok);
            if (ok) applyDepth(pct / 100.0f);
            depth_edit->clearFocus();
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

void VjFxDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect geom = screen->availableGeometry();
        const int target_w = width() > 0 ? width() : 540;
        const int x_pos = geom.right() - target_w - 12;
        const int y_pos = geom.top() + 38;
        move(x_pos, y_pos);
    }
}

void VjFxDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void vdj_open_vj_fx_dialog_tab(int tab_idx, int target_deck)
{
    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    if (!g_vj_fx_dialog) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_vj_fx_dialog = new VjFxDialog(decks, main_window);
    }
    g_vj_fx_dialog->selectTab(tab_idx);
    if (tab_idx == 0) {
        g_vj_fx_dialog->selectDeck(target_deck);
    } else if (tab_idx == 1) {
        g_vj_fx_dialog->refreshCrossfade();
    } else if (tab_idx == 2) {
        g_vj_fx_dialog->refreshLfo();
    }
    g_vj_fx_dialog->show();
    g_vj_fx_dialog->raise();
    g_vj_fx_dialog->activateWindow();
}

void vdj_open_vj_fx_dialog(int target_deck)
{
    vdj_open_vj_fx_dialog_tab(0, target_deck);
}

void vdj_free_vj_fx_dialog()
{
    vdj_free_isf_converter_dialog();
    if (g_vj_fx_dialog) {
        g_vj_fx_dialog->close();
        delete g_vj_fx_dialog;
        g_vj_fx_dialog = nullptr;
    }
}
