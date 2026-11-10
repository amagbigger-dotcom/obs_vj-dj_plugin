#include "mixer-dock.hpp"
#include "deck-engine.hpp"
#include "midi-handler.hpp"
#include "waveform-widget.hpp"

#include <obs.h>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
struct AudioDeviceEntry {
    QString name;
    QString id;
};

struct AudioDeviceList {
    std::vector<AudioDeviceEntry> entries;
};

bool enum_audio_monitoring_devices_cb(void *data, const char *name, const char *id)
{
    auto *list = static_cast<AudioDeviceList *>(data);
    if (!list || !name || !id || !*name || !*id)
        return true;

    list->entries.push_back({QString::fromUtf8(name), QString::fromUtf8(id)});
    return true;
}

static const char *TRAKTOR_PRO_STYLESHEET = R"(
QDialog {
    background-color: #0d0f12;
    color: #e0e4ec;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 12px;
}
QGroupBox {
    background-color: #14171d;
    border: 1px solid #232832;
    border-radius: 6px;
    margin-top: 12px;
    padding-top: 10px;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top center;
    padding: 3px 14px;
    border-radius: 3px;
    font-size: 11px;
    letter-spacing: 1px;
}
QGroupBox#deck_a {
    border: 1px solid #005f7a;
}
QGroupBox#deck_a::title {
    background-color: #007799;
    color: #ffffff;
}
QGroupBox#deck_b {
    border: 1px solid #993d00;
}
QGroupBox#deck_b::title {
    background-color: #bb4400;
    color: #ffffff;
}
QGroupBox#mixer_section {
    border: 1px solid #2d3340;
}
QGroupBox#mixer_section::title {
    background-color: #252b36;
    color: #c0c6d4;
}
QPushButton {
    background-color: #1f232c;
    color: #e0e4ec;
    border: 1px solid #333a48;
    border-radius: 4px;
    padding: 5px 10px;
    font-weight: bold;
    font-size: 11px;
}
QPushButton:hover {
    background-color: #2a303c;
    border-color: #4a5468;
}
QPushButton:pressed {
    background-color: #161920;
}
QPushButton#play_btn {
    background-color: #113620;
    border: 1px solid #27ae60;
    color: #2ecc71;
    font-size: 12px;
    min-height: 26px;
}
QPushButton#play_btn:hover {
    background-color: #164629;
}
QPushButton#cue_btn {
    background-color: #422d0d;
    border: 1px solid #e67e22;
    color: #f39c12;
    font-size: 12px;
    min-height: 26px;
}
QPushButton#cue_btn:hover {
    background-color: #553a12;
}
QPushButton#sync_btn {
    background-color: #152b42;
    border: 1px solid #2980b9;
    color: #3498db;
    min-height: 26px;
}
QPushButton#hp_cue_btn {
    background-color: #1e222a;
    border: 1px solid #3d4554;
    color: #8c96a8;
    font-size: 11px;
    min-height: 24px;
}
QPushButton#hp_cue_btn:checked {
    background-color: #004d66;
    border: 1px solid #00d2ff;
    color: #00d2ff;
}
QPushButton#hp_cue_btn_b:checked {
    background-color: #662900;
    border: 1px solid #ff7700;
    color: #ffaa55;
}
QPushButton#obs_out_btn {
    background-color: #1a1e26;
    border: 1px solid #303746;
    color: #9aa2b2;
    font-size: 11px;
    padding: 6px;
}
QPushButton#obs_out_btn:checked {
    background-color: #007799;
    border: 1px solid #00d2ff;
    color: #ffffff;
}
QSlider::groove:horizontal {
    height: 8px;
    background: #171a21;
    border: 1px solid #282e3b;
    border-radius: 4px;
}
QSlider::sub-page:horizontal {
    background: #007799;
    border-radius: 4px;
}
QSlider::handle:horizontal {
    background: #e2e6ee;
    width: 18px;
    margin: -5px 0;
    border-radius: 3px;
    border: 1px solid #9aa2b2;
}
QSlider::handle:horizontal:hover {
    background: #ffffff;
}
QSlider::groove:vertical {
    width: 6px;
    background: #171a21;
    border: 1px solid #282e3b;
    border-radius: 3px;
}
QSlider::sub-page:vertical {
    background: #2a313e;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: #d8dce6;
    height: 18px;
    margin: 0 -6px;
    border-radius: 3px;
    border: 1px solid #7d8594;
}
QSlider::handle:vertical:hover {
    background: #ffffff;
}
QComboBox {
    background-color: #1c2027;
    color: #e0e4ec;
    border: 1px solid #333a48;
    border-radius: 4px;
    padding: 4px 8px;
    font-size: 11px;
}
QComboBox:hover {
    border-color: #4a5468;
}
QLabel {
    color: #8c94a4;
}
)";
} // namespace

