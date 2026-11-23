#include "gui/mixer_channel.hpp"
#include "engine/deck_engine.hpp"
#include "engine/mixer_fx.hpp"
#include "widget/dj_slider.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include <QSignalBlocker>
#include <QMenu>
#include <algorithm>
#include <cmath>

namespace {

QString formatGain(int v)
{
    if (v == 0) return QStringLiteral("0.0 dB");
    const float lin = 1.0f + static_cast<float>(v) / 100.0f;
    const float db = 20.0f * std::log10(std::max(0.01f, lin));
    return QString::asprintf("%+.1f dB", db);
}

QString formatEq(int v)
{
    if (v == 0) return QStringLiteral("0");
    const float val = static_cast<float>(v) / 100.0f;
    return QString::asprintf("%.2f", val);
}

QString formatFilter(int v, int fx_type = 0)
{
    if (v == 0) {
        return (fx_type == 0) ? QStringLiteral("FLAT") : QStringLiteral("OFF");
    }
    const char *prefix = "FLT";
    switch (static_cast<MixerFxType>(fx_type)) {
    case MixerFxType::FILTER:     prefix = (v < 0) ? "LPF" : "HPF"; break;
    case MixerFxType::DELAY:      prefix = (v < 0) ? "LP+DLY" : "HP+DLY"; break;
    case MixerFxType::REVERB:     prefix = (v < 0) ? "LP+REV" : "HP+REV"; break;
    case MixerFxType::FLANGER:    prefix = (v < 0) ? "LP+FLG" : "HP+FLG"; break;
    case MixerFxType::BEATMASHER: prefix = (v < 0) ? "LP+BTM" : "HP+BTM"; break;
    case MixerFxType::TURNTABLE:  prefix = (v < 0) ? "BRAKE"  : "SPIN"; break;
    case MixerFxType::RAMP_DELAY: prefix = (v < 0) ? "LP+RMP" : "HP+RMP"; break;
    case MixerFxType::MULHOLLAND: prefix = (v < 0) ? "LP+MUL" : "HP+MUL"; break;
    case MixerFxType::TRANSPOSE:  prefix = (v < 0) ? "DOWN"   : "UP"; break;
    default:                      prefix = (v < 0) ? "LPF" : "HPF"; break;
    }

    const int pct = (v < 0) ? -v : v;
    return QString::asprintf("%s %d%%", prefix, pct);
}

} // namespace

MixerChannelStrip::MixerChannelStrip(bool is_deck_a, DeckPlayer *deck, QWidget *parent)
    : QGroupBox(is_deck_a ? QStringLiteral("◄ DECK A") : QStringLiteral("DECK B ►"), parent),
      is_deck_a_(is_deck_a),
      deck_(deck)
{
    setupUi();
}

