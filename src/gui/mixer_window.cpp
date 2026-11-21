#include "gui/mixer_window.hpp"
#include "gui/mixer_channel.hpp"
#include "gui/mixer_master.hpp"
#include "gui/vj_fx_dialog.hpp"
#include "gui/settings_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "controllers/midi_handler.hpp"

#include <obs-frontend-api.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <algorithm>

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
    g_mixer_window->applyWindowPosition();
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

MixerWindow *vdj_get_mixer_window()
{
    return g_mixer_window;
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
    setFixedSize(590, 340);
    setAttribute(Qt::WA_Moved, true);
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

    channel_a_ = new MixerChannelStrip(true, vj_deck_a(), this);
    master_panel_ = new MixerMasterPanel(decks_, this);
    channel_b_ = new MixerChannelStrip(false, vj_deck_b(), this);

    mixer_row->addWidget(channel_a_, 4);
    mixer_row->addWidget(master_panel_, 5);
    mixer_row->addWidget(channel_b_, 4);

    main_layout->addLayout(mixer_row, 1);

    // 2. Bottom Bar: MIDI Status, VJ Visualizer FX and Master BPM
    auto *bottom_row = new QHBoxLayout();
    bottom_row->setContentsMargins(4, 0, 4, 0);

    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Initializing..."), this);
    midi_status_lbl_->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #7f8fa4;"));
    bottom_row->addWidget(midi_status_lbl_);
    bottom_row->addStretch(1);

    auto *settings_btn = new QPushButton(QStringLiteral("⚙ BEÁLLÍTÁSOK"), this);
    settings_btn->setFixedHeight(20);
    settings_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #182230; border: 1px solid #2d3e56; color: #38bdf8; font-weight: bold; font-size: 9px; padding: 1px 8px; border-radius: 3px; } "
        "QPushButton:hover { background: #0284c7; color: #ffffff; border-color: #38bdf8; }"));
    settings_btn->setToolTip(QStringLiteral("Rendszerbeállítások: Hangkártya, ASIO, Külső keverő, MIDI Learn & Nyelvkezelés"));
    connect(settings_btn, &QPushButton::clicked, this, []() {
        vdj_open_settings_dialog();
    });
    bottom_row->addWidget(settings_btn);
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

void MixerWindow::refreshMixer()
{
    if (!decks_)
        return;

    decks_->updateSync();

    if (bottom_bpm_lbl_) {
        bottom_bpm_lbl_->setText(QString::asprintf("MASTER %.2f BPM", decks_->effectiveMasterBpm()));
    }

    if (channel_a_) {
        channel_a_->refresh();
    }
    if (master_panel_) {
        master_panel_->refresh();
    }
    if (channel_b_) {
        channel_b_->refresh();
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

void MixerWindow::applyWindowPosition()
{
    setFixedSize(590, 340);
    const auto &cfg = SettingsDialog::config();
    int x_pos = cfg.win_mixer_x;
    int y_pos = cfg.win_mixer_y;

    if (x_pos < 0 || y_pos < 0 || (x_pos == 655 && y_pos == 317)) {
        x_pos = 660;
        y_pos = 654;
    }
    move(x_pos, y_pos);
}

void MixerWindow::showEvent(QShowEvent *event)
{
    applyWindowPosition();
    QDialog::showEvent(event);
}

void MixerWindow::keyPressEvent(QKeyEvent *event)
{
    if (master_panel_ && master_panel_->handleKeyPress(event)) {
        return;
    }
    QDialog::keyPressEvent(event);
}