QString MixerWindow::formatTime(qint64 milliseconds)
{
    const qint64 total_seconds = milliseconds / 1000;
    const qint64 minutes = total_seconds / 60;
    const qint64 seconds = total_seconds % 60;

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("Traktor Kontrol S2 MK3 — Video DJ Mixer"));
    resize(1140, 680);
    setStyleSheet(QString::fromUtf8(TRAKTOR_PRO_STYLESHEET));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 10, 12, 12);
    main_layout->setSpacing(8);

    // Top status header
    auto *status_layout = new QHBoxLayout();
    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Scanning..."), this);
    midi_status_lbl_->setStyleSheet(
        QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px;"));
    status_layout->addWidget(midi_status_lbl_);

    status_layout->addStretch();
    auto *title_lbl = new QLabel(QStringLiteral("OBS VIDEO DJ MIXER • NATIVE INSTRUMENTS S2 MK3"), this);
    title_lbl->setStyleSheet(QStringLiteral("color: #616a7d; font-weight: bold; font-size: 11px; letter-spacing: 1px;"));
    status_layout->addWidget(title_lbl);
    status_layout->addStretch();

    main_layout->addLayout(status_layout);

    auto *root_layout = new QHBoxLayout();
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(10);

    auto *deck_a_group = new QGroupBox(QStringLiteral("DECK A"), this);
    deck_a_group->setObjectName(QStringLiteral("deck_a"));

    auto *mixer_group = new QGroupBox(QStringLiteral("2-CHANNEL MIXER"), this);
    mixer_group->setObjectName(QStringLiteral("mixer_section"));

    auto *deck_b_group = new QGroupBox(QStringLiteral("DECK B"), this);
    deck_b_group->setObjectName(QStringLiteral("deck_b"));

    buildDeckPanel(deck_a_group, true);
    buildMixerSection(mixer_group);
    buildDeckPanel(deck_b_group, false);

    root_layout->addWidget(deck_a_group, 5);
    root_layout->addWidget(mixer_group, 4);
    root_layout->addWidget(deck_b_group, 5);
    main_layout->addLayout(root_layout);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        refreshDeck(true);
        refreshDeck(false);
        refreshMixer();
    });
    timer->start(50);

    refreshMixer();
}

void MixerWindow::loadAudioMonitoringDevices()
{
    if (!audio_monitor_devices_)
        return;

    audio_monitor_devices_->clear();

    if (!obs_audio_monitoring_available()) {
        audio_monitor_devices_->addItem(QStringLiteral("Monitoring unavailable"));
        audio_monitor_devices_->setEnabled(false);
        return;
    }

    AudioDeviceList devices;
    obs_enum_audio_monitoring_devices(enum_audio_monitoring_devices_cb, &devices);

    const char *current_name = nullptr;
    const char *current_id = nullptr;
    obs_get_audio_monitoring_device(&current_name, &current_id);

    int selected_index = -1;
    for (const auto &device : devices.entries) {
        audio_monitor_devices_->addItem(device.name, device.id);
        if (current_id && device.id == QString::fromUtf8(current_id))
            selected_index = audio_monitor_devices_->count() - 1;
    }

    if (!devices.entries.empty()) {
        audio_monitor_devices_->setEnabled(true);
        const int idx = (selected_index >= 0) ? selected_index : 0;
        audio_monitor_devices_->setCurrentIndex(idx);

        const QString dev_name = audio_monitor_devices_->itemText(idx);
        const QString dev_id = audio_monitor_devices_->itemData(idx).toString();
        if (!dev_id.isEmpty()) {
            obs_set_audio_monitoring_device(
                dev_name.toUtf8().constData(),
                dev_id.toUtf8().constData());
            obs_reset_audio_monitoring();
        }
    } else {
        audio_monitor_devices_->addItem(QStringLiteral("No devices found"));
        audio_monitor_devices_->setEnabled(false);
    }

    if (decks_)
        decks_->enableAudioMonitoring();
}

