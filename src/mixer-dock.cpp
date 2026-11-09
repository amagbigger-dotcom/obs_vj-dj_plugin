#include "mixer-dock.hpp"
#include "deck-engine.hpp"
#include "midi-handler.hpp"

#include <obs.h>
#include <graphics/graphics.h>

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>

static void preview_draw_callback(void *param, uint32_t cx, uint32_t cy)
{
    UNUSED_PARAMETER(cx);
    UNUSED_PARAMETER(cy);

    auto *source = static_cast<obs_source_t *>(param);
    if (source)
        obs_source_video_render(source);
}

VideoPreviewWidget::VideoPreviewWidget(DeckPair *decks, bool is_master,
                                       bool is_deck_a, QWidget *parent)
    : QWidget(parent),
      decks_(decks),
      is_master_(is_master),
      is_deck_a_(is_deck_a)
{
    setFixedHeight(120);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setStyleSheet(QStringLiteral(
        "background-color: #0b0c0e;"
        "border: 1px solid #2a2d36;"
        "border-radius: 4px;"));
}

VideoPreviewWidget::~VideoPreviewWidget()
{
    destroyDisplay();
}

void VideoPreviewWidget::createDisplay()
{
    if (display_ || !isVisible() || width() <= 1 || height() <= 1)
        return;

    obs_video_info video_info = {};
    if (!obs_get_video_info(&video_info))
        return;

    gs_init_data init_data = {};
    init_data.cx = static_cast<uint32_t>(width());
    init_data.cy = static_cast<uint32_t>(height());
    init_data.format =
        static_cast<enum gs_color_format>(video_info.output_format);
    init_data.zsformat = GS_ZS_NONE;

#if defined(_WIN32) || defined(_WIN64)
    init_data.window.hwnd = reinterpret_cast<HWND>(winId());
#else
    init_data.window.id = reinterpret_cast<void *>(winId());
#endif

    obs_enter_graphics();
    display_ = obs_display_create(&init_data, 0x000000);
    obs_leave_graphics();

    if (display_ && current_source_) {
        obs_enter_graphics();
        obs_display_add_draw_callback(
            display_, preview_draw_callback, current_source_);
        obs_leave_graphics();
    }
}

void VideoPreviewWidget::destroyDisplay()
{
    if (!display_)
        return;

    obs_enter_graphics();

    if (current_source_) {
        obs_display_remove_draw_callback(
            display_, preview_draw_callback, current_source_);
    }

    obs_display_destroy(display_);
    display_ = nullptr;

    obs_leave_graphics();
    current_source_ = nullptr;
}

void VideoPreviewWidget::updateSource(obs_source_t *source)
{
    if (current_source_ == source)
        return;

    if (display_ && current_source_) {
        obs_enter_graphics();
        obs_display_remove_draw_callback(
            display_, preview_draw_callback, current_source_);
        obs_leave_graphics();
    }

    current_source_ = source;

    if (display_ && current_source_) {
        obs_enter_graphics();
        obs_display_add_draw_callback(
            display_, preview_draw_callback, current_source_);
        obs_leave_graphics();
    }
}

void VideoPreviewWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    createDisplay();
}

void VideoPreviewWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    destroyDisplay();
}

void VideoPreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (display_ && width() > 1 && height() > 1) {
        obs_enter_graphics();
        obs_display_resize(
            display_,
            static_cast<uint32_t>(width()),
            static_cast<uint32_t>(height()));
        obs_leave_graphics();
    } else if (!display_) {
        createDisplay();
    }
}