void MixerChannelStrip::setupUi()
{
    if (is_deck_a_) {
        setStyleSheet(QStringLiteral(
            "QGroupBox { background: #0c1522; border: 1px solid #143554; border-radius: 4px; padding: 4px; } "
            "QGroupBox::title { color: #00d2ff; font-weight: bold; font-size: 10px; }"));
    } else {
        setStyleSheet(QStringLiteral(
            "QGroupBox { background: #201208; border: 1px solid #542d14; border-radius: 4px; padding: 4px; } "
            "QGroupBox::title { color: #ff7700; font-weight: bold; font-size: 10px; }"));
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(3);

    auto addSliderRow = [this, layout](const QString &name, const QString &color,
                                       DJSlider **out_sl, QLabel **out_val_lbl, QLabel **out_title_lbl,
                                       int def_val, auto setter, auto formatter) {
        auto *row = new QHBoxLayout();
        row->setSpacing(4);
        row->setContentsMargins(0, 1, 0, 1);

        auto *lbl = new QLabel(name, this);
        lbl->setFixedWidth(26);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: %1;").arg(color));
        if (out_title_lbl) *out_title_lbl = lbl;

        auto *sl = new DJSlider(Qt::Horizontal, def_val, this);
        sl->setRange(-100, 100);
        sl->setValue(def_val);
        sl->setFixedHeight(18);
        if (is_deck_a_) {
            sl->setStyleSheet(QStringLiteral(
                "QSlider::groove:horizontal { height: 4px; background: #08101a; border: 1px solid #152c42; border-radius: 2px; } "
                "QSlider::handle:horizontal { background: #00aacc; width: 12px; margin: -4px 0; border-radius: 2px; border: 1px solid #ffffff; } "
                "QSlider::handle:horizontal:hover { background: #00d2ff; }"));
        } else {
            sl->setStyleSheet(QStringLiteral(
                "QSlider::groove:horizontal { height: 4px; background: #1a0c05; border: 1px solid #42220f; border-radius: 2px; } "
                "QSlider::handle:horizontal { background: #e06600; width: 12px; margin: -4px 0; border-radius: 2px; border: 1px solid #ffffff; } "
                "QSlider::handle:horizontal:hover { background: #ff7700; }"));
        }
        *out_sl = sl;

        auto *val_lbl = new QLabel(formatter(def_val), this);
        val_lbl->setFixedWidth(58);
        val_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        val_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-family: monospace; color: %1;")
            .arg(is_deck_a_ ? QStringLiteral("#7eb5e6") : QStringLiteral("#e6a87e")));
        *out_val_lbl = val_lbl;

        row->addWidget(lbl);
        row->addWidget(sl, 1);
        row->addWidget(val_lbl);
        layout->addLayout(row);

        QObject::connect(sl, &QSlider::valueChanged, sl, [this, setter, val_lbl, formatter](int val) {
            setter(1.0f + static_cast<float>(val) / 100.0f);
            val_lbl->setText(formatter(val));
        });
    };

    // 1. GAIN, HI (Red), MID (Green), LOW (Blue), FLT (Saturation)
    addSliderRow(QStringLiteral("GAIN"), QStringLiteral("#8da0b8"), &gain_slider_, &gain_val_lbl_, nullptr, 0,
                 [this](float v) { if (deck_) deck_->setGain(v); }, formatGain);
    addSliderRow(QStringLiteral("HI"), QStringLiteral("#ff4444"), &hi_slider_, &hi_val_lbl_, &hi_title_lbl_, 0,
                 [this](float v) { if (deck_) deck_->setEqHi(v); }, formatEq);
    addSliderRow(QStringLiteral("MID"), QStringLiteral("#00ff88"), &mid_slider_, &mid_val_lbl_, nullptr, 0,
                 [this](float v) { if (deck_) deck_->setEqMid(v); }, formatEq);
    addSliderRow(QStringLiteral("LOW"), QStringLiteral("#4488ff"), &low_slider_, &low_val_lbl_, &low_title_lbl_, 0,
                 [this](float v) { if (deck_) deck_->setEqLow(v); }, formatEq);
    addSliderRow(QStringLiteral("FLT"), QStringLiteral("#ef4444"), &flt_slider_, &flt_val_lbl_, &flt_title_lbl_, 0,
                 [this](float v) { if (deck_) deck_->setFilterVal(v - 1.0f); },
                 [this](int v) { return formatFilter(v, deck_ ? deck_->mixerFxType() : 0); });

    if (flt_title_lbl_) {
        flt_title_lbl_->setCursor(Qt::PointingHandCursor);
        flt_title_lbl_->setToolTip(QStringLiteral("Mixer FX: Kattints a slot váltásához"));
        class FxClickFilter : public QObject {
        public:
            std::function<void()> on_click;
            explicit FxClickFilter(QObject *parent, std::function<void()> cb) : QObject(parent), on_click(std::move(cb)) {}
            bool eventFilter(QObject *obj, QEvent *ev) override {
                if (ev->type() == QEvent::MouseButtonPress) {
                    if (on_click) on_click();
                    return true;
                }
                return QObject::eventFilter(obj, ev);
            }
        };
        flt_title_lbl_->installEventFilter(new FxClickFilter(flt_title_lbl_, [this]() {
            if (deck_) {
                const int next_slot = (deck_->activeSlot() + 1) % 4;
                deck_->selectSlot(next_slot);
                refresh();
            }
        }));
    }

    // 2. Buttons Row: Headphone CUE, Visual EQ & RGB Invert Toggle
    auto *btn_row = new QHBoxLayout();
    btn_row->setContentsMargins(0, 2, 0, 2);
    btn_row->setSpacing(3);

    hp_cue_btn_ = new QPushButton(is_deck_a_ ? QStringLiteral("🎧 CUE A") : QStringLiteral("🎧 CUE B"), this);
    hp_cue_btn_->setCheckable(true);
    hp_cue_btn_->setFixedHeight(22);
    hp_cue_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222c; border: 1px solid #2d3b4d; color: %1; font-weight: bold; font-size: 8px; border-radius: 3px; } "
        "QPushButton:checked { background: %2; color: #fff; border-color: %1; }")
        .arg(is_deck_a_ ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700"))
        .arg(is_deck_a_ ? QStringLiteral("#0088cc") : QStringLiteral("#cc5500")));
    connect(hp_cue_btn_, &QPushButton::toggled, this, [this](bool chk) {
        if (deck_) deck_->setHeadphoneCue(chk);
    });
    btn_row->addWidget(hp_cue_btn_, 1);

    viz_eq_btn_ = new QPushButton(QStringLiteral("🎨 VIZ EQ"), this);
    viz_eq_btn_->setCheckable(true);
    viz_eq_btn_->setChecked(deck_ ? deck_->isVisualEqEnabled() : true);
    viz_eq_btn_->setFixedHeight(22);
    viz_eq_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #161b24; border: 1px solid #2b3545; color: #7f8fa4; font-weight: bold; font-size: 8px; border-radius: 3px; } "
        "QPushButton:checked { background: #133824; color: #00ff88; border-color: #00bb55; }"));
    viz_eq_btn_->setToolTip(QStringLiteral("Vizuális EQ: Bekapcsolva a HI/MID/LOW/FLT potik színezik a videót. Kikapcsolva csak a hangot szűrik."));
    connect(viz_eq_btn_, &QPushButton::toggled, this, [this](bool chk) {
        if (deck_) deck_->setVisualEqEnabled(chk);
    });
    btn_row->addWidget(viz_eq_btn_, 1);

    rgb_inv_btn_ = new QPushButton(QStringLiteral("⇄ BGR"), this);
    rgb_inv_btn_->setCheckable(true);
    rgb_inv_btn_->setChecked(deck_ ? deck_->isRgbInverted() : false);
    rgb_inv_btn_->setFixedHeight(22);
    rgb_inv_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #161b24; border: 1px solid #2b3545; color: #7f8fa4; font-weight: bold; font-size: 8px; border-radius: 3px; } "
        "QPushButton:checked { background: #382413; color: #ffaa00; border-color: #bb7700; }"));
    rgb_inv_btn_->setToolTip(QStringLiteral("RGB & Szaturáció Megfordítása:\n"
                                            "Bekapcsolva: HI = Kék (Blue), MID = Zöld (Green), LOW = Vörös (Red),\n"
                                            "és a szaturáció (FLT) iránya is megfordul!"));
    connect(rgb_inv_btn_, &QPushButton::toggled, this, [this](bool chk) {
        if (deck_) {
            deck_->setRgbInverted(chk);
            updateSliderStyles(chk);
        }
    });
    btn_row->addWidget(rgb_inv_btn_, 1);

    layout->addLayout(btn_row);

    // 3. Vertical Volume Fader and Peak Meter Row + 4-Slot Vertical FX Column
    auto *fader_meter_row = new QHBoxLayout();
    fader_meter_row->setContentsMargins(0, 3, 0, 0);
    fader_meter_row->setSpacing(6);

    vol_slider_ = new DJSlider(Qt::Vertical, 80, this);
    vol_slider_->setRange(0, 100);
    vol_slider_->setValue(80);
    vol_slider_->setFixedHeight(66);
    vol_slider_->setToolTip(QStringLiteral("Channel Volume Fader (Default: 80% Unity Line Level)"));
    if (is_deck_a_) {
        vol_slider_->setStyleSheet(QStringLiteral(
            "QSlider::groove:vertical { width: 6px; background: #08101a; border: 1px solid #152c42; border-radius: 3px; } "
            "QSlider::handle:vertical { background: #00d2ff; height: 16px; margin: 0 -5px; border-radius: 3px; border: 1px solid #ffffff; } "
            "QSlider::handle:vertical:hover { background: #38e0ff; }"));
    } else {
        vol_slider_->setStyleSheet(QStringLiteral(
            "QSlider::groove:vertical { width: 6px; background: #1a0c05; border: 1px solid #42220f; border-radius: 3px; } "
            "QSlider::handle:vertical { background: #ff7700; height: 16px; margin: 0 -5px; border-radius: 3px; border: 1px solid #ffffff; } "
            "QSlider::handle:vertical:hover { background: #ffa038; }"));
    }

    meter_ = new QProgressBar(this);
    meter_->setOrientation(Qt::Vertical);
    meter_->setRange(0, 100);
    meter_->setValue(0);
    meter_->setFixedWidth(7);
    meter_->setFixedHeight(66);
    meter_->setTextVisible(false);
    if (is_deck_a_) {
        meter_->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #08101a; border: 1px solid #152c42; border-radius: 2px; } "
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:1, x2:0, y2:0, stop:0 #005588, stop:0.8 #00d2ff, stop:1 #ffffff); border-radius: 1px; }"));
    } else {
        meter_->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #1a0c05; border: 1px solid #42220f; border-radius: 2px; } "
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:1, x2:0, y2:0, stop:0 #883300, stop:0.8 #ff7700, stop:1 #ffffff); border-radius: 1px; }"));
    }

    vol_val_lbl_ = new QLabel(QStringLiteral("80%"), this);
    vol_val_lbl_->setFixedWidth(30);
    vol_val_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    vol_val_lbl_->setStyleSheet(QStringLiteral("font-size: 8px; font-family: monospace; color: %1;")
        .arg(is_deck_a_ ? QStringLiteral("#7eb5e6") : QStringLiteral("#e6a87e")));

    connect(vol_slider_, &QSlider::valueChanged, this, [this](int val) {
        if (deck_) deck_->setVolume(static_cast<float>(val) / 100.0f);
        if (vol_val_lbl_) vol_val_lbl_->setText(QString::asprintf("%d%%", val));
    });

    auto *fader_box = new QHBoxLayout();
    fader_box->setContentsMargins(0, 0, 0, 0);
    fader_box->setSpacing(4);
    fader_box->addWidget(vol_slider_);
    fader_box->addWidget(meter_);
    fader_box->addWidget(vol_val_lbl_);

    // Függőleges 4-slot FX gombok sávja (egymás alatt, a fader mellett, közép felé tájolva)
    auto *slots_box = new QVBoxLayout();
    slots_box->setContentsMargins(0, 0, 0, 0);
    slots_box->setSpacing(2);

    for (int s = 0; s < 4; ++s) {
        auto *btn = new QPushButton(this);
        btn->setCheckable(true);
        btn->setFixedHeight(15);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(btn, &QPushButton::customContextMenuRequested, this, [this, s](const QPoint &pos) {
            if (slot_btn_[s]) {
                showSlotMenu(s, slot_btn_[s]->mapToGlobal(pos));
            }
        });

        connect(btn, &QPushButton::clicked, this, [this, s]() {
            if (!deck_) return;
            if (deck_->activeSlot() == s) {
                if (slot_btn_[s]) {
                    showSlotMenu(s, slot_btn_[s]->mapToGlobal(QPoint(0, slot_btn_[s]->height())));
                }
            } else {
                deck_->selectSlot(s);
                refresh();
            }
        });

        slot_btn_[s] = btn;
        slots_box->addWidget(btn);
        updateSlotBtn(s);
    }

    if (is_deck_a_) {
        // Deck A: Fader balra, 4 gomb jobbra (közép felé)
        fader_meter_row->addLayout(fader_box);
        fader_meter_row->addLayout(slots_box, 1);
    } else {
        // Deck B: 4 gomb balra (közép felé), Fader jobbra
        fader_meter_row->addLayout(slots_box, 1);
        fader_meter_row->addLayout(fader_box);
    }

    layout->addLayout(fader_meter_row);

    // 4. Volume FX Dropdown
    vol_fx_combo_ = new QComboBox(this);
    vol_fx_combo_->setFixedHeight(20);
    if (is_deck_a_) {
        vol_fx_combo_->setStyleSheet(QStringLiteral(
            "QComboBox { background: #09131e; color: #7eb5e6; border: 1px solid #173450; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 3px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: #0c1826; color: #fff; selection-background-color: #005588; }"));
    } else {
        vol_fx_combo_->setStyleSheet(QStringLiteral(
            "QComboBox { background: #1e0f07; color: #e6a87e; border: 1px solid #502b13; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 3px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: #26150a; color: #fff; selection-background-color: #884400; }"));
    }
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Ki"), 0);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Sötétedés"), 1);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Kiégés"), 2);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Homályosítás"), 3);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Kör Szűkülés"), 4);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: 16:9 Téglalap"), 5);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: 1:1 Négyzet"), 6);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Rombusz / Gyémánt"), 7);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Vízszintes Lamellák"), 8);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Függőleges Lamellák"), 9);
    vol_fx_combo_->addItem(QStringLiteral("VOL FX: Zoom Középre"), 10);
    vol_fx_combo_->setCurrentIndex(deck_ ? deck_->volumeFxMode() : 0);
    connect(vol_fx_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (deck_) deck_->setVolumeFxMode(idx);
    });
    layout->addWidget(vol_fx_combo_);
}

