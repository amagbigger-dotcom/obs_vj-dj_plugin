#include "widget/tempo_control.hpp"
#include "engine/deck_engine.hpp"

#include <cmath>
#include <algorithm>
#include <QSignalBlocker>

extern DeckPair *vdj_decks();

TempoControl::TempoControl(QWidget *parent)
    : QWidget(parent)
{
    pitch_throttle_timer_ = new QTimer(this);
    pitch_throttle_timer_->setSingleShot(true);
    connect(pitch_throttle_timer_, &QTimer::timeout, this, [this]() {
        if (deck_) {
            deck_->applySpeedSettings(pending_throttle_speed_);
        }
    });

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    tempo_hdr_ = new QLabel(QStringLiteral("TEMPO:"), this);
    tempo_hdr_->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 10px;"));
    layout->addWidget(tempo_hdr_);

    bend_minus_btn_ = new QPushButton(QStringLiteral("−"), this);
    bend_minus_btn_->setFixedSize(28, 24);
    bend_minus_btn_->setAutoDefault(false);
    bend_minus_btn_->setDefault(false);
    bend_minus_btn_->setAutoRepeat(true);
    bend_minus_btn_->setAutoRepeatDelay(350);
    bend_minus_btn_->setAutoRepeatInterval(100);
    bend_minus_btn_->setToolTip(QStringLiteral("Tempo léptetés: első klikk kerekít tizedre, utána −0.10 BPM"));
    connect(bend_minus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        const double cur_eff = deck_->effectiveBpm();
        double target_bpm = 0.0;
        if (std::abs(cur_eff - (std::round(cur_eff * 10.0) / 10.0)) < 0.005) {
            target_bpm = (std::round(cur_eff * 10.0) - 1.0) / 10.0;
        } else {
            target_bpm = std::floor(cur_eff * 10.0) / 10.0;
        }
        target_bpm = std::round(target_bpm * 100.0) / 100.0;
        deck_->setPlaybackBpm(target_bpm);
        updateLabelsAndSlider();
    });
    layout->addWidget(bend_minus_btn_);

    pitch_slider_ = new QSlider(Qt::Horizontal, this);
    pitch_slider_->setRange(50, 150);
    pitch_slider_->setValue(100);
    pitch_slider_->setTracking(true);

    connect(pitch_slider_, &QSlider::valueChanged, this, [this](int val) {
        if (deck_) {
            if (deck_->isSync()) {
                deck_->setSync(false);
                DeckPair *dp = vdj_decks();
                if (dp) {
                    const int ch = (deck_->side() == DeckPlayer::DeckSide::B) ? 1 : 0;
                    dp->setSync(ch, false);
                }
            }
            deck_->setSpeed(val);
        }
        const double eff_bpm = deck_ ? deck_->effectiveBpm() : 128.0;
        const double base_bpm = (deck_ && deck_->bpm() > 1.0) ? deck_->bpm() : 128.0;
        const double diff_pct = (eff_bpm / base_bpm - 1.0) * 100.0;
        pitch_val_lbl_->setText(QString::asprintf("%.2f BPM (%+.1f%%)", eff_bpm, diff_pct));

        // Debounced speed update: reset timer on every move so FFmpeg only reloads once user pauses/settles
        pending_throttle_speed_ = val;
        pitch_throttle_timer_->start(350);
    });

    connect(pitch_slider_, &QSlider::sliderReleased, this, [this]() {
        pitch_throttle_timer_->stop();
        if (deck_ && pitch_slider_) {
            deck_->applySpeedSettings(pitch_slider_->value());
        }
    });
    layout->addWidget(pitch_slider_, 1);

    bend_plus_btn_ = new QPushButton(QStringLiteral("+"), this);
    bend_plus_btn_->setFixedSize(28, 24);
    bend_plus_btn_->setAutoDefault(false);
    bend_plus_btn_->setDefault(false);
    bend_plus_btn_->setAutoRepeat(true);
    bend_plus_btn_->setAutoRepeatDelay(350);
    bend_plus_btn_->setAutoRepeatInterval(100);
    bend_plus_btn_->setToolTip(QStringLiteral("Tempo léptetés: első klikk kerekít tizedre, utána +0.10 BPM"));
    connect(bend_plus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        const double cur_eff = deck_->effectiveBpm();
        double target_bpm = 0.0;
        if (std::abs(cur_eff - (std::round(cur_eff * 10.0) / 10.0)) < 0.005) {
            target_bpm = (std::round(cur_eff * 10.0) + 1.0) / 10.0;
        } else {
            target_bpm = std::ceil(cur_eff * 10.0) / 10.0;
        }
        target_bpm = std::round(target_bpm * 100.0) / 100.0;
        deck_->setPlaybackBpm(target_bpm);
        updateLabelsAndSlider();
    });
    layout->addWidget(bend_plus_btn_);

    pitch_val_lbl_ = new QLabel(QStringLiteral("128.00 BPM (+0.0%)"), this);
    pitch_val_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-family: monospace; font-weight: bold; font-size: 11px;"));
    pitch_val_lbl_->setFixedWidth(135);
    layout->addWidget(pitch_val_lbl_);

    pitch_reset_btn_ = new QPushButton(QStringLiteral("0% RESET"), this);
    pitch_reset_btn_->setFixedSize(60, 24);
    pitch_reset_btn_->setAutoDefault(false);
    pitch_reset_btn_->setDefault(false);
    pitch_reset_btn_->setStyleSheet(QStringLiteral("background: #2a3444; border-color: #48566e; color: #cfd6e4; font-size: 10px; padding: 0px;"));
    connect(pitch_reset_btn_, &QPushButton::clicked, this, [this]() {
        if (pitch_slider_) pitch_slider_->setValue(100);
        if (deck_) {
            deck_->applySpeedSettings(100);
            deck_->setPlaybackBpm(deck_->bpm());
        }
        updateLabelsAndSlider();
    });
    layout->addWidget(pitch_reset_btn_);
}