void MixerWindow::buildDeckPanel(QWidget *parent, bool is_deck_a)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    if (!decks_)
        return;

    DeckPlayer *deck = is_deck_a ? &decks_->deckA() : &decks_->deckB();

    // 1. Top File Load row
    auto *file_row = new QHBoxLayout();
    auto *load_button = new QPushButton(QStringLiteral("📂 LOAD TRACK"), parent);
    load_button->setStyleSheet(QStringLiteral(
        "QPushButton { background: #222733; color: #ffffff; border: 1px solid #3c4456; border-radius: 4px; padding: 4px 10px; font-weight: bold; } "
        "QPushButton:hover { background: #2f3647; }"));
    file_row->addWidget(load_button);
    file_row->addStretch();
    layout->addLayout(file_row);

    // 2. Audio Waveform & BPM Widget
    auto *waveform = new WaveformWidget(deck, is_deck_a, parent);
    layout->addWidget(waveform);

    if (is_deck_a)
        waveform_a_ = waveform;
    else
        waveform_b_ = waveform;

    // 3. Transport & Cue Buttons
    auto *transport_row = new QHBoxLayout();
    transport_row->setSpacing(6);

    auto *play_button = new QPushButton(QStringLiteral("▶ PLAY"), parent);
    play_button->setObjectName(QStringLiteral("play_btn"));

    auto *cue_button = new QPushButton(QStringLiteral("CUE"), parent);
    cue_button->setObjectName(QStringLiteral("cue_btn"));

    auto *sync_button = new QPushButton(QStringLiteral("SYNC"), parent);
    sync_button->setObjectName(QStringLiteral("sync_btn"));

    auto *hp_cue_button = new QPushButton(QStringLiteral("🎧 CUE"), parent);
    hp_cue_button->setObjectName(is_deck_a ? QStringLiteral("hp_cue_btn") : QStringLiteral("hp_cue_btn_b"));
    hp_cue_button->setCheckable(true);
    hp_cue_button->setChecked(false);
    hp_cue_button->setToolTip(QStringLiteral("Headphone Pre-Fade Listen (PFL)"));

    if (is_deck_a) {
        play_btn_a_ = play_button;
        hp_cue_a_btn_ = hp_cue_button;
    } else {
        play_btn_b_ = play_button;
        hp_cue_b_btn_ = hp_cue_button;
    }

    transport_row->addWidget(play_button, 2);
    transport_row->addWidget(cue_button, 1);
    transport_row->addWidget(sync_button, 1);
    transport_row->addWidget(hp_cue_button, 1);
    layout->addLayout(transport_row);

    // 4. Time display and Pitch / Loop row
    auto *ctrl_row = new QHBoxLayout();

    // Left sub-column: Time and Loop
    auto *loop_col = new QVBoxLayout();

    auto *time_label = new QLabel(QStringLiteral("00:00 / 00:00"), parent);
    time_label->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-family: monospace; font-size: 13px;"));
    time_label->setAlignment(Qt::AlignCenter);
    if (is_deck_a)
        time_a_ = time_label;
    else
        time_b_ = time_label;
    loop_col->addWidget(time_label);

    auto *loop_btn_row = new QHBoxLayout();
    auto *loop_button = new QPushButton(QStringLiteral("AUTO LOOP"), parent);
    loop_button->setCheckable(true);
    loop_button->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a221e; border: 1px solid #2e7d32; color: #4caf50; font-weight: bold; } "
        "QPushButton:checked { background: #2e7d32; color: #ffffff; }"));

    auto *loop_combo = new QComboBox(parent);
    loop_combo->addItem(QStringLiteral("1/2 Beat"));
    loop_combo->addItem(QStringLiteral("1 Beat"));
    loop_combo->addItem(QStringLiteral("2 Beats"));
    loop_combo->addItem(QStringLiteral("4 Beats"));
    loop_combo->addItem(QStringLiteral("8 Beats"));
    loop_combo->addItem(QStringLiteral("16 Beats"));
    loop_combo->addItem(QStringLiteral("32 Beats"));
    loop_combo->addItem(QStringLiteral("64 Beats"));
    loop_combo->setCurrentIndex(3);

    if (is_deck_a)
        loop_combo_a_ = loop_combo;
    else
        loop_combo_b_ = loop_combo;

    loop_btn_row->addWidget(loop_button);
    loop_btn_row->addWidget(loop_combo);
    loop_col->addLayout(loop_btn_row);

    ctrl_row->addLayout(loop_col, 2);

    // Right sub-column: Pitch Slider with Rate Label
    auto *pitch_col = new QVBoxLayout();
    auto *rate_label = new QLabel(QStringLiteral("100%"), parent);
    rate_label->setAlignment(Qt::AlignCenter);
    rate_label->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px; color: #e0e4ec;"));

    if (is_deck_a)
        rate_lbl_a_ = rate_label;
    else
        rate_lbl_b_ = rate_label;

    auto *pitch_slider = new QSlider(Qt::Horizontal, parent);
    pitch_slider->setRange(50, 150);
    pitch_slider->setValue(100);
    pitch_slider->setToolTip(QStringLiteral("Tempo Pitch Fader (50% - 150%)"));

    if (is_deck_a)
        pitch_slider_a_ = pitch_slider;
    else
        pitch_slider_b_ = pitch_slider;

    auto *pitch_hdr = new QHBoxLayout();
    auto *pitch_title = new QLabel(QStringLiteral("TEMPO"), parent);
    pitch_title->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #7f889b;"));
    auto *reset_pitch = new QPushButton(QStringLiteral("RESET"), parent);
    reset_pitch->setFixedSize(48, 18);
    reset_pitch->setStyleSheet(QStringLiteral("font-size: 9px; padding: 1px;"));
    pitch_hdr->addWidget(pitch_title);
    pitch_hdr->addStretch();
    pitch_hdr->addWidget(rate_label);
    pitch_hdr->addWidget(reset_pitch);

    pitch_col->addLayout(pitch_hdr);
    pitch_col->addWidget(pitch_slider);
    ctrl_row->addLayout(pitch_col, 3);

    layout->addLayout(ctrl_row);

    // Connections
    connect(load_button, &QPushButton::clicked, this, [this, deck, waveform]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select Media"),
            QString(),
            QStringLiteral("Video/Audio (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All (*.*)"));

        if (!path.isEmpty()) {
            deck->loadFile(path);
            waveform->updateTrackInfo(QFileInfo(path).fileName());
        }
    });

    connect(play_button, &QPushButton::clicked, this, [deck]() { deck->togglePlayback(); });
    connect(cue_button, &QPushButton::clicked, this, [deck]() { deck->cue(); });

    connect(hp_cue_button, &QPushButton::toggled, this, [deck](bool checked) {
        deck->setHeadphoneCue(checked);
    });

    connect(loop_button, &QPushButton::toggled, this, [deck, loop_combo](bool enabled) {
        deck->setLoopBeatIndex(loop_combo->currentIndex());
        deck->setLooping(enabled);
    });

    connect(loop_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [deck, loop_button](int index) {
                deck->setLoopBeatIndex(index);
                if (loop_button->isChecked())
                    deck->setLooping(true);
            });

    // Debounced pitch slider to avoid restarting decoder on every tick
    auto *pitch_timer = new QTimer(this);
    pitch_timer->setSingleShot(true);
    pitch_timer->setInterval(80);

    connect(pitch_slider, &QSlider::valueChanged, this, [deck, rate_label, pitch_timer](int value) {
        rate_label->setText(QStringLiteral("%1%").arg(value));
        pitch_timer->start();
    });

    connect(pitch_timer, &QTimer::timeout, this, [deck, pitch_slider]() {
        deck->setSpeed(pitch_slider->value());
    });

    connect(reset_pitch, &QPushButton::clicked, this, [deck, pitch_slider, rate_label]() {
        deck->setSpeed(100);
        pitch_slider->setValue(100);
        rate_label->setText(QStringLiteral("100%"));
    });

    connect(sync_button, &QPushButton::clicked, this, [this, is_deck_a]() {
        if (!decks_)
            return;
        if (is_deck_a)
            decks_->syncTempoAtoB();
        else
            decks_->syncTempoBtoA();
    });
}

