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
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>

static void preview_draw_callback(void *param, uint32_t cx, uint32_t cy)
{
    UNUSED_PARAMETER(cx);
    UNUSED_PARAMETER(cy);
    obs_source_t *source = static_cast<obs_source_t *>(param);
    if (source) {
        obs_source_video_render(source);
    }
}

VideoPreviewWidget::VideoPreviewWidget(DeckPair *decks, bool is_master, bool is_deck_a, QWidget *parent)
    : QWidget(parent), decks_(decks), is_master_(is_master), is_deck_a_(is_deck_a)
{
    setFixedHeight(120);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setStyleSheet("background-color: #0b0c0e; border: 1px solid #2a2d36; border-radius: 4px;");
}

VideoPreviewWidget::~VideoPreviewWidget()
{
    destroyDisplay();
}

void VideoPreviewWidget::createDisplay()
{
    if (display_ || !isVisible() || width() <= 1 || height() <= 1)
        return;

    obs_video_info ovi;
    if (!obs_get_video_info(&ovi))
        return;

    gs_init_data gsd = {};
    gsd.cx = width();
    gsd.cy = height();
    gsd.format = static_cast<enum gs_color_format>(ovi.output_format);
    gsd.zsformat = GS_ZS_NONE;
#if defined(_WIN32) || defined(_WIN64)
    gsd.window.hwnd = reinterpret_cast<HWND>(winId());
#else
    gsd.window.id = reinterpret_cast<void *>(winId());
#endif

    obs_enter_graphics();
    display_ = obs_display_create(&gsd, 0x000000);
    obs_leave_graphics();

    if (display_ && current_source_) {
        obs_enter_graphics();
        obs_display_add_draw_callback(display_, preview_draw_callback, current_source_);
        obs_leave_graphics();
    }
}

void VideoPreviewWidget::destroyDisplay()
{
    if (!display_)
        return;

    obs_enter_graphics();
    if (current_source_) {
        obs_display_remove_draw_callback(display_, preview_draw_callback, current_source_);
    }
    obs_display_destroy(display_);
    display_ = nullptr;
    obs_leave_graphics();
    current_source_ = nullptr;
}

void VideoPreviewWidget::updateSource(obs_source_t *src)
{
    if (current_source_ == src)
        return;

    if (display_ && current_source_) {
        obs_enter_graphics();
        obs_display_remove_draw_callback(display_, preview_draw_callback, current_source_);
        obs_leave_graphics();
    }

    current_source_ = src;

    if (display_ && current_source_) {
        obs_enter_graphics();
        obs_display_add_draw_callback(display_, preview_draw_callback, current_source_);
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
        obs_display_resize(display_, width(), height());
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
    font-weight: bold;
    min-height: 28px;
}
QPushButton#play_btn:hover {
    background-color: #27ae60;
    color: #ffffff;
}
QPushButton#cue_btn {
    background-color: #4a3412;
    border: 1px solid #f39c12;
    color: #f39c12;
    font-size: 13px;
    font-weight: bold;
    min-height: 28px;
}
QPushButton#cue_btn:hover {
    background-color: #e67e22;
    color: #ffffff;
}
QPushButton#sync_btn {
    background-color: #1c2e42;
    border: 1px solid #3498db;
    color: #3498db;
    font-weight: bold;
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

QString MixerWindow::formatTime(qint64 ms)
{
    const qint64 total_sec = ms / 1000;
    const qint64 minutes = total_sec / 60;
    const qint64 seconds = total_sec % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("Traktor S2 VJ Mixer - OBS"));
    resize(1080, 660);
    setStyleSheet(TRAKTOR_S2_STYLESHEET);

    auto *main_v_layout = new QVBoxLayout(this);
    main_v_layout->setContentsMargins(12, 10, 12, 12);
    main_v_layout->setSpacing(8);

    auto *status_bar = new QHBoxLayout();
    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Scanning..."), this);
    midi_status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px;"));
    status_bar->addWidget(midi_status_lbl_);
    status_bar->addStretch();
    main_v_layout->addLayout(status_bar);

    auto *root_layout = new QHBoxLayout();
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(10);

    auto *deck_a_group = new QGroupBox(QStringLiteral("DECK A"), this);
    deck_a_group->setObjectName(QStringLiteral("deck_a"));

    auto *mixer_group = new QGroupBox(QStringLiteral("TRAKTOR 2-CH MIXER"), this);
    mixer_group->setObjectName(QStringLiteral("mixer_section"));

    auto *deck_b_group = new QGroupBox(QStringLiteral("DECK B"), this);
    deck_b_group->setObjectName(QStringLiteral("deck_b"));

    buildDeckPanel(deck_a_group, true);
    buildMixerSection(mixer_group);
    buildDeckPanel(deck_b_group, false);

    root_layout->addWidget(deck_a_group, 3);
    root_layout->addWidget(mixer_group, 2);
    root_layout->addWidget(deck_b_group, 3);

    main_v_layout->addLayout(root_layout);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        refreshDeck(true);
        refreshDeck(false);
        refreshMixer();
    });
    timer->start(80);
}

