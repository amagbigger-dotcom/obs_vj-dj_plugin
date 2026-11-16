#include "mixer-dock.hpp"
#include "deck-engine.hpp"
#include "midi-handler.hpp"
#include "tracklist-dock.hpp"

#include <obs-frontend-api.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTimer>
#include <QSignalBlocker>
#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>

namespace {

constexpr const char *MIXER_STYLESHEET = R"(
QDialog {
    background-color: #11141a;
    color: #c8d0de;
    font-family: 'Segoe UI', Arial, sans-serif;
}
QGroupBox {
    background-color: #161b22;
    border: 1px solid #252d3a;
    border-radius: 5px;
    margin-top: 12px;
    padding: 6px;
    font-weight: bold;
    font-size: 11px;
    color: #8da0b8;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top center;
    padding: 0 6px;
}
QPushButton {
    background-color: #1f2733;
    color: #e0e6f0;
    border: 1px solid #334052;
    border-radius: 3px;
    padding: 4px 8px;
    font-weight: bold;
    font-size: 10px;
}
QPushButton:hover {
    background-color: #2b3646;
    border-color: #44566d;
    color: #ffffff;
}
QPushButton:pressed {
    background-color: #0088aa;
    color: #ffffff;
}
QPushButton:checked {
    background-color: #0088cc;
    color: #ffffff;
    border-color: #00d2ff;
}
QSlider::groove:horizontal {
    height: 7px;
    background: #0d1015;
    border: 1px solid #202733;
    border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: #1c232e;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #cdd5e2;
    width: 16px;
    margin: -5px 0;
    border-radius: 3px;
    border: 1px solid #6c798e;
}
QSlider::handle:horizontal:hover {
    background: #ffffff;
}
QSlider::groove:vertical {
    width: 8px;
    background: #0d1015;
    border: 1px solid #202733;
    border-radius: 4px;
}
QSlider::sub-page:vertical {
    background: #1c232e;
    border-radius: 4px;
}
QSlider::handle:vertical {
    background: #cdd5e2;
    height: 18px;
    width: 22px;
    margin: 0 -7px;
    border-radius: 3px;
    border: 1px solid #6c798e;
}
QSlider::handle:vertical:hover {
    background: #ffffff;
}
QComboBox {
    background-color: #181f2a;
    color: #e0e6f0;
    border: 1px solid #2d3849;
    border-radius: 3px;
    padding: 2px 6px;
    font-size: 10px;
}
QComboBox QAbstractItemView {
    background-color: #181f2a;
    color: #e0e6f0;
    selection-background-color: #0088aa;
}
QProgressBar {
    background-color: #0d1015;
    border: 1px solid #1e2530;
    border-radius: 2px;
}
QProgressBar::chunk {
    background-color: qlineargradient(x1:0, y1:1, x2:0, y2:0,
        stop:0.0 #00d2ff, stop:0.7 #00ff88, stop:0.9 #ffcc00, stop:1.0 #ff3300);
    border-radius: 1px;
}
)";

static MixerWindow *g_mixer_window = nullptr;

} // namespace

void vdj_open_mixer_window()
{
    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    if (!g_mixer_window) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_mixer_window = new MixerWindow(decks, main_window);
    }
    g_mixer_window->show();
    g_mixer_window->raise();
    g_mixer_window->activateWindow();
}

void vdj_free_mixer_window()
{
    if (g_mixer_window) {
        g_mixer_window->close();
        delete g_mixer_window;
        g_mixer_window = nullptr;
    }
}

MixerWindow::~MixerWindow()
{
    if (refresh_timer_) {
        refresh_timer_->stop();
        delete refresh_timer_;
        refresh_timer_ = nullptr;
    }
    decks_ = nullptr;
    if (g_mixer_window == this) {
        g_mixer_window = nullptr;
    }
}

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("OBS VJ Mixer"));
    // Fix kompakt méret a monitor alsó egyharmadához igazítva (1/3 Screen Height, 590x340 px)
    setFixedSize(590, 340);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setStyleSheet(QString::fromUtf8(MIXER_STYLESHEET));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 3, 4, 3);
    main_layout->setSpacing(3);
    main_layout->setSizeConstraint(QLayout::SetNoConstraint);

    // 1. 3-Column DJ Mixer Area: Channel A | Center Master | Channel B
    auto *mixer_row = new QHBoxLayout();
    mixer_row->setContentsMargins(0, 0, 0, 0);
    mixer_row->setSpacing(4);

    QWidget *strip_a = buildChannelStrip(true);
    QWidget *center_master = buildCenterMasterPanel();
    QWidget *strip_b = buildChannelStrip(false);

    mixer_row->addWidget(strip_a, 4);
    mixer_row->addWidget(center_master, 5);
    mixer_row->addWidget(strip_b, 4);

    main_layout->addLayout(mixer_row, 1);

    // 2. Bottom Bar: MIDI Status, VJ Visualizer FX and Master BPM
    auto *bottom_row = new QHBoxLayout();
    bottom_row->setContentsMargins(4, 0, 4, 0);
    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Initializing..."), this);
    midi_status_lbl_->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #7f8fa4;"));
    bottom_row->addWidget(midi_status_lbl_);
    bottom_row->addStretch(1);

    auto *vj_fx_btn = new QPushButton(QStringLiteral("⚡ VJ SHADER FX"), this);
    vj_fx_btn->setFixedHeight(20);
    vj_fx_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #182230; border: 1px solid #2d3e56; color: #00d2ff; font-weight: bold; font-size: 9px; padding: 1px 8px; border-radius: 3px; } "
        "QPushButton:hover { background: #0088cc; color: #ffffff; border-color: #00d2ff; }"));
    vj_fx_btn->setToolTip(QStringLiteral("VJ Visualizer Shader kiválasztása és zenei lüktetés/tónuskövetés beállításai"));
    connect(vj_fx_btn, &QPushButton::clicked, this, []() {
        vdj_open_vj_fx_dialog();
    });
    bottom_row->addWidget(vj_fx_btn);
    bottom_row->addStretch(1);

    bottom_bpm_lbl_ = new QLabel(QStringLiteral("MASTER 128.00 BPM"), this);
    bottom_bpm_lbl_->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace; color: #ffaa00; font-weight: bold;"));
    bottom_row->addWidget(bottom_bpm_lbl_);

    main_layout->addLayout(bottom_row);

    for (auto *btn : findChildren<QPushButton *>()) {
        btn->setAutoDefault(false);
        btn->setDefault(false);
    }

    // 50ms GUI update loop
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, &MixerWindow::refreshMixer);
    refresh_timer_->start(50);
}

