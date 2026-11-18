#include "gui/crossfade_fx_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "effects/crossfade_fx_config.hpp"
#include "widget/dj_slider.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QKeyEvent>
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
    setFixedSize(540, 520);
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

    auto *header_hint_lbl = new QLabel(QStringLiteral("Duplaklikk potméteren = Alapérték"), this);
    header_hint_lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: #708098; font-style: italic;"));
    hdr_layout->addWidget(header_hint_lbl);

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
    auto *hint_lbl = new QLabel(QStringLiteral("Paraméterek: [Érték beírás / ± Gombok] | Moduláció: [Forrás választó + Bipoláris -100%..+100% Mod Depth]"), this);
    hint_lbl->setStyleSheet(QStringLiteral("font-size: 8px; color: #64758d;"));
    bottom_row->addWidget(hint_lbl, 1);

    auto *close_btn = new QPushButton(QStringLiteral("BEZÁRÁS"), this);
    close_btn->setFixedWidth(90);
    close_btn->setFixedHeight(24);
    close_btn->setAutoDefault(false);
    close_btn->setDefault(false);
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

    const auto &src_names = getModulationSourceNames();

    for (size_t i = 0; i < cur_def.params.size() && i < 8; ++i) {
        const auto &p_def = cur_def.params[i];
        const int p_idx = static_cast<int>(i);
        const float cur_val = decks_->crossfadeFxParamValue(fx_mode, p_idx);
        const int cur_mod = decks_->crossfadeFxParamMod(fx_mode, p_idx);
        const float cur_depth = decks_->crossfadeFxParamModDepth(fx_mode, p_idx);

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
        btn_minus->setAutoDefault(false);
        btn_minus->setDefault(false);
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
        btn_plus->setAutoDefault(false);
        btn_plus->setDefault(false);
        btn_plus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #00d2ff; border-color: #00d2ff; }"));
        row1->addWidget(btn_plus);

        // Base slider (Duplaklikk -> def_val)
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
        btn_d_minus->setAutoDefault(false);
        btn_d_minus->setDefault(false);
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
        btn_d_plus->setAutoDefault(false);
        btn_d_plus->setDefault(false);
        btn_d_plus->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a222e; color: #a0b0c8; border: 1px solid #2d3b4e; border-radius: 2px; font-weight: bold; font-size: 10px; padding: 0; } "
            "QPushButton:hover { background: #253346; color: #ffaa00; border-color: #ffaa00; }"));
        row2->addWidget(btn_d_plus);

        // Bipolar Depth Slider (Duplaklikk -> 0 / 0%)
        auto *mod_slider = new DJSlider(Qt::Horizontal, 0, card_frame);
        mod_slider->setRange(-1000, 1000);
        mod_slider->setValue(static_cast<int>(std::clamp(cur_depth, -1.0f, 1.0f) * 1000.0f));
        mod_slider->setToolTip(QStringLiteral("Bipoláris Moduláció Mélysége (Dupla kattintás: 0%): Középen 0% (Nincs), Jobbra: +, Balra: Invertált fázis -"));
        mod_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: #1c2534; border-radius: 2px; } "
            "QSlider::handle:horizontal { background: #ffaa00; width: 10px; margin: -3px 0; border-radius: 5px; } "
            "QSlider::handle:horizontal:hover { background: #ffcc00; }"));
        row2->addWidget(mod_slider, 1);

        card_layout->addLayout(row2);

        // -------------------------------------------------------------
        // SIGNALS & LOGIC
        // -------------------------------------------------------------
        auto applyValue = [this, fx_mode, p_idx, p_def, val_edit, base_slider](float v) {
            float val = std::clamp(v, p_def.min_val, p_def.max_val);
            val_edit->setText(QString::number(val, 'f', 2));
            float norm = (p_def.max_val > p_def.min_val) ? ((val - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
            {
                const QSignalBlocker b(base_slider);
                base_slider->setValue(static_cast<int>(std::clamp(norm, 0.0f, 1.0f) * 1000.0f));
            }
            if (decks_) decks_->setCrossfadeFxParamValue(fx_mode, p_idx, val);
        };

        connect(base_slider, &QSlider::valueChanged, this, [p_def, applyValue](int sval) {
            float val = p_def.min_val + (static_cast<float>(sval) / 1000.0f) * (p_def.max_val - p_def.min_val);
            applyValue(val);
        });

        connect(btn_minus, &QPushButton::clicked, this, [this, fx_mode, p_idx, p_def, applyValue]() {
            float cur = decks_ ? decks_->crossfadeFxParamValue(fx_mode, p_idx) : p_def.def_val;
            applyValue(cur - p_def.step);
        });

        connect(btn_plus, &QPushButton::clicked, this, [this, fx_mode, p_idx, p_def, applyValue]() {
            float cur = decks_ ? decks_->crossfadeFxParamValue(fx_mode, p_idx) : p_def.def_val;
            applyValue(cur + p_def.step);
        });

        connect(val_edit, &QLineEdit::editingFinished, this, [val_edit, applyValue]() {
            bool ok = false;
            float v = val_edit->text().toFloat(&ok);
            if (ok) applyValue(v);
        });

        connect(val_edit, &QLineEdit::returnPressed, this, [val_edit, applyValue]() {
            bool ok = false;
            float v = val_edit->text().toFloat(&ok);
            if (ok) applyValue(v);
            val_edit->clearFocus();
        });

        auto applyDepth = [this, fx_mode, p_idx, depth_edit, mod_slider, formatDepthText](float d) {
            float depth = std::clamp(d, -1.0f, 1.0f);
            depth_edit->setText(formatDepthText(depth));
            {
                const QSignalBlocker b(mod_slider);
                mod_slider->setValue(static_cast<int>(std::round(depth * 1000.0f)));
            }
            if (decks_) decks_->setCrossfadeFxParamModDepth(fx_mode, p_idx, depth);
        };

        connect(mod_slider, &QSlider::valueChanged, this, [applyDepth](int sval) {
            applyDepth(static_cast<float>(sval) / 1000.0f);
        });

        connect(btn_d_minus, &QPushButton::clicked, this, [this, fx_mode, p_idx, applyDepth]() {
            float cur = decks_ ? decks_->crossfadeFxParamModDepth(fx_mode, p_idx) : 0.0f;
            applyDepth(cur - 0.05f);
        });

        connect(btn_d_plus, &QPushButton::clicked, this, [this, fx_mode, p_idx, applyDepth]() {
            float cur = decks_ ? decks_->crossfadeFxParamModDepth(fx_mode, p_idx) : 0.0f;
            applyDepth(cur + 0.05f);
        });

        connect(depth_edit, &QLineEdit::editingFinished, this, [depth_edit, applyDepth]() {
            QString txt = depth_edit->text().remove('%').trimmed();
            bool ok = false;
            float pct = txt.toFloat(&ok);
            if (ok) applyDepth(pct / 100.0f);
        });

        connect(depth_edit, &QLineEdit::returnPressed, this, [depth_edit, applyDepth]() {
            QString txt = depth_edit->text().remove('%').trimmed();
            bool ok = false;
            float pct = txt.toFloat(&ok);
            if (ok) applyDepth(pct / 100.0f);
            depth_edit->clearFocus();
        });

        connect(mod_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, fx_mode, p_idx](int midx) {
            if (decks_) decks_->setCrossfadeFxParamMod(fx_mode, p_idx, midx);
        });

        params_layout_->addWidget(card_frame);
    }

    params_layout_->addStretch(1);
}

void CrossfadeFxDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}
