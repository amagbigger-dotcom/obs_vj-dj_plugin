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

CrossfadeFxTab::CrossfadeFxTab(DeckPair *decks, QWidget *parent)
    : QWidget(parent), decks_(decks)
{
    setupUi();
    refresh();
}

void CrossfadeFxTab::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(6);

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

    auto *trans_lbl = new QLabel(QStringLiteral("Áttűnés:"), fx_sel_frame);
    trans_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #00d2ff;"));
    fx_sel_layout->addWidget(trans_lbl);

    trans_combo_ = new QComboBox(fx_sel_frame);
    trans_combo_->setFixedHeight(24);
    trans_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1c2534; color: #00d2ff; border: 1px solid #0088cc; font-size: 10px; font-weight: bold; padding: 2px 6px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #141a24; color: #fff; selection-background-color: #0077aa; }"));
    trans_combo_->addItem(QStringLiteral("Azonnali (Cut)"), 0);
    trans_combo_->addItem(QStringLiteral("1 Beat (Alap)"), 1);
    trans_combo_->addItem(QStringLiteral("2 Beat"), 2);
    trans_combo_->addItem(QStringLiteral("4 Beat"), 4);
    trans_combo_->addItem(QStringLiteral("8 Beat"), 8);

    const int cur_beats = decks_ ? decks_->crossfadeFxTransBeats() : 1;
    int b_idx = trans_combo_->findData(cur_beats);
    if (b_idx >= 0) trans_combo_->setCurrentIndex(b_idx);

    connect(trans_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (decks_ && trans_combo_) {
            decks_->setCrossfadeFxTransBeats(trans_combo_->itemData(idx).toInt());
        }
    });
    fx_sel_layout->addWidget(trans_combo_);

    main_layout->addWidget(fx_sel_frame);

    // Keverési Fényerő Kiemelés (Blend Boost) Sáv
    auto *boost_frame = new QFrame(this);
    boost_frame->setStyleSheet(QStringLiteral("background: #111722; border: 1px solid #1f2c3d; border-radius: 4px; padding: 4px;"));
    auto *boost_layout = new QHBoxLayout(boost_frame);
    boost_layout->setContentsMargins(6, 2, 6, 2);
    boost_layout->setSpacing(8);

    auto *boost_lbl = new QLabel(QStringLiteral("Fényerő Kiemelés / Boost:"), boost_frame);
    boost_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #ffaa00;"));
    boost_layout->addWidget(boost_lbl);

    auto *boost_slider = new DJSlider(Qt::Horizontal, 100, boost_frame);
    boost_slider->setRange(50, 250);
    const int cur_boost = decks_ ? static_cast<int>(decks_->crossfadeFxBlendBoost() * 100.0f + 0.5f) : 100;
    boost_slider->setValue(cur_boost);
    boost_slider->setFixedHeight(18);
    boost_slider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; background: #162232; border-radius: 2px; } "
        "QSlider::handle:horizontal { background: #ffaa00; width: 14px; margin: -5px 0; border-radius: 3px; border: 1px solid #fff; } "
        "QSlider::handle:horizontal:hover { background: #ffbb22; }"));
    boost_layout->addWidget(boost_slider, 1);

    auto *boost_val_lbl = new QLabel(QString::asprintf("%d%%", cur_boost), boost_frame);
    boost_val_lbl->setFixedWidth(38);
    boost_val_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    boost_val_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; font-weight: bold; color: #ffaa00;"));
    boost_layout->addWidget(boost_val_lbl);

    connect(boost_slider, &QSlider::valueChanged, this, [this, boost_val_lbl](int val) {
        if (decks_) {
            decks_->setCrossfadeFxBlendBoost(static_cast<float>(val) / 100.0f);
        }
        boost_val_lbl->setText(QString::asprintf("%d%%", val));
    });

    main_layout->addWidget(boost_frame);

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
}

void CrossfadeFxTab::refresh()
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
    if (trans_combo_) {
        const int cur_beats = decks_->crossfadeFxTransBeats();
        const int b_idx = trans_combo_->findData(cur_beats);
        if (b_idx >= 0 && b_idx != trans_combo_->currentIndex()) {
            const QSignalBlocker blocker(trans_combo_);
            trans_combo_->setCurrentIndex(b_idx);
        }
    }
}