void MixerChannelStrip::updateSliderStyles(bool inverted)
{
    if (hi_title_lbl_) {
        hi_title_lbl_->setStyleSheet(inverted ? QStringLiteral("font-size: 9px; font-weight: bold; color: #4488ff;")
                                             : QStringLiteral("font-size: 9px; font-weight: bold; color: #ff4444;"));
        hi_title_lbl_->setToolTip(inverted ? QStringLiteral("Magashang EQ -> KÉK (Blue) csatorna")
                                           : QStringLiteral("Magashang EQ -> VÖRÖS (Red) csatorna"));
    }
    if (low_title_lbl_) {
        low_title_lbl_->setStyleSheet(inverted ? QStringLiteral("font-size: 9px; font-weight: bold; color: #ff4444;")
                                              : QStringLiteral("font-size: 9px; font-weight: bold; color: #4488ff;"));
        low_title_lbl_->setToolTip(inverted ? QStringLiteral("Mélyhang EQ -> VÖRÖS (Red) csatorna")
                                            : QStringLiteral("Mélyhang EQ -> KÉK (Blue) csatorna"));
    }
}

void MixerChannelStrip::bindDeck(DeckPlayer *deck)
{
    deck_ = deck;
    if (viz_eq_btn_ && deck_) {
        viz_eq_btn_->setChecked(deck_->isVisualEqEnabled());
    }
    if (rgb_inv_btn_ && deck_) {
        const bool inv = deck_->isRgbInverted();
        rgb_inv_btn_->setChecked(inv);
        updateSliderStyles(inv);
    }
    if (vol_fx_combo_ && deck_) {
        vol_fx_combo_->setCurrentIndex(deck_->volumeFxMode());
    }
    for (int s = 0; s < 4; ++s) {
        updateSlotBtn(s);
    }
    refresh();
}