static const char *TRAKTOR_S2_STYLESHEET = R"(
QDialog {
    background-color: #121316;
    color: #e0e2e6;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 12px;
}
QGroupBox {
    background: #1a1c22;
    border: 1px solid #2a2d36;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 12px;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top center;
    padding: 2px 10px;
    border-radius: 3px;
}
QGroupBox#deck_a {
    border: 1px solid #0088aa;
}
QGroupBox#deck_a::title {
    background-color: #007799;
    color: #ffffff;
}
QGroupBox#deck_b {
    border: 1px solid #cc5500;
}
QGroupBox#deck_b::title {
    background-color: #bb4400;
    color: #ffffff;
}
QPushButton {
    background-color: #272a33;
    color: #ffffff;
    border: 1px solid #3c404d;
    border-radius: 4px;
    padding: 6px 12px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: #343844;
}
QPushButton:pressed {
    background-color: #1a1c23;
}
QPushButton#play_btn {
    background-color: #1b3d2b;
    border: 1px solid #2ecc71;
    color: #2ecc71;
    font-size: 13px;
    min-height: 28px;
}
QPushButton#cue_btn {
    background-color: #4a3412;
    border: 1px solid #f39c12;
    color: #f39c12;
    font-size: 13px;
    min-height: 28px;
}
QPushButton#sync_btn {
    background-color: #1c2e42;
    border: 1px solid #3498db;
    color: #3498db;
}
QSlider::groove:horizontal {
    height: 6px;
    background: #252831;
    border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: #0088aa;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #e0e2e6;
    width: 14px;
    margin: -4px 0;
    border-radius: 7px;
}
QSlider::groove:vertical {
    width: 6px;
    background: #252831;
    border-radius: 3px;
}
QSlider::sub-page:vertical {
    background: #3c404d;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: #e0e2e6;
    height: 16px;
    margin: 0 -5px;
    border-radius: 3px;
}
QComboBox {
    background-color: #22252e;
    color: #ffffff;
    border: 1px solid #3a3e4c;
    border-radius: 4px;
    padding: 4px 8px;
}
QLabel {
    color: #9aa0ac;
}
)";

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
    setWindowTitle(QStringLiteral("Traktor S2 VJ Mixer - OBS"));
    resize(1080, 660);
    setStyleSheet(QString::fromUtf8(TRAKTOR_S2_STYLESHEET));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 10, 12, 12);
    main_layout->setSpacing(8);

    auto *status_layout = new QHBoxLayout();
    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Scanning..."), this);
    midi_status_lbl_->setStyleSheet(
        QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px;"));
    status_layout->addWidget(midi_status_lbl_);
    status_layout->addStretch();
    main_layout->addLayout(status_layout);

    auto *root_layout = new QHBoxLayout();
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(10);

    auto *deck_a_group = new QGroupBox(QStringLiteral("DECK A"), this);
    deck_a_group->setObjectName(QStringLiteral("deck_a"));

    auto *mixer_group =
        new QGroupBox(QStringLiteral("TRAKTOR 2-CH MIXER"), this);
    mixer_group->setObjectName(QStringLiteral("mixer_section"));

    auto *deck_b_group = new QGroupBox(QStringLiteral("DECK B"), this);
    deck_b_group->setObjectName(QStringLiteral("deck_b"));

    buildDeckPanel(deck_a_group, true);
    buildMixerSection(mixer_group);
    buildDeckPanel(deck_b_group, false);

    root_layout->addWidget(deck_a_group, 3);
    root_layout->addWidget(mixer_group, 2);
    root_layout->addWidget(deck_b_group, 3);
    main_layout->addLayout(root_layout);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        refreshDeck(true);
        refreshDeck(false);
        refreshMixer();
    });
    timer->start(80);

    refreshMixer();
}