void MixerWindow::buildMixerSection(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    if (!decks_)
        return;

    // 1. OBS Program / Preview Out Selector
    auto *obs_out_group = new QGroupBox(QStringLiteral("OBS ACTIVE OUTPUT"), parent);
    obs_out_group->setObjectName(QStringLiteral("obs_out_box"));
    obs_out_group->setStyleSheet(QStringLiteral(
        "QGroupBox#obs_out_box { background: #181c24; border: 1px solid #2b3240; border-radius: 4px; margin-top: 10px; }"));
    auto *obs_out_layout = new QHBoxLayout(obs_out_group);
    obs_out_layout->setContentsMargins(6, 8, 6, 6);
    obs_out_layout->setSpacing(4);

    obs_out_master_btn_ = new QPushButton(QStringLiteral("MASTER MIX"), obs_out_group);
    obs_out_deck_a_btn_ = new QPushButton(QStringLiteral("DECK A"), obs_out_group);
    obs_out_deck_b_btn_ = new QPushButton(QStringLiteral("DECK B"), obs_out_group);

    obs_out_master_btn_->setObjectName(QStringLiteral("obs_out_btn"));
    obs_out_deck_a_btn_->setObjectName(QStringLiteral("obs_out_btn"));
    obs_out_deck_b_btn_->setObjectName(QStringLiteral("obs_out_btn"));

    obs_out_master_btn_->setCheckable(true);
    obs_out_deck_a_btn_->setCheckable(true);
    obs_out_deck_b_btn_->setCheckable(true);
    obs_out_master_btn_->setChecked(true);

    obs_out_layout->addWidget(obs_out_master_btn_);
    obs_out_layout->addWidget(obs_out_deck_a_btn_);
    obs_out_layout->addWidget(obs_out_deck_b_btn_);
    layout->addWidget(obs_out_group);

    connect(obs_out_master_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(0); });
    connect(obs_out_deck_a_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(1); });
    connect(obs_out_deck_b_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(2); });

    // 2. Video Transition Selector
    auto *transition_row = new QHBoxLayout();
    auto *transition_label = new QLabel(QStringLiteral("TRANSITION:"), parent);
    transition_label->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 10px; color: #7f889b;"));

    auto *transition_combo = new QComboBox(parent);
    transition_combo->addItem(QStringLiteral("Linear Fade"), 0);
    transition_combo->addItem(QStringLiteral("Horizontal Wipe"), 1);
    transition_combo->addItem(QStringLiteral("Additive Blend"), 2);
    transition_combo->addItem(QStringLiteral("Hard Cut"), 3);

    transition_row->addWidget(transition_label);
    transition_row->addWidget(transition_combo, 1);
    layout->addLayout(transition_row);

    connect(transition_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, transition_combo](int) {
                if (decks_)
                    decks_->setTransitionMode(transition_combo->currentData().toInt());
            });

    // 3. EQ & Volume Channel Strips
    auto *channels = new QHBoxLayout();
    channels->setSpacing(12);

    auto createChannelControls = [this, parent, channels](bool is_deck_a) {
        auto *channel_layout = new QVBoxLayout();
        channel_layout->setSpacing(4);

        const QString deck_name = is_deck_a ? QStringLiteral("DECK A") : QStringLiteral("DECK B");
        const QString title_color = is_deck_a ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700");

        auto *channel_title = new QLabel(deck_name, parent);
        channel_title->setAlignment(Qt::AlignCenter);
        channel_title->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 11px;").arg(title_color));
        channel_layout->addWidget(channel_title);

        auto addControl = [parent, channel_layout](const QString &name,
                                                   QSlider **stored_slider,
                                                   int minimum,
                                                   int maximum,
                                                   int initial_value,
                                                   const QString &tooltip) {
            auto *control_layout = new QHBoxLayout();
            control_layout->setContentsMargins(0, 0, 0, 0);

            auto *label = new QLabel(name, parent);
            label->setFixedWidth(38);
            label->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #7a8496;"));

            auto *slider = new QSlider(Qt::Horizontal, parent);
            slider->setRange(minimum, maximum);
            slider->setValue(initial_value);
            slider->setToolTip(tooltip);

            control_layout->addWidget(label);
            control_layout->addWidget(slider, 1);
            channel_layout->addLayout(control_layout);

            if (stored_slider)
                *stored_slider = slider;

            return slider;
        };

        QSlider **hi_slot = is_deck_a ? &hi_a_ : &hi_b_;
        QSlider **mid_slot = is_deck_a ? &mid_a_ : &mid_b_;
        QSlider **low_slot = is_deck_a ? &low_a_ : &low_b_;
        QSlider **filter_slot = is_deck_a ? &flt_a_ : &flt_b_;
        QSlider **volume_slot = is_deck_a ? &vol_a_ : &vol_b_;

        QSlider *hi_slider = addControl(
            QStringLiteral("HI"), hi_slot, -100, 100, 0,
            QStringLiteral("EQ high band (Red tint: 0.0 - 2.0)"));
        QSlider *mid_slider = addControl(
            QStringLiteral("MID"), mid_slot, -100, 100, 0,
            QStringLiteral("EQ mid band (Green tint: 0.0 - 2.0)"));
        QSlider *low_slider = addControl(
            QStringLiteral("LOW"), low_slot, -100, 100, 0,
            QStringLiteral("EQ low band (Blue tint: 0.0 - 2.0)"));
        QSlider *filter_slider = addControl(
            QStringLiteral("FILTER"), filter_slot, -100, 100, 0,
            QStringLiteral("Filter (Saturation: 0.0 - 2.0)"));
        QSlider *volume_slider = addControl(
            QStringLiteral("VOL"), volume_slot, 0, 100, 100,
            QStringLiteral("Deck channel volume"));

        DeckPlayer *deck = is_deck_a ? &decks_->deckA() : &decks_->deckB();

        connect(hi_slider, &QSlider::valueChanged, this, [deck](int value) {
            deck->setRed(1.0f + static_cast<float>(value) / 100.0f);
        });
        connect(mid_slider, &QSlider::valueChanged, this, [deck](int value) {
            deck->setGreen(1.0f + static_cast<float>(value) / 100.0f);
        });
        connect(low_slider, &QSlider::valueChanged, this, [deck](int value) {
            deck->setBlue(1.0f + static_cast<float>(value) / 100.0f);
        });
        connect(filter_slider, &QSlider::valueChanged, this, [deck](int value) {
            deck->setSaturation(1.0f + static_cast<float>(value) / 100.0f);
        });
        connect(volume_slider, &QSlider::valueChanged, this, [deck](int value) {
            deck->setVolume(static_cast<float>(value) / 100.0f);
        });

        channels->addLayout(channel_layout);
    };

    createChannelControls(true);
    createChannelControls(false);
    layout->addLayout(channels);

    // 4. Center Master Crossfader
    auto *crossfader_box = new QVBoxLayout();
    crossfader_box->setContentsMargins(0, 4, 0, 4);

    auto *crossfader_hdr = new QHBoxLayout();
    auto *cf_label_a = new QLabel(QStringLiteral("◄ DECK A"), parent);
    cf_label_a->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px;"));
    auto *cf_title = new QLabel(QStringLiteral("CROSSFADER"), parent);
    cf_title->setStyleSheet(QStringLiteral("color: #7a8496; font-weight: bold; font-size: 10px;"));
    auto *cf_label_b = new QLabel(QStringLiteral("DECK B ►"), parent);
    cf_label_b->setStyleSheet(QStringLiteral("color: #ff7700; font-weight: bold; font-size: 11px;"));

    crossfader_hdr->addWidget(cf_label_a);
    crossfader_hdr->addStretch();
    crossfader_hdr->addWidget(cf_title);
    crossfader_hdr->addStretch();
    crossfader_hdr->addWidget(cf_label_b);
    crossfader_box->addLayout(crossfader_hdr);

    auto *crossfader_slider = new QSlider(Qt::Horizontal, parent);
    crossfader_slider->setRange(0, 1000);
    crossfader_slider->setValue(500);
    crossfader_slider->setFixedHeight(28);
    crossfader_slider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 10px; background: #16181f; border: 1px solid #333a48; border-radius: 5px; } "
        "QSlider::handle:horizontal { background: #ffffff; width: 24px; margin: -6px 0; border-radius: 3px; border: 2px solid #0088aa; }"));
    crossfader_box->addWidget(crossfader_slider);
    crossfader_ = crossfader_slider;

    connect(crossfader_slider, &QSlider::valueChanged, this, [this](int value) {
        if (decks_)
            decks_->setCrossfader(static_cast<double>(value) / 1000.0);
    });
    layout->addLayout(crossfader_box);

    // 5. Audio Output & Headphone Monitor Section
    auto *monitor_group = new QGroupBox(QStringLiteral("HEADPHONE & MONITOR OUTPUT"), parent);
    monitor_group->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #161922; border: 1px solid #282f3d; border-radius: 4px; margin-top: 10px; padding-top: 8px; }"));
    auto *mon_layout = new QVBoxLayout(monitor_group);
    mon_layout->setContentsMargins(8, 8, 8, 8);
    mon_layout->setSpacing(6);

    auto *dev_row = new QHBoxLayout();
    auto *dev_lbl = new QLabel(QStringLiteral("DEVICE:"), monitor_group);
    dev_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #7f889b;"));
    audio_monitor_devices_ = new QComboBox(monitor_group);
    dev_row->addWidget(dev_lbl);
    dev_row->addWidget(audio_monitor_devices_, 1);
    mon_layout->addLayout(dev_row);

    connect(audio_monitor_devices_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (index < 0 || !audio_monitor_devices_)
                    return;
                const QString selected_id = audio_monitor_devices_->itemData(index).toString();
                const QString selected_name = audio_monitor_devices_->itemText(index);
                if (selected_id.isEmpty())
                    return;
                if (obs_audio_monitoring_available()) {
                    obs_set_audio_monitoring_device(
                        selected_name.toUtf8().constData(),
                        selected_id.toUtf8().constData());
                    obs_reset_audio_monitoring();
                }
                if (decks_)
                    decks_->enableAudioMonitoring();
            });

    loadAudioMonitoringDevices();

    // Headphone Cue / Master Mix knob & Volume
    auto *hp_row = new QHBoxLayout();

    auto *cue_mix_col = new QVBoxLayout();
    auto *cue_mix_title = new QLabel(QStringLiteral("CUE / MASTER MIX"), monitor_group);
    cue_mix_title->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #7a8496;"));
    cue_mix_title->setAlignment(Qt::AlignCenter);

    hp_mix_slider_ = new QSlider(Qt::Horizontal, monitor_group);
    hp_mix_slider_->setRange(0, 100);
    hp_mix_slider_->setValue(50);
    hp_mix_slider_->setToolTip(QStringLiteral("Headphone Mix: 0% = Pure CUE, 100% = Pure Master"));

    cue_mix_col->addWidget(cue_mix_title);
    cue_mix_col->addWidget(hp_mix_slider_);
    hp_row->addLayout(cue_mix_col);

    auto *hp_vol_col = new QVBoxLayout();
    auto *hp_vol_title = new QLabel(QStringLiteral("HEADPHONE VOL"), monitor_group);
    hp_vol_title->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #7a8496;"));
    hp_vol_title->setAlignment(Qt::AlignCenter);

    hp_vol_slider_ = new QSlider(Qt::Horizontal, monitor_group);
    hp_vol_slider_->setRange(0, 100);
    hp_vol_slider_->setValue(100);
    hp_vol_slider_->setToolTip(QStringLiteral("Headphone Volume Level"));

    hp_vol_col->addWidget(hp_vol_title);
    hp_vol_col->addWidget(hp_vol_slider_);
    hp_row->addLayout(hp_vol_col);

    connect(hp_mix_slider_, &QSlider::valueChanged, this, [this](int value) {
        if (decks_)
            decks_->setHeadphoneMix(static_cast<float>(value) / 100.0f);
    });

    connect(hp_vol_slider_, &QSlider::valueChanged, this, [this](int value) {
        if (decks_)
            decks_->setHeadphoneVolume(static_cast<float>(value) / 100.0f);
    });

    mon_layout->addLayout(hp_row);
    layout->addWidget(monitor_group);
}

