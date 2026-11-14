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
#include <QComboBox>
#include <QTimer>
#include <QSignalBlocker>
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
    height: 6px;
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
    width: 14px;
    margin: -5px 0;
    border-radius: 2px;
    border: 1px solid #6c798e;
}
QSlider::handle:horizontal:hover {
    background: #ffffff;
}
QSlider::groove:vertical {
    width: 6px;
    background: #0d1015;
    border: 1px solid #202733;
    border-radius: 3px;
}
QSlider::sub-page:vertical {
    background: #1c232e;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: #cdd5e2;
    height: 14px;
    margin: 0 -5px;
    border-radius: 2px;
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

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("OBS VJ Mixer"));
    resize(580, 480);
    setStyleSheet(QString::fromUtf8(MIXER_STYLESHEET));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 8, 10, 8);
    main_layout->setSpacing(8);

    // 1. Top Bar
    buildTopBar(main_layout);

    // 2. 3-Column DJ Mixer Area: Channel A | Center Master | Channel B
    auto *mixer_row = new QHBoxLayout();
    mixer_row->setContentsMargins(0, 0, 0, 0);
    mixer_row->setSpacing(8);

    QWidget *strip_a = buildChannelStrip(true);
    QWidget *center_master = buildCenterMasterPanel();
    QWidget *strip_b = buildChannelStrip(false);

    mixer_row->addWidget(strip_a, 4);
    mixer_row->addWidget(center_master, 5);
    mixer_row->addWidget(strip_b, 4);

    main_layout->addLayout(mixer_row, 1);

    // 3. Bottom Bar: MIDI Status
    auto *bottom_row = new QHBoxLayout();
    bottom_row->setContentsMargins(4, 0, 4, 0);
    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Initializing..."), this);
    midi_status_lbl_->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #7f8fa4;"));
    bottom_row->addWidget(midi_status_lbl_);
    bottom_row->addStretch(1);

    auto *clock_bpm_lbl = new QLabel(QStringLiteral("MASTER 128.0 BPM"), this);
    clock_bpm_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace; color: #ffaa00; font-weight: bold;"));
    bottom_row->addWidget(clock_bpm_lbl);

    main_layout->addLayout(bottom_row);

    loadAudioDevices();

    // 50ms GUI update loop
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MixerWindow::refreshMixer);
    timer->start(50);
}

void MixerWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    loadAudioDevices();
}

void MixerWindow::buildTopBar(QVBoxLayout *main_layout)
{
    auto *top_bar = new QHBoxLayout();
    top_bar->setContentsMargins(0, 0, 0, 0);
    top_bar->setSpacing(6);

    auto *title_lbl = new QLabel(QStringLiteral("OBS VJ MIXER"), this);
    title_lbl->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 12px; letter-spacing: 1px;"));
    top_bar->addWidget(title_lbl);

    top_bar->addSpacing(8);

    // OBS Output Selector
    auto *out_lbl = new QLabel(QStringLiteral("OUT:"), this);
    out_lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 10px; color: #8da0b8;"));
    top_bar->addWidget(out_lbl);

    obs_out_master_btn_ = new QPushButton(QStringLiteral("MASTER"), this);
    obs_out_deck_a_btn_ = new QPushButton(QStringLiteral("DECK A"), this);
    obs_out_deck_b_btn_ = new QPushButton(QStringLiteral("DECK B"), this);

    obs_out_master_btn_->setCheckable(true);
    obs_out_deck_a_btn_->setCheckable(true);
    obs_out_deck_b_btn_->setCheckable(true);
    obs_out_master_btn_->setChecked(true);

    top_bar->addWidget(obs_out_master_btn_);
    top_bar->addWidget(obs_out_deck_a_btn_);
    top_bar->addWidget(obs_out_deck_b_btn_);

    connect(obs_out_master_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(0); });
    connect(obs_out_deck_a_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(1); });
    connect(obs_out_deck_b_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(2); });

    top_bar->addStretch(1);

    // Quick Scene Add Buttons
    add_master_btn_ = new QPushButton(QStringLiteral("📺 + MASTER"), this);
    add_master_btn_->setStyleSheet(QStringLiteral("background: #173324; border: 1px solid #2e7d32; color: #4caf50; font-size: 9px; font-weight: bold;"));
    add_master_btn_->setToolTip(QStringLiteral("Add OBS VJ Master Mixer source to current scene"));
    connect(add_master_btn_, &QPushButton::clicked, this, &MixerWindow::addMasterToActiveScene);
    top_bar->addWidget(add_master_btn_);

    add_cue_btn_ = new QPushButton(QStringLiteral("🎧 + CUE"), this);
    add_cue_btn_->setStyleSheet(QStringLiteral("background: #332617; border: 1px solid #7d5a2e; color: #ffaa00; font-size: 9px; font-weight: bold;"));
    add_cue_btn_->setToolTip(QStringLiteral("Add Headphone CUE monitor audio source to current scene"));
    connect(add_cue_btn_, &QPushButton::clicked, this, &MixerWindow::addCueToActiveScene);
    top_bar->addWidget(add_cue_btn_);

    tracklist_btn_ = new QPushButton(QStringLiteral("📋 TRACKLIST"), this);
    tracklist_btn_->setStyleSheet(QStringLiteral("background: #1a222d; border: 1px solid #2d3b4d; color: #00d2ff; font-size: 9px; font-weight: bold;"));
    tracklist_btn_->setToolTip(QStringLiteral("Open Tracklist Library panel"));
    connect(tracklist_btn_, &QPushButton::clicked, this, []() {
        vdj_open_tracklist_window();
    });
    top_bar->addWidget(tracklist_btn_);

    main_layout->addLayout(top_bar);
}