void MixerWindow::buildDeckPanel(QWidget *parent, bool is_deck_a)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    DeckPlayer *deck = is_deck_a ? &decks_->deckA() : &decks_->deckB();

    auto *preview = new VideoPreviewWidget(decks_, false, is_deck_a, parent);
    layout->addWidget(preview);
    if (is_deck_a) preview_a_ = preview;
    else preview_b_ = preview;

    auto *file_row = new QHBoxLayout();
    auto *file_label = new QLabel(QStringLiteral("Empty Deck - Click Load"), parent);
    file_label->setStyleSheet(QStringLiteral("color: #7d8494; font-size: 11px; font-weight: bold;"));
    file_label->setWordWrap(true);
    file_label->setMaximumHeight(28);

    auto *load_btn = new QPushButton(QStringLiteral("LOAD"), parent);
    load_btn->setFixedWidth(60);
    file_row->addWidget(file_label, 1);
    file_row->addWidget(load_btn);
    layout->addLayout(file_row);

    auto *time_row = new QHBoxLayout();
    auto *pos_lbl = new QLabel(QStringLiteral("00:00 / 00:00"), parent);
    pos_lbl->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-family: monospace;"));
    if (is_deck_a) time_a_ = pos_lbl;
    else time_b_ = pos_lbl;
    time_row->addWidget(new QLabel(QStringLiteral("Position"), parent));
    time_row->addStretch();
    time_row->addWidget(pos_lbl);
    layout->addLayout(time_row);

    auto *seek_slider = new QSlider(Qt::Horizontal, parent);
    seek_slider->setRange(0, 1000);
    layout->addWidget(seek_slider);
    if (is_deck_a) seek_a_ = seek_slider;
    else seek_b_ = seek_slider;

    auto *mid_row = new QHBoxLayout();

    auto *pitch_box = new QVBoxLayout();
    auto *rate_lbl = new QLabel(QStringLiteral("0.0%"), parent);
    rate_lbl->setAlignment(Qt::AlignCenter);
    rate_lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    if (is_deck_a) rate_lbl_a_ = rate_lbl;
    else rate_lbl_b_ = rate_lbl;

    auto *pitch_slider = new QSlider(Qt::Vertical, parent);
    pitch_slider->setRange(50, 150);
    pitch_slider->setValue(100);
    pitch_slider->setFixedHeight(90);

    if (is_deck_a) pitch_slider_a_ = pitch_slider;
    else pitch_slider_b_ = pitch_slider;

    pitch_box->addWidget(new QLabel(QStringLiteral("TEMPO"), parent), 0, Qt::AlignCenter);
    pitch_box->addWidget(pitch_slider, 0, Qt::AlignCenter);
    pitch_box->addWidget(rate_lbl, 0, Qt::AlignCenter);
    mid_row->addLayout(pitch_box);

    auto *loop_box = new QVBoxLayout();
    auto *loop_lbl = new QLabel(QStringLiteral("LOOP / BEATS"), parent);
    loop_lbl->setStyleSheet(QStringLiteral("font-weight: bold; color: #a0a6b5;"));
    loop_box->addWidget(loop_lbl);

    auto *loop_btn = new QPushButton(QStringLiteral("AUTO LOOP"), parent);
    loop_btn->setCheckable(true);
    loop_box->addWidget(loop_btn);

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
    if (is_deck_a) loop_combo_a_ = loop_combo;
    else loop_combo_b_ = loop_combo;
    loop_box->addWidget(loop_combo);

    auto *reset_pitch_btn = new QPushButton(QStringLiteral("RESET PITCH"), parent);
    loop_box->addWidget(reset_
