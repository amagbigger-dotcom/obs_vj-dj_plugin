#include "waveform-widget.hpp"
#include "deck-engine.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QColor>
#include <QFont>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kOverviewHeight = 14;
constexpr int kHeaderHeight = 32;
constexpr double kZoomSeconds = 8.0; // Seconds visible across the waveform window
}

WaveformWidget::WaveformWidget(DeckPlayer *deck, bool is_deck_a, QWidget *parent)
    : QWidget(parent),
      deck_(deck),
      is_deck_a_(is_deck_a)
{
    setMinimumHeight(130);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(6, 4, 6, 4);
    main_layout->setSpacing(2);

    // Header layout
    auto *header_layout = new QHBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(6);

    file_label_ = new QLabel(QStringLiteral("Empty Deck - Click Load"), this);
    file_label_->setStyleSheet(
        QStringLiteral("color: #a0a6b5; font-size: 11px; font-weight: bold;"));
    header_layout->addWidget(file_label_, 1);

    bpm_label_ = new QLabel(QStringLiteral("128.00 BPM"), this);
    const QString bpm_color = is_deck_a_ ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700");
    bpm_label_->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 13px; font-weight: bold; font-family: monospace; background: #16181f; padding: 2px 8px; border-radius: 3px; border: 1px solid #2a2d36;").arg(bpm_color));
    bpm_label_->setToolTip(QStringLiteral("Track BPM (scales dynamically with pitch fader)"));
    header_layout->addWidget(bpm_label_);

    tap_btn_ = new QPushButton(QStringLiteral("TAP"), this);
    tap_btn_->setFixedSize(36, 22);
    tap_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #222630; color: #ffffff; border: 1px solid #3d4250; border-radius: 3px; font-weight: bold; font-size: 10px; } "
        "QPushButton:hover { background: #2f3442; } "
        "QPushButton:pressed { background: #0088aa; }"));
    tap_btn_->setToolTip(QStringLiteral("Tap along to beat to detect/set BPM"));
    header_layout->addWidget(tap_btn_);

    bpm_minus_btn_ = new QPushButton(QStringLiteral("-"), this);
    bpm_minus_btn_->setFixedSize(22, 22);
    bpm_minus_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #222630; color: #c0c4ce; border: 1px solid #3d4250; border-radius: 3px; font-weight: bold; } "
        "QPushButton:hover { background: #2f3442; color: #fff; }"));
    bpm_minus_btn_->setToolTip(QStringLiteral("Decrease BPM by 0.5"));
    header_layout->addWidget(bpm_minus_btn_);

    bpm_plus_btn_ = new QPushButton(QStringLiteral("+"), this);
    bpm_plus_btn_->setFixedSize(22, 22);
    bpm_plus_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #222630; color: #c0c4ce; border: 1px solid #3d4250; border-radius: 3px; font-weight: bold; } "
        "QPushButton:hover { background: #2f3442; color: #fff; }"));
    bpm_plus_btn_->setToolTip(QStringLiteral("Increase BPM by 0.5"));
    header_layout->addWidget(bpm_plus_btn_);

    main_layout->addLayout(header_layout);
    main_layout->addStretch();

    connect(tap_btn_, &QPushButton::clicked, this, &WaveformWidget::handleTapTempo);
    connect(bpm_minus_btn_, &QPushButton::clicked, this, [this]() { adjustBpm(-0.5); });
    connect(bpm_plus_btn_, &QPushButton::clicked, this, [this]() { adjustBpm(+0.5); });

    tap_timer_.start();
}

void WaveformWidget::updateTrackInfo(const QString &file_name)
{
    if (file_label_) {
        file_label_->setText(file_name.isEmpty() ? QStringLiteral("Empty Deck - Click Load") : file_name);
        file_label_->setStyleSheet(file_name.isEmpty()
            ? QStringLiteral("color: #a0a6b5; font-size: 11px; font-weight: bold;")
            : QStringLiteral("color: #ffffff; font-size: 11px; font-weight: bold;"));
    }
    cached_duration_ms_ = 0;
    waveform_samples_.clear();
    update();
}