QWidget *MixerWindow::buildChannelStrip(bool is_deck_a)
{
    const QString title = is_deck_a ? QStringLiteral("◄ DECK A") : QStringLiteral("DECK B ►");
    auto *panel = new QGroupBox(title, this);
    panel->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #151a22; border: 1px solid #232b38; border-radius: 4px; padding: 6px; } "
        "QGroupBox::title { color: %1; font-weight: bold; font-size: 11px; }")
        .arg(is_deck_a ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700")));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    DeckPlayer *deck = is_deck_a ? vj_deck_a() : vj_deck_b();

    auto addSliderRow = [panel, layout](const QString &name, const QString &color, DJSlider **out_sl, int def_val, auto setter) {
        auto *row = new QHBoxLayout();
        row->setSpacing(4);
        auto *lbl = new QLabel(name, panel);
        lbl->setFixedWidth(28);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: %1;").arg(color));

        auto *sl = new DJSlider(Qt::Horizontal, def_val, panel);
        sl->setRange(-100, 100);
        sl->setValue(def_val);
        *out_sl = sl;

        row->addWidget(lbl);
        row->addWidget(sl, 1);
        layout->addLayout(row);

        QObject::connect(sl, &QSlider::valueChanged, sl, [setter](int val) {
            setter(1.0f + static_cast<float>(val) / 100.0f);
        });
    };

    // 1. GAIN
    if (is_deck_a) {
        addSliderRow(QStringLiteral("GAIN"), QStringLiteral("#8da0b8"), &gain_a_, 0, [deck](float v) { if (deck) deck->setGain(v); });
        addSliderRow(QStringLiteral("HI"), QStringLiteral("#ff4444"), &hi_a_, 0, [deck](float v) { if (deck) deck->setEqHi(v); });
        addSliderRow(QStringLiteral("MID"), QStringLiteral("#00ff88"), &mid_a_, 0, [deck](float v) { if (deck) deck->setEqMid(v); });
        addSliderRow(QStringLiteral("LOW"), QStringLiteral("#4488ff"), &low_a_, 0, [deck](float v) { if (deck) deck->setEqLow(v); });
        addSliderRow(QStringLiteral("FLT"), QStringLiteral("#ffaa00"), &flt_a_, 0, [deck](float v) { if (deck) deck->setFilterVal(v - 1.0f); });
    } else {
        addSliderRow(QStringLiteral("GAIN"), QStringLiteral("#8da0b8"), &gain_b_, 0, [deck](float v) { if (deck) deck->setGain(v); });
        addSliderRow(QStringLiteral("HI"), QStringLiteral("#ff4444"), &hi_b_, 0, [deck](float v) { if (deck) deck->setEqHi(v); });
        addSliderRow(QStringLiteral("MID"), QStringLiteral("#00ff88"), &mid_b_, 0, [deck](float v) { if (deck) deck->setEqMid(v); });
        addSliderRow(QStringLiteral("LOW"), QStringLiteral("#4488ff"), &low_b_, 0, [deck](float v) { if (deck) deck->setEqLow(v); });
        addSliderRow(QStringLiteral("FLT"), QStringLiteral("#ffaa00"), &flt_b_, 0, [deck](float v) { if (deck) deck->setFilterVal(v - 1.0f); });
    }

    // 2. Headphone CUE button
    auto *cue_btn = new QPushButton(is_deck_a ? QStringLiteral("🎧 CUE A") : QStringLiteral("🎧 CUE B"), panel);
    cue_btn->setCheckable(true);
    cue_btn->setFixedHeight(24);
    cue_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222c; border: 1px solid #2d3b4d; color: %1; font-weight: bold; font-size: 9px; } "
        "QPushButton:checked { background: %2; color: #fff; border-color: %1; }")
        .arg(is_deck_a ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700"))
        .arg(is_deck_a ? QStringLiteral("#0088cc") : QStringLiteral("#cc5500")));
    layout->addWidget(cue_btn);

    if (is_deck_a) {
        hp_cue_a_btn_ = cue_btn;
        connect(cue_btn, &QPushButton::toggled, this, [deck](bool chk) { if (deck) deck->setHeadphoneCue(chk); });
    } else {
        hp_cue_b_btn_ = cue_btn;
        connect(cue_btn, &QPushButton::toggled, this, [deck](bool chk) { if (deck) deck->setHeadphoneCue(chk); });
    }

    // 3. Vertical Volume Fader and Peak Meter Row
    auto *fader_meter_row = new QHBoxLayout();
    fader_meter_row->setContentsMargins(0, 4, 0, 0);
    fader_meter_row->setSpacing(8);

    auto *vol_slider = new DJSlider(Qt::Vertical, 80, panel);
    vol_slider->setRange(0, 100);
    vol_slider->setValue(80);
    vol_slider->setFixedHeight(120);
    vol_slider->setToolTip(QStringLiteral("Channel Volume Fader (Default: 80% Unity Line Level)"));

    auto *meter = new QProgressBar(panel);
    meter->setOrientation(Qt::Vertical);
    meter->setRange(0, 100);
    meter->setValue(0);
    meter->setFixedWidth(10);
    meter->setFixedHeight(120);
    meter->setTextVisible(false);

    if (is_deck_a) {
        vol_a_ = vol_slider;
        meter_a_ = meter;
        connect(vol_slider, &QSlider::valueChanged, this, [deck](int val) {
            if (deck) deck->setVolume(static_cast<float>(val) / 100.0f);
        });
    } else {
        vol_b_ = vol_slider;
        meter_b_ = meter;
        connect(vol_slider, &QSlider::valueChanged, this, [deck](int val) {
            if (deck) deck->setVolume(static_cast<float>(val) / 100.0f);
        });
    }

    fader_meter_row->addStretch(1);
    fader_meter_row->addWidget(vol_slider);
    fader_meter_row->addWidget(meter);
    fader_meter_row->addStretch(1);

    layout->addLayout(fader_meter_row);

    return panel;
}