QWidget *MixerWindow::buildChannelStrip(bool is_deck_a)
{
    const QString title = is_deck_a ? QStringLiteral("◄ DECK A") : QStringLiteral("DECK B ►");
    auto *panel = new QGroupBox(title, this);
    if (is_deck_a) {
        panel->setStyleSheet(QStringLiteral(
            "QGroupBox { background: #0c1522; border: 1px solid #143554; border-radius: 4px; padding: 4px; } "
            "QGroupBox::title { color: #00d2ff; font-weight: bold; font-size: 10px; }"));
    } else {
        panel->setStyleSheet(QStringLiteral(
            "QGroupBox { background: #201208; border: 1px solid #542d14; border-radius: 4px; padding: 4px; } "
            "QGroupBox::title { color: #ff7700; font-weight: bold; font-size: 10px; }"));
    }

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(3);

    DeckPlayer *deck = is_deck_a ? vj_deck_a() : vj_deck_b();

    auto addSliderRow = [panel, layout, is_deck_a](const QString &name, const QString &color,
                                                   DJSlider **out_sl, QLabel **out_val_lbl,
                                                   int def_val, auto setter, auto formatter) {
        auto *row = new QHBoxLayout();
        row->setSpacing(4);
        row->setContentsMargins(0, 1, 0, 1);

        auto *lbl = new QLabel(name, panel);
        lbl->setFixedWidth(26);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: %1;").arg(color));

        auto *sl = new DJSlider(Qt::Horizontal, def_val, panel);
        sl->setRange(-100, 100);
        sl->setValue(def_val);
        sl->setFixedHeight(18);
        if (is_deck_a) {
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

        auto *val_lbl = new QLabel(formatter(def_val), panel);
        val_lbl->setFixedWidth(42);
        val_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        val_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-family: monospace; color: %1;")
            .arg(is_deck_a ? QStringLiteral("#7eb5e6") : QStringLiteral("#e6a87e")));
        *out_val_lbl = val_lbl;

        row->addWidget(lbl);
        row->addWidget(sl, 1);
        row->addWidget(val_lbl);
        layout->addLayout(row);

        QObject::connect(sl, &QSlider::valueChanged, sl, [setter, val_lbl, formatter](int val) {
            setter(1.0f + static_cast<float>(val) / 100.0f);
            val_lbl->setText(formatter(val));
        });
    };

    auto gain_fmt = [](int v) {
        if (v == 0) return QStringLiteral("0.0 dB");
        const float lin = 1.0f + static_cast<float>(v) / 100.0f;
        const float db = 20.0f * std::log10(std::max(0.01f, lin));
        return QString::asprintf("%+.1f dB", db);
    };
    auto rgb_fmt = [](int v) {
        if (v == 0) return QStringLiteral("1.00");
        return QString::asprintf("%.2f", 1.0f + static_cast<float>(v) / 100.0f);
    };
    auto flt_fmt = [](int v) {
        if (v == 0) return QStringLiteral("FLAT");
        if (v < 0) return QString::asprintf("LPF %d%%", -v);
        return QString::asprintf("HPF %d%%", v);
    };

    // 1. GAIN, HI (Red), MID (Green), LOW (Blue), FLT (Saturation)
    if (is_deck_a) {
        addSliderRow(QStringLiteral("GAIN"), QStringLiteral("#8da0b8"), &gain_a_, &gain_val_a_, 0,
                     [deck](float v) { if (deck) deck->setGain(v); }, gain_fmt);
        addSliderRow(QStringLiteral("HI"), QStringLiteral("#ff4444"), &hi_a_, &hi_val_a_, 0,
                     [deck](float v) { if (deck) { deck->setEqHi(v); deck->setRed(v); } }, rgb_fmt);
        addSliderRow(QStringLiteral("MID"), QStringLiteral("#00ff88"), &mid_a_, &mid_val_a_, 0,
                     [deck](float v) { if (deck) { deck->setEqMid(v); deck->setGreen(v); } }, rgb_fmt);
        addSliderRow(QStringLiteral("LOW"), QStringLiteral("#4488ff"), &low_a_, &low_val_a_, 0,
                     [deck](float v) { if (deck) { deck->setEqLow(v); deck->setBlue(v); } }, rgb_fmt);
        addSliderRow(QStringLiteral("FLT"), QStringLiteral("#ffaa00"), &flt_a_, &flt_val_a_, 0,
                     [deck](float v) { if (deck) { deck->setFilterVal(v - 1.0f); deck->setSaturation(v); } }, flt_fmt);
    } else {
        addSliderRow(QStringLiteral("GAIN"), QStringLiteral("#8da0b8"), &gain_b_, &gain_val_b_, 0,
                     [deck](float v) { if (deck) deck->setGain(v); }, gain_fmt);
        addSliderRow(QStringLiteral("HI"), QStringLiteral("#ff4444"), &hi_b_, &hi_val_b_, 0,
                     [deck](float v) { if (deck) { deck->setEqHi(v); deck->setRed(v); } }, rgb_fmt);
        addSliderRow(QStringLiteral("MID"), QStringLiteral("#00ff88"), &mid_b_, &mid_val_b_, 0,
                     [deck](float v) { if (deck) { deck->setEqMid(v); deck->setGreen(v); } }, rgb_fmt);
        addSliderRow(QStringLiteral("LOW"), QStringLiteral("#4488ff"), &low_b_, &low_val_b_, 0,
                     [deck](float v) { if (deck) { deck->setEqLow(v); deck->setBlue(v); } }, rgb_fmt);
        addSliderRow(QStringLiteral("FLT"), QStringLiteral("#ffaa00"), &flt_b_, &flt_val_b_, 0,
                     [deck](float v) { if (deck) { deck->setFilterVal(v - 1.0f); deck->setSaturation(v); } }, flt_fmt);
    }

    // 2. Buttons Row: Headphone CUE & Visual EQ Toggle
    auto *btn_row = new QHBoxLayout();
    btn_row->setContentsMargins(0, 2, 0, 2);
    btn_row->setSpacing(4);

    auto *cue_btn = new QPushButton(is_deck_a ? QStringLiteral("🎧 CUE A") : QStringLiteral("🎧 CUE B"), panel);
    cue_btn->setCheckable(true);
    cue_btn->setFixedHeight(22);
    cue_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222c; border: 1px solid #2d3b4d; color: %1; font-weight: bold; font-size: 8px; border-radius: 3px; } "
        "QPushButton:checked { background: %2; color: #fff; border-color: %1; }")
        .arg(is_deck_a ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700"))
        .arg(is_deck_a ? QStringLiteral("#0088cc") : QStringLiteral("#cc5500")));
    btn_row->addWidget(cue_btn, 1);

    auto *viz_eq_btn = new QPushButton(QStringLiteral("🎨 VIZ EQ"), panel);
    viz_eq_btn->setCheckable(true);
    viz_eq_btn->setChecked(deck ? deck->isVisualEqEnabled() : true);
    viz_eq_btn->setFixedHeight(22);
    viz_eq_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #161b24; border: 1px solid #2b3545; color: #7f8fa4; font-weight: bold; font-size: 8px; border-radius: 3px; } "
        "QPushButton:checked { background: #133824; color: #00ff88; border-color: #00bb55; }"));
    viz_eq_btn->setToolTip(QStringLiteral("Vizuális EQ: Bekapcsolva a HI/MID/LOW/FLT potik színezik a videót. Kikapcsolva csak a hangot szűrik."));
    connect(viz_eq_btn, &QPushButton::toggled, this, [deck](bool chk) {
        if (deck) deck->setVisualEqEnabled(chk);
    });
    btn_row->addWidget(viz_eq_btn, 1);

    layout->addLayout(btn_row);

    if (is_deck_a) {
        hp_cue_a_btn_ = cue_btn;
        connect(cue_btn, &QPushButton::toggled, this, [deck](bool chk) { if (deck) deck->setHeadphoneCue(chk); });
    } else {
        hp_cue_b_btn_ = cue_btn;
        connect(cue_btn, &QPushButton::toggled, this, [deck](bool chk) { if (deck) deck->setHeadphoneCue(chk); });
    }

    // 3. Vertical Volume Fader and Peak Meter Row
    auto *fader_meter_row = new QHBoxLayout();
    fader_meter_row->setContentsMargins(0, 3, 0, 0);
    fader_meter_row->setSpacing(6);

    auto *vol_slider = new DJSlider(Qt::Vertical, 80, panel);
    vol_slider->setRange(0, 100);
    vol_slider->setValue(80);
    vol_slider->setFixedHeight(62);
    vol_slider->setToolTip(QStringLiteral("Channel Volume Fader (Default: 80% Unity Line Level)"));
    if (is_deck_a) {
        vol_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:vertical { width: 6px; background: #08101a; border: 1px solid #152c42; border-radius: 3px; } "
            "QSlider::handle:vertical { background: #00d2ff; height: 16px; margin: 0 -5px; border-radius: 3px; border: 1px solid #ffffff; } "
            "QSlider::handle:vertical:hover { background: #38e0ff; }"));
    } else {
        vol_slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:vertical { width: 6px; background: #1a0c05; border: 1px solid #42220f; border-radius: 3px; } "
            "QSlider::handle:vertical { background: #ff7700; height: 16px; margin: 0 -5px; border-radius: 3px; border: 1px solid #ffffff; } "
            "QSlider::handle:vertical:hover { background: #ffa038; }"));
    }

    auto *meter = new QProgressBar(panel);
    meter->setOrientation(Qt::Vertical);
    meter->setRange(0, 100);
    meter->setValue(0);
    meter->setFixedWidth(7);
    meter->setFixedHeight(62);
    meter->setTextVisible(false);
    if (is_deck_a) {
        meter->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #08101a; border: 1px solid #152c42; border-radius: 2px; } "
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:1, x2:0, y2:0, stop:0 #005588, stop:0.8 #00d2ff, stop:1 #ffffff); border-radius: 1px; }"));
    } else {
        meter->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #1a0c05; border: 1px solid #42220f; border-radius: 2px; } "
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:1, x2:0, y2:0, stop:0 #883300, stop:0.8 #ff7700, stop:1 #ffffff); border-radius: 1px; }"));
    }

    auto *vol_lbl = new QLabel(QStringLiteral("80%"), panel);
    vol_lbl->setFixedWidth(34);
    vol_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    vol_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: %1;")
        .arg(is_deck_a ? QStringLiteral("#7eb5e6") : QStringLiteral("#e6a87e")));

    if (is_deck_a) {
        vol_a_ = vol_slider;
        meter_a_ = meter;
        vol_val_a_ = vol_lbl;
        connect(vol_slider, &QSlider::valueChanged, this, [this, deck](int val) {
            if (deck) deck->setVolume(static_cast<float>(val) / 100.0f);
            if (vol_val_a_) vol_val_a_->setText(QString::asprintf("%d%%", val));
        });
    } else {
        vol_b_ = vol_slider;
        meter_b_ = meter;
        vol_val_b_ = vol_lbl;
        connect(vol_slider, &QSlider::valueChanged, this, [this, deck](int val) {
            if (deck) deck->setVolume(static_cast<float>(val) / 100.0f);
            if (vol_val_b_) vol_val_b_->setText(QString::asprintf("%d%%", val));
        });
    }

    fader_meter_row->addStretch(1);
    fader_meter_row->addWidget(vol_slider);
    fader_meter_row->addWidget(meter);
    fader_meter_row->addWidget(vol_lbl);
    fader_meter_row->addStretch(1);

    layout->addLayout(fader_meter_row);

    // 4. Volume FX Dropdown
    auto *vol_fx_combo = new QComboBox(panel);
    vol_fx_combo->setFixedHeight(20);
    if (is_deck_a) {
        vol_fx_combo->setStyleSheet(QStringLiteral(
            "QComboBox { background: #09131e; color: #7eb5e6; border: 1px solid #173450; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 3px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: #0c1826; color: #fff; selection-background-color: #005588; }"));
    } else {
        vol_fx_combo->setStyleSheet(QStringLiteral(
            "QComboBox { background: #1e0f07; color: #e6a87e; border: 1px solid #502b13; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 3px; } "
            "QComboBox::drop-down { border: none; } "
            "QComboBox QAbstractItemView { background: #26150a; color: #fff; selection-background-color: #884400; }"));
    }
    vol_fx_combo->addItem(QStringLiteral("VOL FX: Ki"), 0);
    vol_fx_combo->addItem(QStringLiteral("VOL FX: Sötétedés"), 1);
    vol_fx_combo->addItem(QStringLiteral("VOL FX: Kiégés"), 2);
    vol_fx_combo->addItem(QStringLiteral("VOL FX: Homályosítás"), 3);
    vol_fx_combo->setCurrentIndex(deck ? deck->volumeFxMode() : 0);
    connect(vol_fx_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [deck](int idx) {
        if (deck) deck->setVolumeFxMode(idx);
    });
    layout->addWidget(vol_fx_combo);

    return panel;
}