void MixerWindow::updateObsOutputButtons(int mode)
{
    if (decks_)
        decks_->setObsOutputMode(mode);

    if (obs_out_master_btn_) {
        const QSignalBlocker b1(obs_out_master_btn_);
        obs_out_master_btn_->setChecked(mode == 0);
    }
    if (obs_out_deck_a_btn_) {
        const QSignalBlocker b2(obs_out_deck_a_btn_);
        obs_out_deck_a_btn_->setChecked(mode == 1);
    }
    if (obs_out_deck_b_btn_) {
        const QSignalBlocker b3(obs_out_deck_b_btn_);
        obs_out_deck_b_btn_->setChecked(mode == 2);
    }
}

void MixerWindow::refreshDeck(bool is_deck_a)
{
    if (!decks_)
        return;

    DeckPlayer &deck = is_deck_a ? decks_->deckA() : decks_->deckB();

    QLabel *time_label = is_deck_a ? time_a_ : time_b_;
    QLabel *rate_label = is_deck_a ? rate_lbl_a_ : rate_lbl_b_;
    QSlider *pitch_slider = is_deck_a ? pitch_slider_a_ : pitch_slider_b_;
    QPushButton *play_button = is_deck_a ? play_btn_a_ : play_btn_b_;
    QPushButton *hp_cue_button = is_deck_a ? hp_cue_a_btn_ : hp_cue_b_btn_;

    if (time_label) {
        time_label->setText(
            QStringLiteral("%1 / %2")
                .arg(formatTime(deck.currentTimeMs()))
                .arg(formatTime(deck.durationMs())));
    }

    if (rate_label && (!pitch_slider || !pitch_slider->isSliderDown()))
        rate_label->setText(QStringLiteral("%1%").arg(deck.speed()));

    if (pitch_slider && !pitch_slider->isSliderDown() && pitch_slider->value() != deck.speed()) {
        const QSignalBlocker blocker(pitch_slider);
        pitch_slider->setValue(deck.speed());
    }

    if (hp_cue_button && hp_cue_button->isChecked() != deck.isHeadphoneCue()) {
        const QSignalBlocker blocker(hp_cue_button);
        hp_cue_button->setChecked(deck.isHeadphoneCue());
    }

    QSlider *hi_slider = is_deck_a ? hi_a_ : hi_b_;
    QSlider *mid_slider = is_deck_a ? mid_a_ : mid_b_;
    QSlider *low_slider = is_deck_a ? low_a_ : low_b_;
    QSlider *filter_slider = is_deck_a ? flt_a_ : flt_b_;
    QSlider *vol_slider = is_deck_a ? vol_a_ : vol_b_;

    auto updateSliderFromFloat = [](QSlider *slider, float float_val) {
        if (!slider || slider->isSliderDown())
            return;
        const int target_val = std::clamp(
            static_cast<int>(std::lround((float_val - 1.0f) * 100.0f)), -100, 100);
        if (slider->value() != target_val) {
            const QSignalBlocker blocker(slider);
            slider->setValue(target_val);
        }
    };

    updateSliderFromFloat(hi_slider, deck.red());
    updateSliderFromFloat(mid_slider, deck.green());
    updateSliderFromFloat(low_slider, deck.blue());
    updateSliderFromFloat(filter_slider, deck.saturation());

    if (vol_slider && !vol_slider->isSliderDown()) {
        const int vol_val = std::clamp(
            static_cast<int>(std::lround(deck.volume() * 100.0f)), 0, 100);
        if (vol_slider->value() != vol_val) {
            const QSignalBlocker blocker(vol_slider);
            vol_slider->setValue(vol_val);
        }
    }

    if (play_button) {
        play_button->setText(
            deck.isPlaying()
                ? QStringLiteral("⏸ PAUSE")
                : QStringLiteral("▶ PLAY"));
    }

    WaveformWidget *waveform = is_deck_a ? waveform_a_ : waveform_b_;
    if (waveform)
        waveform->refreshWaveform();
}