QWidget *MixerWindow::buildCenterMasterPanel()
{
    auto *panel = new QGroupBox(QStringLiteral("MASTER & CROSSFADER"), this);
    panel->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #13171e; border: 1px solid #222936; border-radius: 4px; padding: 6px; } "
        "QGroupBox::title { color: #8da0b8; font-weight: bold; font-size: 11px; }"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(6);

    // 1. Transition FX Selector
    auto *trans_row = new QHBoxLayout();
    auto *trans_lbl = new QLabel(QStringLiteral("FX:"), panel);
    trans_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #8da0b8;"));
    trans_combo_ = new QComboBox(panel);
    trans_combo_->addItem(QStringLiteral("Linear Fade"), 0);
    trans_combo_->addItem(QStringLiteral("Horizontal Wipe"), 1);
    trans_combo_->addItem(QStringLiteral("Additive Blend"), 2);
    trans_combo_->addItem(QStringLiteral("Hard Cut"), 3);
    trans_row->addWidget(trans_lbl);
    trans_row->addWidget(trans_combo_, 1);
    layout->addLayout(trans_row);

    connect(trans_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (decks_ && trans_combo_) {
            decks_->setTransitionMode(trans_combo_->currentData().toInt());
        }
    });

    // 2. Headphone Monitoring Section
    auto *hp_box = new QFrame(panel);
    hp_box->setStyleSheet(QStringLiteral("background: #0f1218; border: 1px solid #1c232f; border-radius: 3px; padding: 4px;"));
    auto *hp_vbox = new QVBoxLayout(hp_box);
    hp_vbox->setContentsMargins(4, 2, 4, 2);
    hp_vbox->setSpacing(3);

    auto *hp_dev_lbl = new QLabel(QStringLiteral("🎧 CUE MONITOR DEVICE:"), hp_box);
    hp_dev_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #ffaa00;"));
    hp_vbox->addWidget(hp_dev_lbl);

    headphone_device_combo_ = new QComboBox(hp_box);
    headphone_device_combo_->setToolTip(QStringLiteral("Select headphone audio output device (soundcard)"));
    hp_vbox->addWidget(headphone_device_combo_);

    auto *hp_sl_row = new QHBoxLayout();
    hp_sl_row->setSpacing(4);

    auto *hp_vol_vbox = new QVBoxLayout();
    auto *hp_vol_lbl = new QLabel(QStringLiteral("HP VOL"), hp_box);
    hp_vol_lbl->setAlignment(Qt::AlignCenter);
    hp_vol_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #8da0b8;"));
    hp_vol_slider_ = new DJSlider(Qt::Horizontal, 100, hp_box);
    hp_vol_slider_->setRange(0, 100);
    hp_vol_slider_->setValue(100);
    hp_vol_vbox->addWidget(hp_vol_lbl);
    hp_vol_vbox->addWidget(hp_vol_slider_);

    auto *hp_mix_vbox = new QVBoxLayout();
    auto *hp_mix_lbl = new QLabel(QStringLiteral("CUE / PGM"), hp_box);
    hp_mix_lbl->setAlignment(Qt::AlignCenter);
    hp_mix_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #8da0b8;"));
    hp_mix_slider_ = new DJSlider(Qt::Horizontal, 50, hp_box);
    hp_mix_slider_->setRange(0, 100);
    hp_mix_slider_->setValue(50);
    hp_mix_vbox->addWidget(hp_mix_lbl);
    hp_mix_vbox->addWidget(hp_mix_slider_);

    hp_sl_row->addLayout(hp_vol_vbox);
    hp_sl_row->addLayout(hp_mix_vbox);
    hp_vbox->addLayout(hp_sl_row);

    layout->addWidget(hp_box);

    connect(hp_vol_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_) decks_->setHeadphoneVolume(static_cast<float>(v) / 100.0f);
    });
    connect(hp_mix_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_) decks_->setHeadphoneMix(static_cast<float>(v) / 100.0f);
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

    crossfader_ = new DJSlider(Qt::Horizontal, 500, panel);
    crossfader_->setRange(0, 1000);
    crossfader_->setValue(500);
    crossfader_->setFixedHeight(30);
    crossfader_->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 10px; background: #0c0f14; border: 1px solid #232b38; border-radius: 5px; } "
        "QSlider::handle:horizontal { background: #ffffff; width: 26px; margin: -6px 0; border-radius: 3px; border: 2px solid #0088aa; }"));
    crossfader_->setToolTip(QStringLiteral("Master Crossfader (Double-click resets to 50/50 center)"));
    cf_box->addWidget(crossfader_);

    connect(crossfader_, &QSlider::valueChanged, this, [this](int val) {
        if (decks_) {
            decks_->setCrossfader(static_cast<double>(val) / 1000.0);
        }
    });

    layout->addLayout(cf_box);

    return panel;
}