QWidget *MixerWindow::buildCenterMasterPanel()
{
    auto *panel = new QGroupBox(QStringLiteral("MASTER & CROSSFADER"), this);
    panel->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #13171e; border: 1px solid #222936; border-radius: 4px; padding: 4px; } "
        "QGroupBox::title { color: #8da0b8; font-weight: bold; font-size: 10px; }"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    // 0. Traktor Master Clock & Tempo Section
    auto *clock_box = new QFrame(panel);
    clock_box->setStyleSheet(QStringLiteral("background: #0f1319; border: 1px solid #1f2733; border-radius: 3px; padding: 2px;"));
    auto *clock_vbox = new QVBoxLayout(clock_box);
    clock_vbox->setContentsMargins(2, 2, 2, 2);
    clock_vbox->setSpacing(2);

    auto *clock_row1 = new QHBoxLayout();
    clock_row1->setSpacing(3);

    master_clock_btn_ = new QPushButton(QStringLiteral("MASTER"), clock_box);
    master_clock_btn_->setCheckable(true);
    master_clock_btn_->setFixedHeight(20);
    master_clock_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 8px; padding: 1px 3px; } "
        "QPushButton:checked { background: #d97706; border: 1px solid #f59e0b; color: #fff; }"));
    master_clock_btn_->setToolTip(QStringLiteral("Enable Central Master Clock mode"));
    clock_row1->addWidget(master_clock_btn_);

    master_clock_ext_btn_ = new QPushButton(QStringLiteral("EXT SYNC"), clock_box);
    master_clock_ext_btn_->setCheckable(true);
    master_clock_ext_btn_->setFixedHeight(20);
    master_clock_ext_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 8px; padding: 1px 3px; } "
        "QPushButton:checked { background: #0284c7; border: 1px solid #38bdf8; color: #fff; }"));
    master_clock_ext_btn_->setToolTip(QStringLiteral("Sync Master Clock to incoming external MIDI Clock"));
    clock_row1->addWidget(master_clock_ext_btn_);

    master_clock_send_btn_ = new QPushButton(QStringLiteral("SEND CLOCK"), clock_box);
    master_clock_send_btn_->setCheckable(true);
    master_clock_send_btn_->setFixedHeight(20);
    master_clock_send_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 8px; padding: 1px 3px; } "
        "QPushButton:checked { background: #2e7d32; border: 1px solid #4caf50; color: #fff; }"));
    master_clock_send_btn_->setToolTip(QStringLiteral("Transmit MIDI Clock (24 PPQN) to external MIDI hardware / software"));
    clock_row1->addWidget(master_clock_send_btn_);

    clock_vbox->addLayout(clock_row1);

    auto *clock_row2 = new QHBoxLayout();
    clock_row2->setSpacing(4);

    master_clock_minus_btn_ = new QPushButton(QStringLiteral("−"), clock_box);
    master_clock_minus_btn_->setFixedSize(22, 22);
    master_clock_minus_btn_->setStyleSheet(QStringLiteral("background: #232b36; border: 1px solid #334050; color: #e0e6f0; font-size: 11px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_minus_btn_);

    master_clock_bpm_edit_ = new QLineEdit(QStringLiteral("128.00"), clock_box);
    master_clock_bpm_edit_->setAlignment(Qt::AlignCenter);
    master_clock_bpm_edit_->setFixedHeight(22);
    master_clock_bpm_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #141a22; color: #ffaa00; border: 1px solid #2d3b4d; border-radius: 3px; font-size: 11px; font-family: monospace; font-weight: bold; padding: 1px; } "
        "QLineEdit:focus { border: 1px solid #ffaa00; background: #1a222d; }"));
    master_clock_bpm_edit_->setToolTip(QStringLiteral("Master Clock BPM. Írd be és nyomj Entert!"));
    clock_row2->addWidget(master_clock_bpm_edit_, 1);

    auto commit_master_bpm = [this]() {
        if (!decks_ || !master_clock_bpm_edit_) return;
        bool ok = false;
        const double val = master_clock_bpm_edit_->text().toDouble(&ok);
        if (ok && val >= 30.0 && val <= 300.0) {
            decks_->setMasterClockBpm(val);
            decks_->updateSync();
            if (master_clock_btn_) master_clock_btn_->setChecked(true);
        }
    };

    connect(master_clock_bpm_edit_, &QLineEdit::returnPressed, this, [this, commit_master_bpm]() {
        commit_master_bpm();
        master_clock_bpm_edit_->clearFocus();
    });
    connect(master_clock_bpm_edit_, &QLineEdit::editingFinished, this, commit_master_bpm);

    master_clock_plus_btn_ = new QPushButton(QStringLiteral("+"), clock_box);
    master_clock_plus_btn_->setFixedSize(22, 22);
    master_clock_plus_btn_->setStyleSheet(QStringLiteral("background: #232b36; border: 1px solid #334050; color: #e0e6f0; font-size: 11px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_plus_btn_);

    master_clock_tap_btn_ = new QPushButton(QStringLiteral("TAP"), clock_box);
    master_clock_tap_btn_->setFixedSize(32, 22);
    master_clock_tap_btn_->setStyleSheet(QStringLiteral("background: #1c2634; border: 1px solid #385070; color: #00d2ff; font-size: 9px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_tap_btn_);

    clock_vbox->addLayout(clock_row2);
    layout->addWidget(clock_box);

    connect(master_clock_btn_, &QPushButton::clicked, this, [this]() {
        if (!decks_) return;
        if (decks_->masterMode() != DeckPair::MASTER_CLOCK) {
            const double cur_tempo = decks_->effectiveMasterBpm();
            if (cur_tempo > 30.0) {
                decks_->setMasterClockBpm(cur_tempo);
            } else {
                decks_->setMasterMode(DeckPair::MASTER_CLOCK);
            }
            master_clock_btn_->setChecked(true);
        } else {
            decks_->setMasterMode(DeckPair::MASTER_CLOCK);
        }
        decks_->updateSync();
    });
    connect(master_clock_ext_btn_, &QPushButton::clicked, this, [this]() {
        if (!decks_) return;
        if (decks_->masterMode() == DeckPair::MASTER_EXT_MIDI) {
            decks_->setMasterMode(DeckPair::MASTER_CLOCK);
        } else {
            decks_->setMasterMode(DeckPair::MASTER_EXT_MIDI);
        }
        decks_->updateSync();
    });
    connect(master_clock_send_btn_, &QPushButton::clicked, this, [this]() {
        const bool send = !vdj_midi_is_clock_send();
        vdj_midi_set_clock_send(send);
        master_clock_send_btn_->setChecked(send);
    });
    connect(master_clock_minus_btn_, &QPushButton::clicked, this, [this]() {
        if (decks_) {
            decks_->setMasterClockBpm(decks_->masterClockBpm() - 0.1);
            decks_->updateSync();
            if (master_clock_btn_) master_clock_btn_->setChecked(true);
        }
    });
    connect(master_clock_plus_btn_, &QPushButton::clicked, this, [this]() {
        if (decks_) {
            decks_->setMasterClockBpm(decks_->masterClockBpm() + 0.1);
            decks_->updateSync();
            if (master_clock_btn_) master_clock_btn_->setChecked(true);
        }
    });
    connect(master_clock_tap_btn_, &QPushButton::clicked, this, [this]() {
        const auto now = std::chrono::steady_clock::now();
        if (!master_tap_times_.empty()) {
            const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - master_tap_times_.back()).count();
            if (diff > 2500)
                master_tap_times_.clear();
        }
        master_tap_times_.push_back(now);
        while (master_tap_times_.size() > 8)
            master_tap_times_.erase(master_tap_times_.begin());
        if (master_tap_times_.size() >= 3 && decks_) {
            const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(master_tap_times_.back() - master_tap_times_.front()).count();
            const double avg_ms = static_cast<double>(total_ms) / static_cast<double>(master_tap_times_.size() - 1);
            if (avg_ms > 150.0) {
                const double tap_bpm = 60000.0 / avg_ms;
                decks_->setMasterClockBpm(tap_bpm);
                decks_->updateSync();
                if (master_clock_btn_) master_clock_btn_->setChecked(true);
            }
        }
    });

    // 1. Headphone Monitoring Section
    auto *hp_box = new QFrame(panel);
    hp_box->setStyleSheet(QStringLiteral("background: #0f1218; border: 1px solid #1c232f; border-radius: 3px; padding: 4px;"));
    auto *hp_vbox = new QVBoxLayout(hp_box);
    hp_vbox->setContentsMargins(4, 2, 4, 2);
    hp_vbox->setSpacing(3);

    auto *hp_hdr_lbl = new QLabel(QStringLiteral("HEADPHONE MONITOR"), hp_box);
    hp_hdr_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #ffaa00;"));
    hp_vbox->addWidget(hp_hdr_lbl);

    auto *hp_sl_row = new QHBoxLayout();
    hp_sl_row->setSpacing(6);

    auto *hp_vol_vbox = new QVBoxLayout();
    auto *hp_vol_hdr = new QHBoxLayout();
    auto *hp_vol_lbl = new QLabel(QStringLiteral("HP VOL"), hp_box);
    hp_vol_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #8da0b8;"));
    hp_vol_val_lbl_ = new QLabel(QStringLiteral("100%"), hp_box);
    hp_vol_val_lbl_->setFixedWidth(34);
    hp_vol_val_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hp_vol_val_lbl_->setStyleSheet(QStringLiteral("font-size: 8px; font-family: monospace; color: #ffaa00;"));
    hp_vol_hdr->addWidget(hp_vol_lbl);
    hp_vol_hdr->addStretch();
    hp_vol_hdr->addWidget(hp_vol_val_lbl_);

    hp_vol_slider_ = new DJSlider(Qt::Horizontal, 100, hp_box);
    hp_vol_slider_->setRange(0, 100);
    hp_vol_slider_->setValue(100);
    hp_vol_vbox->addLayout(hp_vol_hdr);
    hp_vol_vbox->addWidget(hp_vol_slider_);

    auto *hp_mix_vbox = new QVBoxLayout();
    auto *hp_mix_hdr = new QHBoxLayout();
    auto *hp_mix_lbl = new QLabel(QStringLiteral("CUE / PGM"), hp_box);
    hp_mix_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #8da0b8;"));
    hp_mix_val_lbl_ = new QLabel(QStringLiteral("50/50"), hp_box);
    hp_mix_val_lbl_->setFixedWidth(38);
    hp_mix_val_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hp_mix_val_lbl_->setStyleSheet(QStringLiteral("font-size: 8px; font-family: monospace; color: #ffaa00;"));
    hp_mix_hdr->addWidget(hp_mix_lbl);
    hp_mix_hdr->addStretch();
    hp_mix_hdr->addWidget(hp_mix_val_lbl_);

    hp_mix_slider_ = new DJSlider(Qt::Horizontal, 50, hp_box);
    hp_mix_slider_->setRange(0, 100);
    hp_mix_slider_->setValue(50);
    hp_mix_vbox->addLayout(hp_mix_hdr);
    hp_mix_vbox->addWidget(hp_mix_slider_);

    hp_sl_row->addLayout(hp_vol_vbox);
    hp_sl_row->addLayout(hp_mix_vbox);
    hp_vbox->addLayout(hp_sl_row);

    layout->addWidget(hp_box);

    connect(hp_vol_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_) decks_->setHeadphoneVolume(static_cast<float>(v) / 100.0f);
        if (hp_vol_val_lbl_) hp_vol_val_lbl_->setText(QString::asprintf("%d%%", v));
    });
    connect(hp_mix_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_) decks_->setHeadphoneMix(static_cast<float>(v) / 100.0f);
        if (hp_mix_val_lbl_) hp_mix_val_lbl_->setText(QString::asprintf("%d/%d", 100 - v, v));
    });

    layout->addStretch(1);

    // 3. Center Master Crossfader
    auto *cf_box = new QVBoxLayout();
    cf_box->setContentsMargins(0, 2, 0, 2);

    auto *cf_hdr = new QHBoxLayout();
    auto *cf_a = new QLabel(QStringLiteral("◄ A"), panel);
    cf_a->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px;"));
    auto *cf_title = new QLabel(QStringLiteral("CROSSFADER"), panel);
    cf_title->setStyleSheet(QStringLiteral("color: #788599; font-weight: bold; font-size: 9px;"));
    auto *cf_b = new QLabel(QStringLiteral("B ►"), panel);
    cf_b->setStyleSheet(QStringLiteral("color: #ff7700; font-weight: bold; font-size: 11px;"));

    cf_hdr->addWidget(cf_a);
    cf_hdr->addStretch();
    cf_hdr->addWidget(cf_title);
    cf_hdr->addStretch();
    cf_hdr->addWidget(cf_b);
    cf_box->addLayout(cf_hdr);

    cf_val_lbl_ = new QLabel(QStringLiteral("◄ 50% | 50% ►"), panel);
    cf_val_lbl_->setFixedWidth(130);
    cf_val_lbl_->setAlignment(Qt::AlignCenter);
    cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #8da0b8; font-weight: bold;"));

    auto *cf_val_row = new QHBoxLayout();
    cf_val_row->addStretch(1);
    cf_val_row->addWidget(cf_val_lbl_);
    cf_val_row->addStretch(1);
    cf_box->addLayout(cf_val_row);

    // Crossfader FX Dropdown & Canvas Scale Mode (FIT vs FILL)
    auto *fx_scale_row = new QHBoxLayout();
    fx_scale_row->setContentsMargins(0, 0, 0, 0);
    fx_scale_row->setSpacing(4);

    auto *cf_fx_combo = new QComboBox(panel);
    cf_fx_combo->setFixedHeight(20);
    cf_fx_combo->setStyleSheet(QStringLiteral(
        "QComboBox { background: #12161f; color: #8da0b8; border: 1px solid #242d3d; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #181d26; color: #fff; selection-background-color: #005588; }"));
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Ki (Normál)"), 0);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Spirális Örvény"), 1);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Halszem Optika"), 2);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: RGB Split"), 3);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Radial Zoom Blur"), 4);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Pixelesedés (Mosaic)"), 5);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Luma Dissolve"), 6);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Folyadék Hullám"), 7);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Kaleidoszkóp"), 8);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Glitch Displacement"), 9);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: CRT / VHS Scanline"), 10);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Pop-Art Halftone"), 11);
    cf_fx_combo->addItem(QStringLiteral("XFADE FX: Neon Kontúr"), 12);
    cf_fx_combo->setCurrentIndex(decks_ ? decks_->crossfadeFxMode() : 0);
    connect(cf_fx_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (decks_) decks_->setCrossfadeFxMode(idx);
    });
    fx_scale_row->addWidget(cf_fx_combo, 1);

    auto *scale_btn = new QPushButton(decks_ && decks_->videoScaleMode() == 0 ? QStringLiteral("📺 FIT") : QStringLiteral("📺 FILL"), panel);
    scale_btn->setFixedHeight(20);
    scale_btn->setToolTip(QStringLiteral("OBS Vászon Képméretezés:\nFILL = Teljes vászonkitöltés fekete sávok nélkül\nFIT = Aránytartó teljes kép"));
    scale_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #182230; border: 1px solid #2d3e56; color: #00d2ff; font-size: 8px; font-weight: bold; padding: 1px 6px; border-radius: 3px; } "
        "QPushButton:hover { background: #223247; border-color: #00d2ff; }"));
    connect(scale_btn, &QPushButton::clicked, this, [scale_btn, this]() {
        if (!decks_) return;
        const int cur = decks_->videoScaleMode();
        const int next = (cur == 0) ? 1 : 0;
        decks_->setVideoScaleMode(next);
        scale_btn->setText(next == 0 ? QStringLiteral("📺 FIT") : QStringLiteral("📺 FILL"));
    });
    fx_scale_row->addWidget(scale_btn);

    cf_box->addLayout(fx_scale_row);

    crossfader_ = new DJSlider(Qt::Horizontal, 500, panel);
    crossfader_->setRange(0, 1000);
    crossfader_->setValue(500);
    crossfader_->setFixedHeight(28);
    crossfader_->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 10px; background: #0c0f14; border: 1px solid #232b38; border-radius: 5px; } "
        "QSlider::handle:horizontal { background: #ffffff; width: 26px; margin: -6px 0; border-radius: 3px; border: 2px solid #0088aa; }"));
    crossfader_->setToolTip(QStringLiteral("Master Crossfader (Double-click resets to 50/50 center)"));
    cf_box->addWidget(crossfader_);

    auto updateCfLabel = [this](int val) {
        if (!cf_val_lbl_) return;
        const int pct_b = val / 10;
        const int pct_a = 100 - pct_b;
        if (val == 500) {
            cf_val_lbl_->setText(QStringLiteral("◄ 50% | 50% ►"));
            cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #8da0b8; font-weight: bold;"));
        } else if (val < 500) {
            cf_val_lbl_->setText(QString::asprintf("◄ A %d%% | %d%%", pct_a, pct_b));
            cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #00d2ff; font-weight: bold;"));
        } else {
            cf_val_lbl_->setText(QString::asprintf("%d%% | B %d%% ►", pct_a, pct_b));
            cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #ff7700; font-weight: bold;"));
        }
    };

    connect(crossfader_, &QSlider::valueChanged, this, [this, updateCfLabel](int val) {
        if (decks_) {
            decks_->setCrossfader(static_cast<double>(val) / 1000.0);
        }
        updateCfLabel(val);
    });

    layout->addLayout(cf_box);

    return panel;
}