TempoControl::~TempoControl()
{
}

void TempoControl::bindDeck(DeckPlayer *deck)
{
    deck_ = deck;
    updateLabelsAndSlider();
}

void TempoControl::updateLabelsAndSlider()
{
    if (!deck_) return;
    const double eff_bpm = deck_->effectiveBpm();
    const double base_bpm = (deck_->bpm() > 1.0) ? deck_->bpm() : 128.0;
    const double diff_pct = (eff_bpm / base_bpm - 1.0) * 100.0;
    pitch_val_lbl_->setText(QString::asprintf("%.2f BPM (%+.1f%%)", eff_bpm, diff_pct));
    if (pitch_slider_) {
        QSignalBlocker b(pitch_slider_);
        pitch_slider_->setValue(deck_->speed());
    }
}

void TempoControl::applyTheme(bool is_a)
{
    if (is_a) {
        if (pitch_slider_) {
            pitch_slider_->setStyleSheet(QStringLiteral(
                "QSlider::groove:horizontal { height: 6px; background: #08101a; border: 1px solid #152c42; border-radius: 3px; } "
                "QSlider::handle:horizontal { background: #00d2ff; width: 18px; margin: -5px 0; border-radius: 3px; border: 1px solid #ffffff; } "
                "QSlider::handle:horizontal:hover { background: #38e0ff; }"));
        }
        if (bend_minus_btn_) {
            bend_minus_btn_->setStyleSheet(QStringLiteral(
                "QPushButton { background: #101e30; border: 1px solid #1c3756; border-radius: 3px; color: #00d2ff; font-size: 13px; font-weight: bold; padding: 0px; } "
                "QPushButton:hover { background: #192f4c; border-color: #00d2ff; color: #ffffff; } "
                "QPushButton:pressed { background: #0088cc; color: #ffffff; }"));
        }
        if (bend_plus_btn_) {
            bend_plus_btn_->setStyleSheet(QStringLiteral(
                "QPushButton { background: #101e30; border: 1px solid #1c3756; border-radius: 3px; color: #00d2ff; font-size: 13px; font-weight: bold; padding: 0px; } "
                "QPushButton:hover { background: #192f4c; border-color: #00d2ff; color: #ffffff; } "
                "QPushButton:pressed { background: #0088cc; color: #ffffff; }"));
        }
        if (pitch_val_lbl_) {
            pitch_val_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-family: monospace; font-weight: bold; font-size: 11px;"));
        }
    } else {
        if (pitch_slider_) {
            pitch_slider_->setStyleSheet(QStringLiteral(
                "QSlider::groove:horizontal { height: 6px; background: #140b05; border: 1px solid #3b200e; border-radius: 3px; } "
                "QSlider::handle:horizontal { background: #ff8800; width: 18px; margin: -5px 0; border-radius: 3px; border: 1px solid #ffffff; } "
                "QSlider::handle:horizontal:hover { background: #ffa538; }"));
        }
        if (bend_minus_btn_) {
            bend_minus_btn_->setStyleSheet(QStringLiteral(
                "QPushButton { background: #2c1a0c; border: 1px solid #4f2e15; border-radius: 3px; color: #ff8800; font-size: 13px; font-weight: bold; padding: 0px; } "
                "QPushButton:hover { background: #442813; border-color: #ff8800; color: #ffffff; } "
                "QPushButton:pressed { background: #cc5500; color: #ffffff; }"));
        }
        if (bend_plus_btn_) {
            bend_plus_btn_->setStyleSheet(QStringLiteral(
                "QPushButton { background: #2c1a0c; border: 1px solid #4f2e15; border-radius: 3px; color: #ff8800; font-size: 13px; font-weight: bold; padding: 0px; } "
                "QPushButton:hover { background: #442813; border-color: #ff8800; color: #ffffff; } "
                "QPushButton:pressed { background: #cc5500; color: #ffffff; }"));
        }
        if (pitch_val_lbl_) {
            pitch_val_lbl_->setStyleSheet(QStringLiteral("color: #ff8800; font-family: monospace; font-weight: bold; font-size: 11px;"));
        }
    }
}

void TempoControl::refresh()
{
    if (!deck_) return;
    if (pitch_slider_ && !pitch_slider_->isSliderDown()) {
        const int spd = deck_->speed();
        if (pitch_slider_->value() != spd) {
            QSignalBlocker b(pitch_slider_);
            pitch_slider_->setValue(spd);
        }
        const double eff_bpm = deck_->effectiveBpm();
        const double base_bpm = (deck_->bpm() > 1.0) ? deck_->bpm() : 128.0;
        const double diff_pct = (eff_bpm / base_bpm - 1.0) * 100.0;
        pitch_val_lbl_->setText(QString::asprintf("%.2f BPM (%+.1f%%)", eff_bpm, diff_pct));
    }
}