void MixerWindow::updateObsOutputButtons(int mode)
{
    if (decks_)
        decks_->setObsOutputMode(mode);

    obs_out_master_btn_->setChecked(mode == 0);
    obs_out_deck_a_btn_->setChecked(mode == 1);
    obs_out_deck_b_btn_->setChecked(mode == 2);
}

void MixerWindow::loadAudioDevices()
{
    if (!headphone_device_combo_)
        return;

    const QSignalBlocker blocker(headphone_device_combo_);
    headphone_device_combo_->clear();

    obs_enum_audio_monitoring_devices(
        [](void *param, const char *name, const char *id) -> bool {
            auto *cb = static_cast<QComboBox *>(param);
            const QString dev_name = QString::fromUtf8(name);
            const QString dev_id = QString::fromUtf8(id);
            cb->addItem(dev_name, dev_id);
            return true;
        },
        headphone_device_combo_);

    const char *current_name = nullptr;
    const char *current_id = nullptr;
    obs_get_audio_monitoring_device(&current_name, &current_id);
    if (current_id && *current_id) {
        const QString cur_str = QString::fromUtf8(current_id);
        const int idx_h = headphone_device_combo_->findData(cur_str);
        if (idx_h >= 0)
            headphone_device_combo_->setCurrentIndex(idx_h);
    }

    connect(headphone_device_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (index < 0 || !headphone_device_combo_)
                    return;
                const QString id = headphone_device_combo_->itemData(index).toString();
                const QString name = headphone_device_combo_->itemText(index);
                if (!id.isEmpty() && obs_audio_monitoring_available()) {
                    obs_set_audio_monitoring_device(name.toUtf8().constData(), id.toUtf8().constData());
                }
            });
}