void WaveformWidget::adjustBpm(double delta)
{
    if (!deck_)
        return;
    const double current = deck_->bpm();
    const double new_bpm = std::clamp(current + delta, 40.0, 300.0);
    deck_->setBpm(new_bpm);
    emit bpmChanged(new_bpm);
    refreshWaveform();
}

void WaveformWidget::handleTapTempo()
{
    if (!deck_)
        return;

    const qint64 now = tap_timer_.elapsed();
    if (last_tap_time_ms_ > 0) {
        const qint64 delta = now - last_tap_time_ms_;
        // If gap is between 200ms (300 BPM) and 2000ms (30 BPM)
        if (delta >= 200 && delta <= 2000) {
            tap_intervals_.push_back(delta);
            if (tap_intervals_.size() > 8)
                tap_intervals_.erase(tap_intervals_.begin());

            qint64 sum = 0;
            for (qint64 interval : tap_intervals_)
                sum += interval;

            const double avg_ms = static_cast<double>(sum) / static_cast<double>(tap_intervals_.size());
            const double calculated_bpm = std::round((60000.0 / avg_ms) * 10.0) / 10.0;
            deck_->setBpm(calculated_bpm);
            emit bpmChanged(calculated_bpm);
        } else {
            tap_intervals_.clear();
        }
    }
    last_tap_time_ms_ = now;
    refreshWaveform();
}

double WaveformWidget::getEffectiveBpm() const
{
    if (!deck_)
        return 128.0;
    return deck_->effectiveBpm();
}

void WaveformWidget::refreshWaveform()
{
    if (bpm_label_ && deck_) {
        const double base = deck_->bpm();
        const double eff = deck_->effectiveBpm();
        const int spd = deck_->speed();
        if (spd == 100) {
            bpm_label_->setText(QStringLiteral("%1 BPM").arg(base, 0, 'f', 2));
        } else {
            const double diff = static_cast<double>(spd - 100);
            bpm_label_->setText(QStringLiteral("%1 BPM (%2%3%)")
                .arg(eff, 0, 'f', 2)
                .arg(diff >= 0 ? QStringLiteral("+") : QStringLiteral(""))
                .arg(diff, 0, 'f', 1));
        }
    }
    update();
}