void MixerChannelStrip::refresh()
{
    if (!deck_)
        return;

    auto updateSliderFromVal = [](DJSlider *sl, QLabel *lbl, int target_val, auto formatFunc) {
        if (sl && !sl->isSliderDown()) {
            if (std::abs(sl->value() - target_val) > 1) {
                QSignalBlocker blocker(sl);
                sl->setValue(target_val);
            }
        }
        if (lbl && sl) {
            lbl->setText(formatFunc(sl->value()));
        }
    };

    const int fx_mode = deck_->mixerFxType();
    if (flt_title_lbl_) {
        const auto fx_t = static_cast<MixerFxType>(fx_mode);
        flt_title_lbl_->setText(QString::fromUtf8(MixerFxProcessor::shortName(fx_t)));
        flt_title_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: %1;")
            .arg(QString::fromUtf8(MixerFxProcessor::colorHex(fx_t))));
        flt_title_lbl_->setToolTip(QStringLiteral("Mixer FX: %1 - Kattints a slot váltásához!")
            .arg(QString::fromUtf8(MixerFxProcessor::name(fx_t))));
    }

    updateSliderFromVal(gain_slider_, gain_val_lbl_, static_cast<int>((deck_->gain() - 1.0f) * 100.0f), formatGain);
    updateSliderFromVal(hi_slider_, hi_val_lbl_, static_cast<int>((deck_->eqHi() - 1.0f) * 100.0f), formatEq);
    updateSliderFromVal(mid_slider_, mid_val_lbl_, static_cast<int>((deck_->eqMid() - 1.0f) * 100.0f), formatEq);
    updateSliderFromVal(low_slider_, low_val_lbl_, static_cast<int>((deck_->eqLow() - 1.0f) * 100.0f), formatEq);
    updateSliderFromVal(flt_slider_, flt_val_lbl_, static_cast<int>(deck_->filterVal() * 100.0f),
                        [fx_mode](int v) { return formatFilter(v, fx_mode); });

    if (vol_slider_ && !vol_slider_->isSliderDown()) {
        const int target_vol = static_cast<int>(deck_->volume() * 100.0f);
        if (std::abs(vol_slider_->value() - target_vol) > 1) {
            QSignalBlocker blocker(vol_slider_);
            vol_slider_->setValue(target_vol);
        }
    }
    if (vol_val_lbl_ && vol_slider_) {
        vol_val_lbl_->setText(QString::asprintf("%d%%", vol_slider_->value()));
    }

    if (hp_cue_btn_ && hp_cue_btn_->isChecked() != deck_->isHeadphoneCue()) {
        QSignalBlocker blocker(hp_cue_btn_);
        hp_cue_btn_->setChecked(deck_->isHeadphoneCue());
    }

    if (viz_eq_btn_ && viz_eq_btn_->isChecked() != deck_->isVisualEqEnabled()) {
        QSignalBlocker blocker(viz_eq_btn_);
        viz_eq_btn_->setChecked(deck_->isVisualEqEnabled());
    }

    if (rgb_inv_btn_ && rgb_inv_btn_->isChecked() != deck_->isRgbInverted()) {
        QSignalBlocker blocker(rgb_inv_btn_);
        const bool inv = deck_->isRgbInverted();
        rgb_inv_btn_->setChecked(inv);
        updateSliderStyles(inv);
    }

    if (meter_) {
        float amp = 0.0f;
        if (deck_ && deck_->isPlaying() && deck_->hasAudioBuffer() && !deck_->isAtEnd()) {
            amp = deck_->currentAmplitude();
        }
        const float vol = (deck_ ? deck_->volume() : 0.8f) * (deck_ ? deck_->gain() : 1.0f);
        const int level = static_cast<int>(std::clamp(amp * vol * 100.0f, 0.0f, 100.0f));
        meter_->setValue(level);
    }

    for (int s = 0; s < 4; ++s) {
        updateSlotBtn(s);
    }
}