void MixerWindow::addMasterToActiveScene()
{
    obs_source_t *scene_source = obs_frontend_get_current_scene();
    if (!scene_source)
        return;

    obs_scene_t *scene = obs_scene_from_source(scene_source);
    if (scene) {
        obs_source_t *master = obs_get_source_by_name("OBS VJ Master Mixer");
        if (!master) {
            master = obs_source_create("vdj_master_mixer", "OBS VJ Master Mixer", nullptr, nullptr);
        }
        if (master) {
            obs_sceneitem_t *item = obs_scene_find_source(scene, "OBS VJ Master Mixer");
            if (!item) {
                obs_scene_add(scene, master);
            }
            obs_source_release(master);
        }
    }
    obs_source_release(scene_source);
}

void MixerWindow::addCueToActiveScene()
{
    obs_source_t *scene_source = obs_frontend_get_current_scene();
    if (!scene_source)
        return;

    obs_scene_t *scene = obs_scene_from_source(scene_source);
    if (scene) {
        obs_source_t *cue = obs_get_source_by_name("OBS VJ Headphone CUE");
        if (!cue) {
            cue = obs_source_create("vdj_headphone_cue", "OBS VJ Headphone CUE", nullptr, nullptr);
        }
        if (cue) {
            obs_source_set_monitoring_type(cue, OBS_MONITORING_TYPE_MONITOR_ONLY);
            obs_source_set_audio_mixers(cue, 0); // mute on main stream / recording tracks
            obs_sceneitem_t *item = obs_scene_find_source(scene, "OBS VJ Headphone CUE");
            if (!item) {
                obs_scene_add(scene, cue);
            }
            obs_source_release(cue);
        }
    }
    obs_source_release(scene_source);
}

void MixerWindow::refreshMixer()
{
    if (!decks_)
        return;

    decks_->updateSync();

    if (crossfader_ && !crossfader_->isSliderDown()) {
        const int cf_val = static_cast<int>(decks_->crossfader() * 1000.0);
        if (std::abs(crossfader_->value() - cf_val) > 2) {
            QSignalBlocker blocker(crossfader_);
            crossfader_->setValue(cf_val);
        }
    }

    auto updateSliderFromVal = [](DJSlider *sl, int target_val) {
        if (sl && !sl->isSliderDown()) {
            if (std::abs(sl->value() - target_val) > 1) {
                QSignalBlocker blocker(sl);
                sl->setValue(target_val);
            }
        }
    };

    updateSliderFromVal(gain_a_, static_cast<int>((decks_->deckA().gain() - 1.0f) * 100.0f));
    updateSliderFromVal(hi_a_, static_cast<int>((decks_->deckA().eqHi() - 1.0f) * 100.0f));
    updateSliderFromVal(mid_a_, static_cast<int>((decks_->deckA().eqMid() - 1.0f) * 100.0f));
    updateSliderFromVal(low_a_, static_cast<int>((decks_->deckA().eqLow() - 1.0f) * 100.0f));
    updateSliderFromVal(flt_a_, static_cast<int>(decks_->deckA().filterVal() * 100.0f));
    updateSliderFromVal(vol_a_, static_cast<int>(decks_->deckA().volume() * 100.0f));

    updateSliderFromVal(gain_b_, static_cast<int>((decks_->deckB().gain() - 1.0f) * 100.0f));
    updateSliderFromVal(hi_b_, static_cast<int>((decks_->deckB().eqHi() - 1.0f) * 100.0f));
    updateSliderFromVal(mid_b_, static_cast<int>((decks_->deckB().eqMid() - 1.0f) * 100.0f));
    updateSliderFromVal(low_b_, static_cast<int>((decks_->deckB().eqLow() - 1.0f) * 100.0f));
    updateSliderFromVal(flt_b_, static_cast<int>(decks_->deckB().filterVal() * 100.0f));
    updateSliderFromVal(vol_b_, static_cast<int>(decks_->deckB().volume() * 100.0f));

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
        midi_status_lbl_->setText(conn ? QStringLiteral("MIDI: NI S2 MK3 CONNECTED") : QStringLiteral("MIDI: Disconnected"));
        midi_status_lbl_->setStyleSheet(conn
            ? QStringLiteral("color: #00ff88; font-weight: bold; font-size: 10px;")
            : QStringLiteral("color: #ff5555; font-weight: bold; font-size: 10px;"));
    }
}