void MixerWindow::refreshMixer()
{
    if (!decks_)
        return;

    decks_->updateSync();

    if (bottom_bpm_lbl_) {
        bottom_bpm_lbl_->setText(QString::asprintf("MASTER %.2f BPM", decks_->effectiveMasterBpm()));
    }

    if (master_clock_btn_) {
        const bool is_clock_m = (decks_->masterMode() == DeckPair::MASTER_CLOCK);
        if (master_clock_btn_->isChecked() != is_clock_m) {
            QSignalBlocker blocker(master_clock_btn_);
            master_clock_btn_->setChecked(is_clock_m);
        }
    }
    if (master_clock_ext_btn_) {
        const bool is_ext_m = (decks_->masterMode() == DeckPair::MASTER_EXT_MIDI);
        if (master_clock_ext_btn_->isChecked() != is_ext_m) {
            QSignalBlocker blocker(master_clock_ext_btn_);
            master_clock_ext_btn_->setChecked(is_ext_m);
        }
    }
    if (master_clock_send_btn_) {
        const bool is_send = vdj_midi_is_clock_send();
        if (master_clock_send_btn_->isChecked() != is_send) {
            QSignalBlocker blocker(master_clock_send_btn_);
            master_clock_send_btn_->setChecked(is_send);
        }
    }
    if (master_clock_bpm_edit_ && !master_clock_bpm_edit_->hasFocus()) {
        master_clock_bpm_edit_->setText(QString::asprintf("%.2f", decks_->effectiveMasterBpm()));
    }

    if (crossfader_ && !crossfader_->isSliderDown()) {
        const int cf_val = static_cast<int>(decks_->crossfader() * 1000.0);
        if (std::abs(crossfader_->value() - cf_val) > 2) {
            QSignalBlocker blocker(crossfader_);
            crossfader_->setValue(cf_val);
        }
        if (cf_val_lbl_) {
            const int pct_b = cf_val / 10;
            const int pct_a = 100 - pct_b;
            if (cf_val == 500) {
                cf_val_lbl_->setText(QStringLiteral("◄ 50% | 50% ►"));
                cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #8da0b8; font-weight: bold;"));
            } else if (cf_val < 500) {
                cf_val_lbl_->setText(QString::asprintf("◄ A %d%% | %d%%", pct_a, pct_b));
                cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #00d2ff; font-weight: bold;"));
            } else {
                cf_val_lbl_->setText(QString::asprintf("%d%% | B %d%% ►", pct_a, pct_b));
                cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #ff7700; font-weight: bold;"));
            }
        }
    }

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

    auto gain_fmt = [](int v) {
        if (v == 0) return QStringLiteral("0.0 dB");
        const float lin = 1.0f + static_cast<float>(v) / 100.0f;
        const float db = 20.0f * std::log10(std::max(0.01f, lin));
        return QString::asprintf("%+.1f dB", db);
    };
    auto rgb_fmt = [](int v) {
        if (v == 0) return QStringLiteral("1.00");
        return QString::asprintf("%.2f", 1.0f + static_cast<float>(v) / 100.0f);
    };
    auto flt_fmt = [](int v) {
        if (v == 0) return QStringLiteral("FLAT");
        if (v < 0) return QString::asprintf("LPF %d%%", -v);
        return QString::asprintf("HPF %d%%", v);
    };

    updateSliderFromVal(gain_a_, gain_val_a_, static_cast<int>((decks_->deckA().gain() - 1.0f) * 100.0f), gain_fmt);
    updateSliderFromVal(hi_a_, hi_val_a_, static_cast<int>((decks_->deckA().eqHi() - 1.0f) * 100.0f), rgb_fmt);
    updateSliderFromVal(mid_a_, mid_val_a_, static_cast<int>((decks_->deckA().eqMid() - 1.0f) * 100.0f), rgb_fmt);
    updateSliderFromVal(low_a_, low_val_a_, static_cast<int>((decks_->deckA().eqLow() - 1.0f) * 100.0f), rgb_fmt);
    updateSliderFromVal(flt_a_, flt_val_a_, static_cast<int>(decks_->deckA().filterVal() * 100.0f), flt_fmt);

    if (vol_a_ && !vol_a_->isSliderDown()) {
        const int target_vol = static_cast<int>(decks_->deckA().volume() * 100.0f);
        if (std::abs(vol_a_->value() - target_vol) > 1) {
            QSignalBlocker blocker(vol_a_);
            vol_a_->setValue(target_vol);
        }
    }
    if (vol_val_a_ && vol_a_) {
        vol_val_a_->setText(QString::asprintf("%d%%", vol_a_->value()));
    }

    updateSliderFromVal(gain_b_, gain_val_b_, static_cast<int>((decks_->deckB().gain() - 1.0f) * 100.0f), gain_fmt);
    updateSliderFromVal(hi_b_, hi_val_b_, static_cast<int>((decks_->deckB().eqHi() - 1.0f) * 100.0f), rgb_fmt);
    updateSliderFromVal(mid_b_, mid_val_b_, static_cast<int>((decks_->deckB().eqMid() - 1.0f) * 100.0f), rgb_fmt);
    updateSliderFromVal(low_b_, low_val_b_, static_cast<int>((decks_->deckB().eqLow() - 1.0f) * 100.0f), rgb_fmt);
    updateSliderFromVal(flt_b_, flt_val_b_, static_cast<int>(decks_->deckB().filterVal() * 100.0f), flt_fmt);

    if (vol_b_ && !vol_b_->isSliderDown()) {
        const int target_vol = static_cast<int>(decks_->deckB().volume() * 100.0f);
        if (std::abs(vol_b_->value() - target_vol) > 1) {
            QSignalBlocker blocker(vol_b_);
            vol_b_->setValue(target_vol);
        }
    }
    if (vol_val_b_ && vol_b_) {
        vol_val_b_->setText(QString::asprintf("%d%%", vol_b_->value()));
    }

    if (hp_cue_a_btn_ && hp_cue_a_btn_->isChecked() != decks_->deckA().isHeadphoneCue()) {
        QSignalBlocker blocker(hp_cue_a_btn_);
        hp_cue_a_btn_->setChecked(decks_->deckA().isHeadphoneCue());
    }
    if (hp_cue_b_btn_ && hp_cue_b_btn_->isChecked() != decks_->deckB().isHeadphoneCue()) {
        QSignalBlocker blocker(hp_cue_b_btn_);
        hp_cue_b_btn_->setChecked(decks_->deckB().isHeadphoneCue());
    }

    if (meter_a_) {
        const bool playing_a = decks_->deckA().isPlaying();
        const int vol_a = static_cast<int>(decks_->deckA().volume() * decks_->deckA().gain() * 75.0f);
        meter_a_->setValue(playing_a ? std::min(100, vol_a + (rand() % 10)) : 0);
    }
    if (meter_b_) {
        const bool playing_b = decks_->deckB().isPlaying();
        const int vol_b = static_cast<int>(decks_->deckB().volume() * decks_->deckB().gain() * 75.0f);
        meter_b_->setValue(playing_b ? std::min(100, vol_b + (rand() % 10)) : 0);
    }

    if (midi_status_lbl_) {
        const bool conn = vdj_midi_is_connected();
        if (conn) {
            const char *dev = vdj_midi_get_device_name();
            const QString name = (dev && *dev) ? QString::fromUtf8(dev) : QStringLiteral("MIDI DEVICE");
            midi_status_lbl_->setText(QStringLiteral("MIDI: %1 CONNECTED").arg(name));
            midi_status_lbl_->setStyleSheet(QStringLiteral("color: #00ff88; font-weight: bold; font-size: 10px;"));
        } else {
            midi_status_lbl_->setText(QStringLiteral("MIDI: Disconnected"));
            midi_status_lbl_->setStyleSheet(QStringLiteral("color: #ff5555; font-weight: bold; font-size: 10px;"));
        }
    }
}