void MixerWindow::buildDeckPanel(QWidget *parent, bool is_deck_a)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    if (!decks_)
        return;

    DeckPlayer *deck =
        is_deck_a ? &decks_->deckA() : &decks_->deckB();

    auto *preview =
        new VideoPreviewWidget(decks_, false, is_deck_a, parent);
    layout->addWidget(preview);

    if (is_deck_a)
        preview_a_ = preview;
    else
        preview_b_ = preview;

    auto *file_row = new QHBoxLayout();
    auto *file_label =
        new QLabel(QStringLiteral("Empty Deck - Click Load"), parent);
    file_label->setStyleSheet(
        QStringLiteral("color: #7d8494; font-size: 11px; font-weight: bold;"));
    file_label->setWordWrap(true);
    file_label->setMaximumHeight(28);

    auto *load_button = new QPushButton(QStringLiteral("LOAD"), parent);
    load_button->setFixedWidth(60);

    file_row->addWidget(file_label, 1);
    file_row->addWidget(load_button);
    layout->addLayout(file_row);

    auto *time_row = new QHBoxLayout();
    auto *time_label = new QLabel(QStringLiteral("00:00 / 00:00"), parent);
    time_label->setStyleSheet(
        QStringLiteral("color: #00d2ff; font-weight: bold; font-family: monospace;"));

    if (is_deck_a)
        time_a_ = time_label;
    else
        time_b_ = time_label;

    time_row->addWidget(new QLabel(QStringLiteral("Position"), parent));
    time_row->addStretch();
    time_row->addWidget(time_label);
    layout->addLayout(time_row);

    auto *seek_slider = new QSlider(Qt::Horizontal, parent);
    seek_slider->setRange(0, 1000);
    layout->addWidget(seek_slider);

    if (is_deck_a)
        seek_a_ = seek_slider;
    else
        seek_b_ = seek_slider;

    auto *middle_row = new QHBoxLayout();

    auto *pitch_layout = new QVBoxLayout();
    auto *rate_label = new QLabel(QStringLiteral("100%"), parent);
    rate_label->setAlignment(Qt::AlignCenter);
    rate_label->setStyleSheet(
        QStringLiteral("font-weight: bold; font-size: 11px;"));

    if (is_deck_a)
        rate_lbl_a_ = rate_label;
    else
        rate_lbl_b_ = rate_label;

    auto *pitch_slider = new QSlider(Qt::Vertical, parent);
    pitch_slider->setRange(50, 150);
    pitch_slider->setValue(100);
    pitch_slider->setFixedHeight(90);

    if (is_deck_a)
        pitch_slider_a_ = pitch_slider;
    else
        pitch_slider_b_ = pitch_slider;

    pitch_layout->addWidget(
        new QLabel(QStringLiteral("TEMPO"), parent), 0, Qt::AlignCenter);
    pitch_layout->addWidget(pitch_slider, 0, Qt::AlignCenter);
    pitch_layout->addWidget(rate_label, 0, Qt::AlignCenter);
    middle_row->addLayout(pitch_layout);

    auto *loop_layout = new QVBoxLayout();
    auto *loop_label = new QLabel(QStringLiteral("LOOP / BEATS"), parent);
    loop_label->setStyleSheet(
        QStringLiteral("font-weight: bold; color: #a0a6b5;"));
    loop_layout->addWidget(loop_label);

    auto *loop_button = new QPushButton(QStringLiteral("AUTO LOOP"), parent);
    loop_button->setCheckable(true);
    loop_layout->addWidget(loop_button);

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

    loop_layout->addWidget(loop_combo);

    auto *reset_pitch_button =
        new QPushButton(QStringLiteral("RESET PITCH"), parent);
    loop_layout->addWidget(reset_pitch_button);
    loop_layout->addStretch();

    middle_row->addLayout(loop_layout);
    layout->addLayout(middle_row);

    auto *transport_row = new QHBoxLayout();

    auto *play_button =
        new QPushButton(QStringLiteral("PLAY / PAUSE"), parent);
    play_button->setObjectName(QStringLiteral("play_btn"));

    if (is_deck_a)
        play_btn_a_ = play_button;
    else
        play_btn_b_ = play_button;

    auto *cue_button = new QPushButton(QStringLiteral("CUE"), parent);
    cue_button->setObjectName(QStringLiteral("cue_btn"));

    auto *sync_button = new QPushButton(QStringLiteral("SYNC"), parent);
    sync_button->setObjectName(QStringLiteral("sync_btn"));

    transport_row->addWidget(play_button, 2);
    transport_row->addWidget(cue_button, 1);
    transport_row->addWidget(sync_button, 1);
    layout->addLayout(transport_row);

    connect(load_button, &QPushButton::clicked, this,
            [this, deck, file_label]() {
                const QString path = QFileDialog::getOpenFileName(
                    this,
                    QStringLiteral("Select Media"),
                    QString(),
                    QStringLiteral(
                        "Video/Audio (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;"
                        "All (*.*)"));

                if (!path.isEmpty()) {
                    deck->loadFile(path);
                    file_label->setText(QFileInfo(path).fileName());
                }
            });

    connect(play_button, &QPushButton::clicked, this,
            [deck]() { deck->togglePlayback(); });

    connect(cue_button, &QPushButton::clicked, this,
            [deck]() { deck->cue(); });

    connect(loop_button, &QPushButton::toggled, this,
            [deck, loop_combo](bool enabled) {
                deck->setLoopBeatIndex(loop_combo->currentIndex());
                deck->setLooping(enabled);
            });

    connect(loop_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [deck, loop_button](int index) {
                deck->setLoopBeatIndex(index);
                if (loop_button->isChecked())
                    deck->setLooping(true);
            });

    connect(pitch_slider, &QSlider::valueChanged, this,
            [deck, rate_label](int value) {
                deck->setSpeed(value);
                rate_label->setText(
                    QStringLiteral("%1%").arg(value));
            });

    connect(reset_pitch_button, &QPushButton::clicked, this,
            [deck, pitch_slider, rate_label]() {
                deck->setSpeed(100);
                pitch_slider->setValue(100);
                rate_label->setText(QStringLiteral("100%"));
            });

    connect(sync_button, &QPushButton::clicked, this,
            [this, is_deck_a]() {
                if (!decks_)
                    return;

                if (is_deck_a)
                    decks_->syncTempoAtoB();
                else
                    decks_->syncTempoBtoA();
            });

    connect(seek_slider, &QSlider::sliderMoved, this,
            [deck](int value) {
                deck->seek(static_cast<double>(value) / 1000.0);
            });
}