void WaveformWidget::ensureWaveformSamples(int64_t duration_ms)
{
    if (duration_ms <= 0) {
        waveform_samples_.clear();
        cached_duration_ms_ = 0;
        return;
    }

    if (cached_duration_ms_ == duration_ms && !waveform_samples_.empty())
        return;

    cached_duration_ms_ = duration_ms;
    constexpr size_t kNumPoints = 400;
    waveform_samples_.resize(kNumPoints);

    // Multi-harmonic synthesized waveform envelope based on track duration
    uint32_t seed = static_cast<uint32_t>(duration_ms);
    for (size_t i = 0; i < kNumPoints; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kNumPoints);
        seed = (seed * 1103515245U + 12345U) & 0x7fffffffU;
        const double noise = static_cast<double>(seed % 1000) / 1000.0;

        // Music energy curve: build-ups, drops, breakdowns
        const double bass = std::sin(t * 3.14159 * 12.0) * 0.25;
        const double mid = std::sin(t * 3.14159 * 48.0) * 0.15;
        const double envelope = 0.35 + 0.35 * std::sin(t * 3.14159) + bass + mid + noise * 0.25;

        waveform_samples_[i] = static_cast<float>(std::clamp(envelope, 0.05, 0.98));
    }
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const int wave_top = kHeaderHeight;
    const int wave_height = h - kHeaderHeight - kOverviewHeight - 4;
    const int wave_bottom = wave_top + wave_height;
    const int center_y = wave_top + wave_height / 2;
    const int playhead_x = w / 2;

    // Background
    p.fillRect(0, wave_top, w, wave_height, QColor(14, 16, 20));

    // Subtle horizontal center baseline
    p.setPen(QPen(QColor(35, 40, 50), 1, Qt::DashLine));
    p.drawLine(0, center_y, w, center_y);

    if (!deck_)
        return;

    const int64_t duration = deck_->durationMs();
    const int64_t current = deck_->currentTimeMs();
    ensureWaveformSamples(duration);

    const double eff_bpm = getEffectiveBpm();
    const double beat_ms = (eff_bpm > 10.0) ? (60000.0 / eff_bpm) : 500.0;
    const double visible_ms = kZoomSeconds * 1000.0;
    const double ms_per_pixel = visible_ms / static_cast<double>(w);

    const int64_t start_ms = current - static_cast<int64_t>(playhead_x * ms_per_pixel);
    const int64_t end_ms = current + static_cast<int64_t>((w - playhead_x) * ms_per_pixel);

    // 1. Draw Loop Region Overlay if active
    if (deck_->isLooping()) {
        const int64_t l_start = deck_->loopStartMs();
        const int64_t l_end = deck_->loopEndMs();
        const int x_start = static_cast<int>((l_start - current) / ms_per_pixel) + playhead_x;
        const int x_end = static_cast<int>((l_end - current) / ms_per_pixel) + playhead_x;

        if (x_end > 0 && x_start < w) {
            const int left = std::max(0, x_start);
            const int right = std::min(w, x_end);
            p.fillRect(left, wave_top, right - left, wave_height, QColor(0, 200, 100, 45));

            p.setPen(QPen(QColor(0, 255, 128, 200), 2));
            if (x_start >= 0 && x_start <= w)
                p.drawLine(x_start, wave_top, x_start, wave_bottom);
            if (x_end >= 0 && x_end <= w)
                p.drawLine(x_end, wave_top, x_end, wave_bottom);
        }
    }

    // 2. Draw Beat Grid Lines
    if (beat_ms > 10.0) {
        const int64_t first_beat = static_cast<int64_t>(std::floor(start_ms / beat_ms)) * static_cast<int64_t>(beat_ms);
        for (int64_t b_ms = first_beat; b_ms <= end_ms; b_ms += static_cast<int64_t>(beat_ms)) {
            if (b_ms < 0)
                continue;
            if (duration > 0 && b_ms > duration)
                break;

            const int bx = static_cast<int>((b_ms - current) / ms_per_pixel) + playhead_x;
            if (bx < 0 || bx >= w)
                continue;

            const int64_t beat_num = static_cast<int64_t>(std::round(b_ms / beat_ms));
            const bool is_bar = (beat_num % 4 == 0);

            if (is_bar) {
                p.setPen(QPen(QColor(255, 255, 255, 150), 1.5));
                p.drawLine(bx, wave_top, bx, wave_bottom);

                // Bar number
                p.setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::Bold));
                p.setPen(QColor(255, 255, 255, 180));
                p.drawText(bx + 3, wave_top + 12, QString::number(beat_num / 4 + 1));
            } else {
                p.setPen(QPen(QColor(150, 160, 180, 50), 1, Qt::DotLine));
                p.drawLine(bx, wave_top + 4, bx, wave_bottom - 4);
            }
        }
    }

    // 3. Draw Scrolling Waveform
    if (duration > 0 && !waveform_samples_.empty()) {
        const QColor theme_main = is_deck_a_ ? QColor(0, 210, 255) : QColor(255, 119, 0);
        const QColor theme_core = is_deck_a_ ? QColor(0, 140, 220) : QColor(200, 80, 0);

        for (int x = 0; x < w; ++x) {
            const int64_t sample_time_ms = current + static_cast<int64_t>((x - playhead_x) * ms_per_pixel);
            if (sample_time_ms < 0 || sample_time_ms >= duration)
                continue;

            const double norm_pos = static_cast<double>(sample_time_ms) / static_cast<double>(duration);
            const size_t sample_idx = static_cast<size_t>(norm_pos * (waveform_samples_.size() - 1));
            const float amp = waveform_samples_[sample_idx];

            const int bar_h = static_cast<int>(amp * (wave_height / 2 - 4));

            // Outer frequency glow
            p.setPen(QPen(theme_core, 1));
            p.drawLine(x, center_y - bar_h, x, center_y + bar_h);

            // Inner high energy core
            if (bar_h > 4) {
                p.setPen(QPen(theme_main, 1));
                p.drawLine(x, center_y - bar_h / 2, x, center_y + bar_h / 2);
            }
        }
    }

    // 4. Center Playhead Line & Triangle Cursor
    p.setPen(QPen(QColor(255, 230, 0), 2));
    p.drawLine(playhead_x, wave_top, playhead_x, wave_bottom);

    // Top & bottom playhead markers
    QPainterPath marker_top;
    marker_top.moveTo(playhead_x - 5, wave_top);
    marker_top.lineTo(playhead_x + 5, wave_top);
    marker_top.lineTo(playhead_x, wave_top + 6);
    marker_top.closeSubpath();
    p.fillPath(marker_top, QColor(255, 230, 0));

    QPainterPath marker_bottom;
    marker_bottom.moveTo(playhead_x - 5, wave_bottom);
    marker_bottom.lineTo(playhead_x + 5, wave_bottom);
    marker_bottom.lineTo(playhead_x, wave_bottom - 6);
    marker_bottom.closeSubpath();
    p.fillPath(marker_bottom, QColor(255, 230, 0));

    // 5. Track Overview Bar (Bottom Progress Strip)
    const int ov_top = h - kOverviewHeight - 2;
    p.fillRect(0, ov_top, w, kOverviewHeight, QColor(20, 23, 29));
    p.setPen(QColor(45, 50, 60));
    p.drawRect(0, ov_top, w - 1, kOverviewHeight - 1);

    if (duration > 0) {
        const double progress = std::clamp(static_cast<double>(current) / static_cast<double>(duration), 0.0, 1.0);
        const int prog_x = static_cast<int>(progress * w);

        const QColor fill_color = is_deck_a_ ? QColor(0, 160, 220, 120) : QColor(220, 100, 0, 120);
        p.fillRect(0, ov_top, prog_x, kOverviewHeight, fill_color);

        // Loop highlight on overview bar
        if (deck_->isLooping()) {
            const int lx_start = static_cast<int>((static_cast<double>(deck_->loopStartMs()) / duration) * w);
            const int lx_end = static_cast<int>((static_cast<double>(deck_->loopEndMs()) / duration) * w);
            p.fillRect(lx_start, ov_top, std::max(2, lx_end - lx_start), kOverviewHeight, QColor(0, 255, 128, 180));
        }

        // Overview playhead pin
        p.setPen(QPen(QColor(255, 255, 255), 2));
        p.drawLine(prog_x, ov_top, prog_x, ov_top + kOverviewHeight);
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        is_dragging_ = true;
        mouseMoveEvent(event);
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!is_dragging_ || !deck_)
        return;

    const int w = width();
    const int h = height();
    const int ov_top = h - kOverviewHeight - 2;

    if (event->pos().y() >= ov_top) {
        // Scrubbing on overview bar
        const double norm = std::clamp(static_cast<double>(event->pos().x()) / static_cast<double>(w), 0.0, 1.0);
        deck_->seek(norm);
        emit seekRequested(norm);
    } else {
        // Scrubbing on main waveform
        const int64_t duration = deck_->durationMs();
        if (duration > 0) {
            const double visible_ms = kZoomSeconds * 1000.0;
            const double ms_per_pixel = visible_ms / static_cast<double>(w);
            const int delta_pixels = event->pos().x() - (w / 2);
            const int64_t target_ms = deck_->currentTimeMs() + static_cast<int64_t>(delta_pixels * ms_per_pixel);
            const double norm = std::clamp(static_cast<double>(target_ms) / static_cast<double>(duration), 0.0, 1.0);
            deck_->seek(norm);
            emit seekRequested(norm);
        }
    }
    update();
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        is_dragging_ = false;
}