// =============================================================================
// Mixer ablak megjelenítése: Képernyő alsó közepére igazítás (Dock to Bottom Center)
// =============================================================================
void MixerWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect geom = screen->availableGeometry();
        const int target_w = 590;
        const int target_h = 340;
        setFixedSize(target_w, target_h);

        const int y_pos = std::max(0, geom.bottom() - target_h - 4);
        const int x_pos = geom.left() + 665;
        move(x_pos, y_pos);
    }
}

void MixerWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if (master_clock_bpm_edit_ && master_clock_bpm_edit_->hasFocus()) {
            bool ok = false;
            const double val = master_clock_bpm_edit_->text().toDouble(&ok);
            if (ok && val >= 30.0 && val <= 300.0 && decks_) {
                decks_->setMasterClockBpm(val);
                decks_->updateSync();
                if (master_clock_btn_) master_clock_btn_->setChecked(true);
            }
            master_clock_bpm_edit_->clearFocus();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (master_clock_bpm_edit_ && master_clock_bpm_edit_->hasFocus()) {
            master_clock_bpm_edit_->clearFocus();
            event->accept();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

// =============================================================================
// VJ Visualizer Beállítások Ablak (VjFxDialog)
// =============================================================================
static VjFxDialog *g_vj_fx_dialog = nullptr;

VjFxDialog::VjFxDialog(DeckPair *decks, QWidget *parent)
    : QDialog(parent), decks_(decks)
{
    setWindowTitle(QStringLiteral("VJ Visualizer Beállítások"));
    setFixedSize(430, 250);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setStyleSheet(QString::fromUtf8(MIXER_STYLESHEET));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);

    auto *title_lbl = new QLabel(QStringLiteral("⚡ VJ AUDIO VISUALIZER GENERÁTOR"), this);
    title_lbl->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold; color: #00d2ff;"));
    layout->addWidget(title_lbl);

    auto *desc_lbl = new QLabel(
        QStringLiteral("Ha a Deck-be tiszta audió fájlt (.mp3, .wav) töltesz be videó nélkül,\n"
                       "a GPU valós időben ezt a procedurális vizuális effektet generálja a vászonra:"), this);
    desc_lbl->setStyleSheet(QStringLiteral("font-size: 10px; color: #8da0b8;"));
    layout->addWidget(desc_lbl);

    auto *form_box = new QFrame(this);
    form_box->setStyleSheet(QStringLiteral("background: #0f1319; border: 1px solid #1f2733; border-radius: 4px; padding: 6px;"));
    auto *form_layout = new QVBoxLayout(form_box);
    form_layout->setContentsMargins(8, 8, 8, 8);
    form_layout->setSpacing(8);

    auto *combo_row = new QHBoxLayout();
    auto *combo_lbl = new QLabel(QStringLiteral("Shader Effekt:"), form_box);
    combo_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #ffaa00;"));
    combo_row->addWidget(combo_lbl);

    mode_combo_ = new QComboBox(form_box);
    mode_combo_->setFixedHeight(24);
    mode_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #182230; color: #e0e6f0; border: 1px solid #2d3e56; font-size: 10px; font-weight: bold; padding: 2px 6px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #181d26; color: #fff; selection-background-color: #005588; }"));
    mode_combo_->addItem(QStringLiteral("1. Fluid Flow (Organikus Folyadék - Alapértelmezett)"), 0);
    mode_combo_->addItem(QStringLiteral("2. Truchet Spin (Forgó Geometrikus Labirintus)"), 1);
    mode_combo_->addItem(QStringLiteral("3. Hypno Worm (Hipnotikus Féregalagút - OpArt)"), 2);
    mode_combo_->addItem(QStringLiteral("4. Shape Twist (Geometrikus Csavarás - Triangle Twist)"), 3);
    if (decks_) {
        mode_combo_->setCurrentIndex(decks_->visualizerMode());
    }
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (decks_) decks_->setVisualizerMode(idx);
    });
    combo_row->addWidget(mode_combo_, 1);
    form_layout->addLayout(combo_row);

    pulse_chk_ = new QCheckBox(QStringLiteral("Zenei lüktetés (Kick & Bass Pulse dinamika)"), form_box);
    pulse_chk_->setStyleSheet(QStringLiteral(
        "QCheckBox { font-size: 10px; font-weight: bold; color: #c8d0de; } "
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #334052; border-radius: 2px; background: #161b22; } "
        "QCheckBox::indicator:checked { background: #0088cc; border-color: #00d2ff; }"));
    pulse_chk_->setToolTip(QStringLiteral("A lábdob és mélyhang leütésekre a shader elemei és fényessége dinamikusan lüktet."));
    pulse_chk_->setChecked(decks_ ? decks_->isVisualizerPulse() : true);
    connect(pulse_chk_, &QCheckBox::toggled, this, [this](bool chk) {
        if (decks_) decks_->setVisualizerPulse(chk);
    });
    form_layout->addWidget(pulse_chk_);

    tone_chk_ = new QCheckBox(QStringLiteral("Zenei tónuskövetés (Track Spectral Tone Tracking)"), form_box);
    tone_chk_->setStyleSheet(QStringLiteral(
        "QCheckBox { font-size: 10px; font-weight: bold; color: #c8d0de; } "
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #334052; border-radius: 2px; background: #161b22; } "
        "QCheckBox::indicator:checked { background: #0088cc; border-color: #00d2ff; }"));
    tone_chk_->setToolTip(QStringLiteral("A szám frekvenciaspektruma (mély, közép, magas) szerint finoman modulálja a shader színárnyalatait."));
    tone_chk_->setChecked(decks_ ? decks_->isVisualizerTone() : true);
    connect(tone_chk_, &QCheckBox::toggled, this, [this](bool chk) {
        if (decks_) decks_->setVisualizerTone(chk);
    });
    form_layout->addWidget(tone_chk_);

    layout->addWidget(form_box);

    auto *btn_row = new QHBoxLayout();
    btn_row->addStretch(1);
    auto *close_btn = new QPushButton(QStringLiteral("BEZÁRÁS"), this);
    close_btn->setFixedWidth(90);
    close_btn->setFixedHeight(24);
    close_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #222c3a; color: #fff; border: 1px solid #3d4f68; border-radius: 3px; font-weight: bold; font-size: 10px; } "
        "QPushButton:hover { background: #0088cc; border-color: #00d2ff; }"));
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    btn_row->addWidget(close_btn);
    layout->addLayout(btn_row);
}

VjFxDialog::~VjFxDialog()
{
    if (g_vj_fx_dialog == this) {
        g_vj_fx_dialog = nullptr;
    }
}

void vdj_open_vj_fx_dialog()
{
    DeckPair *decks = vdj_decks();
    if (!decks)
        return;

    if (!g_vj_fx_dialog) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_vj_fx_dialog = new VjFxDialog(decks, main_window);
    }
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