void MixerWindow::buildMixerSection(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    if (!decks_)
        return;

    auto *master_preview =
        new VideoPreviewWidget(decks_, true, true, parent);
    layout->addWidget(master_preview);
    preview_master_ = master_preview;

    auto *transition_label =
        new QLabel(QStringLiteral("VIDEO TRANSITION"), parent);
    transition_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(transition_label);

    auto *transition_combo = new QComboBox(parent);
    transition_combo->addItem(QStringLiteral("Linear Fade"), 0);
    transition_combo->addItem(QStringLiteral("Horizontal Wipe"), 1);
    transition_combo->addItem(QStringLiteral("Additive Blend"), 2);
    transition_combo->addItem(QStringLiteral("Hard Cut"), 3);
    layout->addWidget(transition_combo);

    connect(transition_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this, transition_combo](int index) {
                UNUSED_PARAMETER(index);
                if (decks_)
                    decks_->setTransitionMode(
                        transition_combo->currentData().toInt());
            });

    auto *crossfader_label =
        new QLabel(QStringLiteral("CROSSFADER"), parent);
    crossfader_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(crossfader_label);

    auto *crossfader_slider = new QSlider(Qt::Horizontal, parent);
    crossfader_slider->setRange(0, 1000);
    crossfader_slider->setValue(500);
    layout->addWidget(crossfader_slider);
    crossfader_ = crossfader_slider;

    connect(crossfader_slider, &QSlider::valueChanged, this,
            [this](int value) {
                if (decks_)
                    decks_->setCrossfader(
                        static_cast<double>(value) / 1000.0);
            });

    auto *channels = new QHBoxLayout();

    auto createChannelControls =
        [this, parent, channels](bool is_deck_a) {
            auto *channel_layout = new QVBoxLayout();
            const QString deck_name =
                is_deck_a ? QStringLiteral("A") : QStringLiteral("B");

            auto *channel_title =
                new QLabel(QStringLiteral("DECK %1").arg(deck_name), parent);
            channel_title->setAlignment(Qt::AlignCenter);
            channel_layout->addWidget(channel_title);

            auto addControl =
                [parent, channel_layout](const QString &name,
                                        QSlider **stored_slider,
                                        int minimum,
                                        int maximum,
                                        int initial_value,
                                        const QString &tooltip) {
                    auto *control_layout = new QVBoxLayout();

                    auto *label = new QLabel(name, parent);
                    label->setAlignment(Qt::AlignCenter);

                    auto *slider =
                        new QSlider(Qt::Vertical, parent);
                    slider->setRange(minimum, maximum);
                    slider->setValue(initial_value);
                    slider->setFixedHeight(90);
                    slider->setToolTip(tooltip);

                    control_layout->addWidget(label);
                    control_layout->addWidget(slider, 0, Qt::AlignCenter);
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

            addControl(QStringLiteral("HI"), hi_slot, -100, 100, 0,
                       QStringLiteral("EQ high band (UI control)"));
            addControl(QStringLiteral("MID"), mid_slot, -100, 100, 0,
                       QStringLiteral("EQ mid band (UI control)"));
            addControl(QStringLiteral("LOW"), low_slot, -100, 100, 0,
                       QStringLiteral("EQ low band (UI control)"));
            addControl(QStringLiteral("FILTER"), filter_slot, -100, 100, 0,
                       QStringLiteral("Filter (UI control)"));

            QSlider *volume_slider =
                addControl(QStringLiteral("VOL"), volume_slot, 0, 100, 100,
                           QStringLiteral("Deck channel volume"));

            DeckPlayer *deck =
                is_deck_a ? &decks_->deckA() : &decks_->deckB();

            connect(volume_slider, &QSlider::valueChanged, this,
                    [deck](int value) {
                        deck->setVolume(static_cast<float>(value) / 100.0f);
                    });

            channels->addLayout(channel_layout);
        };

    createChannelControls(true);
    createChannelControls(false);
    layout->addLayout(channels);
}