void MixerWindow::refreshMixer()
{
    if (!decks_)
        return;

    if (crossfader_ && !crossfader_->isSliderDown()) {
        const int value = static_cast<int>(decks_->crossfader() * 1000.0);
        if (crossfader_->value() != value) {
            const QSignalBlocker blocker(crossfader_);
            crossfader_->setValue(value);
        }
    }

    if (midi_status_lbl_) {
        if (vdj_midi_is_connected()) {
            const char *device_name = vdj_midi_get_device_name();
            midi_status_lbl_->setText(
                device_name && *device_name
                    ? QStringLiteral("MIDI: Connected — %1")
                          .arg(QString::fromUtf8(device_name))
                    : QStringLiteral("MIDI: Connected"));
            midi_status_lbl_->setStyleSheet(
                QStringLiteral("color: #2ecc71; font-weight: bold; font-size: 11px;"));
        } else {
            midi_status_lbl_->setText(QStringLiteral("MIDI: Not connected"));
            midi_status_lbl_->setStyleSheet(
                QStringLiteral("color: #e67e22; font-weight: bold; font-size: 11px;"));
        }
    }

    if (audio_monitor_devices_) {
        const char *current_name = nullptr;
        const char *current_id = nullptr;
        obs_get_audio_monitoring_device(&current_name, &current_id);
        if (current_id && *current_id) {
            for (int i = 0; i < audio_monitor_devices_->count(); ++i) {
                if (audio_monitor_devices_->itemData(i).toString() == QString::fromUtf8(current_id)) {
                    if (audio_monitor_devices_->currentIndex() != i) {
                        const QSignalBlocker blocker(audio_monitor_devices_);
                        audio_monitor_devices_->setCurrentIndex(i);
                    }
                    break;
                }
            }
        }
    }
}