void MixerChannelStrip::updateSlotBtn(int slot)
{
    if (slot < 0 || slot >= 4 || !slot_btn_[slot] || !deck_) return;

    QPushButton *btn = slot_btn_[slot];
    const int fx_type_idx = deck_->slotFxType(slot);
    const auto fx_type = static_cast<MixerFxType>(fx_type_idx);
    const bool is_active = (deck_->activeSlot() == slot);

    const QString short_name = QString::fromUtf8(MixerFxProcessor::shortName(fx_type));
    const QString full_name = QString::fromUtf8(MixerFxProcessor::name(fx_type));
    const QString hex_col = QString::fromUtf8(MixerFxProcessor::colorHex(fx_type));

    QString icon = QStringLiteral("🔴");
    switch (fx_type) {
    case MixerFxType::FILTER:     icon = QStringLiteral("🔴"); break;
    case MixerFxType::DELAY:      icon = QStringLiteral("🔵"); break;
    case MixerFxType::REVERB:     icon = QStringLiteral("🟢"); break;
    case MixerFxType::FLANGER:    icon = QStringLiteral("🟡"); break;
    case MixerFxType::BEATMASHER: icon = QStringLiteral("🟣"); break;
    case MixerFxType::TURNTABLE:  icon = QStringLiteral("🟠"); break;
    case MixerFxType::RAMP_DELAY: icon = QStringLiteral("🚀"); break;
    case MixerFxType::MULHOLLAND: icon = QStringLiteral("🔥"); break;
    case MixerFxType::TRANSPOSE:  icon = QStringLiteral("🎚️"); break;
    default: break;
    }

    btn->setText(QStringLiteral("%1 %2 ▾").arg(icon, short_name));
    btn->setToolTip(QStringLiteral(
        "Mixer FX %1 Slot %2: %3\n"
        "• Bal klikk: Kiválasztás / Menü megnyitása\n"
        "• Jobb klikk: Effekt csere (9 NI effekt)")
        .arg(is_deck_a_ ? QStringLiteral("Deck A") : QStringLiteral("Deck B"))
        .arg(slot + 1)
        .arg(full_name));

    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #161c24; border: 1px solid #2b3545; color: #8da0b8; font-weight: bold; font-size: 8px; border-radius: 2px; padding: 0px 2px; } "
        "QPushButton:hover { background: #222b38; color: #ffffff; border: 1px solid #4a5568; } "
        "QPushButton:checked { background: %1; color: #ffffff; border: 1px solid %1; font-weight: bold; }")
        .arg(hex_col));

    if (btn->isChecked() != is_active) {
        QSignalBlocker blocker(btn);
        btn->setChecked(is_active);
    }
}