void CrossfadeFxTab::rebuildParamControls()
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

        // Mod Depth Clickable Edit Box (bipoláris: -100% .. +100%)
        auto *depth_edit = new QLineEdit(QStringLiteral("%1%").arg(static_cast<int>(std::round(cur_depth * 100.0f))), card_frame);
        depth_edit->setFixedWidth(46);
        depth_edit->setFixedHeight(18);
        depth_edit->setAlignment(Qt::AlignCenter);
        depth_edit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #0c1017; color: #ffaa00; border: 1px solid #352515; border-radius: 2px; font-family: monospace; font-size: 8px; font-weight: bold; } "
            "QLineEdit:focus { border: 1px solid #ffaa00; background: #20150a; color: #fff; }"));
        row2->addWidget(depth_edit);

        // Mod Depth Slider: -1000..+1000 (0 középen, duplaklikk -> 0)
        auto *depth_slider = new DJSlider(Qt::Horizontal, 0, card_frame);
        depth_slider->setRange(-1000, 1000);
        depth_slider->setValue(static_cast<int>(cur_depth * 1000.0f));
        depth_slider->setToolTip(QStringLiteral("Modulációs Mélység (-100%..+100%, Duplaklikk: 0%)"));
        depth_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: #202b3a; border-radius: 2px; } "
            "QSlider::handle:horizontal { background: #ffaa00; width: 10px; margin: -3px 0; border-radius: 5px; }"));
        row2->addWidget(depth_slider, 1);

        card_layout->addLayout(row2);

        // Interakciók
        auto applyVal = [this, fx_mode, p_idx, p_def, val_edit, base_slider](float v) {
            v = std::clamp(v, p_def.min_val, p_def.max_val);
            if (decks_) decks_->setCrossfadeFxParamValue(fx_mode, p_idx, v);
            val_edit->setText(QString::number(v, 'f', 2));
            const QSignalBlocker b(base_slider);
            float n = (p_def.max_val > p_def.min_val) ? ((v - p_def.min_val) / (p_def.max_val - p_def.min_val)) : 0.0f;
            base_slider->setValue(static_cast<int>(std::clamp(n, 0.0f, 1.0f) * 1000.0f));
        };

        connect(base_slider, &QSlider::valueChanged, this, [applyVal, p_def](int iv) {
            float n = static_cast<float>(iv) / 1000.0f;
            applyVal(p_def.min_val + n * (p_def.max_val - p_def.min_val));
        });

        connect(btn_minus, &QPushButton::clicked, this, [this, fx_mode, p_idx, p_def, applyVal]() {
            if (!decks_) return;
            const float step = (p_def.max_val - p_def.min_val) * 0.05f;
            applyVal(decks_->crossfadeFxParamValue(fx_mode, p_idx) - step);
        });

        connect(btn_plus, &QPushButton::clicked, this, [this, fx_mode, p_idx, p_def, applyVal]() {
            if (!decks_) return;
            const float step = (p_def.max_val - p_def.min_val) * 0.05f;
            applyVal(decks_->crossfadeFxParamValue(fx_mode, p_idx) + step);
        });

        connect(val_edit, &QLineEdit::editingFinished, this, [val_edit, applyVal]() {
            bool ok = false;
            float parsed = val_edit->text().toFloat(&ok);
            if (ok) applyVal(parsed);
            val_edit->clearFocus();
        });

        auto applyDepth = [this, fx_mode, p_idx, depth_edit, depth_slider](float d) {
            d = std::clamp(d, -1.0f, 1.0f);
            if (decks_) decks_->setCrossfadeFxParamModDepth(fx_mode, p_idx, d);
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

        connect(mod_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, fx_mode, p_idx](int midx) {
            if (decks_) decks_->setCrossfadeFxParamMod(fx_mode, p_idx, midx);
        });

        params_layout_->addWidget(card_frame);
    }

    params_layout_->addStretch(1);
}

void vdj_open_crossfade_fx_dialog()
{
    extern void vdj_open_vj_fx_dialog_tab(int tab_idx, int target_deck);
    vdj_open_vj_fx_dialog_tab(1, 0);
}

void vdj_free_crossfade_fx_dialog()
{
    // A közös dialógus felszabadítása végzi
}