void MixerWindow::refreshDeck(bool is_deck_a)
{
    if (!decks_)
        return;

    DeckPlayer &deck =
        is_deck_a ? decks_->deckA() : decks_->deckB();

    QSlider *seek_slider = is_deck_a ? seek_a_ : seek_b_;
    QLabel *time_label = is_deck_a ? time_a_ : time_b_;
    QLabel *rate_label = is_deck_a ? rate_lbl_a_ : rate_lbl_b_;
    QSlider *pitch_slider =
        is_deck_a ? pitch_slider_a_ : pitch_slider_b_;
    QPushButton *play_button =
        is_deck_a ? play_btn_a_ : play_btn_b_;

    if (seek_slider && !seek_slider->isSliderDown()) {
        const QSignalBlocker blocker(seek_slider);
        seek_slider->setValue(
            static_cast<int>(deck.position() * 1000.0));
    }

    if (time_label) {
        time_label->setText(
            QStringLiteral("%1 / %2")
                .arg(formatTime(deck.currentTimeMs()))
                .arg(formatTime(deck.durationMs())));
    }

    if (rate_label)
        rate_label->setText(QStringLiteral("%1%").arg(deck.speed()));

    if (pitch_slider && pitch_slider->value() != deck.speed()) {
        const QSignalBlocker blocker(pitch_slider);
        pitch_slider->setValue(deck.speed());
    }

    if (play_button) {
        play_button->setText(
            deck.isPlaying()
                ? QStringLiteral("PAUSE")
                : QStringLiteral("PLAY"));
    }

    VideoPreviewWidget *preview =
        is_deck_a ? preview_a_ : preview_b_;
    if (preview)
        preview->updateSource(deck.source());
}

void MixerWindow::refreshMixer()
{
    if (!decks_)
        return;

    if (crossfader_) {
        const int value =
            static_cast<int>(decks_->crossfader() * 1000.0);
        if (crossfader_->value() != value) {
            const QSignalBlocker blocker(crossfader_);
            crossfader_->setValue(value);
        }
    }

    if (preview_master_)
        preview_master_->updateSource(decks_->masterSource());

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
            midi_status_lbl_->setText(
                QStringLiteral("MIDI: Not connected"));
            midi_status_lbl_->setStyleSheet(
                QStringLiteral("color: #e67e22; font-weight: bold; font-size: 11px;"));
        }
    }
}