void MixerChannelStrip::showSlotMenu(int slot, const QPoint &pos)
{
    if (slot < 0 || slot >= 4 || !deck_) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #161c24; border: 1px solid #2b3545; color: #e2e8f0; font-size: 11px; padding: 4px; } "
        "QMenu::item { padding: 4px 20px 4px 10px; border-radius: 2px; } "
        "QMenu::item:selected { background: #2563eb; color: #ffffff; } "
        "QMenu::item:checked { font-weight: bold; } "
        "QMenu::separator { height: 1px; background: #2b3545; margin: 4px 0; }"));

    struct FxOption {
        MixerFxType type;
        const char *label;
    };
    const std::array<FxOption, 9> options = {{
        {MixerFxType::FILTER,     "🔴 Filter (Classic HPF/LPF Sweep)"},
        {MixerFxType::DELAY,      "🔵 Delay (3/4 Beat Echo + Haas Stereo)"},
        {MixerFxType::REVERB,     "🟢 Reverb (3D Hall Plate + Modulated Tail)"},
        {MixerFxType::FLANGER,    "🟡 Flanger (Stereo Jet Sweep)"},
        {MixerFxType::BEATMASHER, "🟣 Beatmasher 2 (1/2..1/32 Rhythmic Stutter)"},
        {MixerFxType::TURNTABLE,  "🟠 Turntable FX (Vinyl Brake & Spinback)"},
        {MixerFxType::RAMP_DELAY, "🚀 Ramp Delay (Accelerating Siren Riser)"},
        {MixerFxType::MULHOLLAND, "🔥 Mulholland Drive (Overdrive & Feedback Growl)"},
        {MixerFxType::TRANSPOSE,  "🎚️ Transpose Stretch (Octave Pitch Shift)"}
    }};

    const int cur_assigned = deck_->slotFxType(slot);

    for (const auto &opt : options) {
        const int fx_val = static_cast<int>(opt.type);
        auto *action = menu.addAction(QString::fromUtf8(opt.label));
        action->setCheckable(true);
        action->setChecked(cur_assigned == fx_val);
        connect(action, &QAction::triggered, this, [this, slot, fx_val]() {
            if (deck_) {
                deck_->setSlotFxType(slot, fx_val);
                deck_->selectSlot(slot);
                refresh();
            }
        });
    }

    menu.exec(pos);
}
