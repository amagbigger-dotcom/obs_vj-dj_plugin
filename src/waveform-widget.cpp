#include "waveform-widget.hpp"
#include "deck-engine.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QColor>
#include <QFont>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kHeaderHeight = 24;
constexpr int kOverviewHeight = 14;
constexpr double kZoomSeconds = 8.0; // Seconds visible across the waveform window
}

WaveformWidget::WaveformWidget(DeckPlayer *deck, bool is_deck_a, QWidget *parent)
    : QWidget(parent),
      deck_(deck),
      is_deck_a_(is_deck_a)
{
    setFixedHeight(92);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 2, 4, 2);
    main_layout->setSpacing(1);

    // Header layout (Traktor DISP style)
    auto *header_layout = new QHBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(4);

    file_label_ = new QLabel(QStringLiteral("Empty Deck - Click Load"), this);
    file_label_->setStyleSheet(
        QStringLiteral("color: #d0d6e2; font-size: 10px; font-weight: bold; font-family: 'Segoe UI', sans-serif;"));
    header_layout->addWidget(file_label_, 1);

    time_label_ = new QLabel(QStringLiteral("00:00 / -00:00"), this);
    time_label_->setStyleSheet(
        QStringLiteral("color: #00d2ff; font-size: 11px; font-weight: bold; font-family: monospace; background: #0c0f14; padding: 1px 6px; border-radius: 2px; border: 1px solid #1e2430;"));
    header_layout->addWidget(time_label_);

    bpm_label_ = new QLabel(QStringLiteral("128.0 BPM"), this);
    const QString bpm_color = is_deck_a_ ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700");
    bpm_label_->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; font-weight: bold; font-family: monospace; background: #0c0f14; padding: 1px 6px; border-radius: 2px; border: 1px solid #1e2430;").arg(bpm_color));
    bpm_label_->setToolTip(QStringLiteral("Track BPM (scales with pitch fader). Click [edit] to type exact BPM."));
    header_layout->addWidget(bpm_label_);

    edit_bpm_btn_ = new QPushButton(QStringLiteral("edit"), this);
    edit_bpm_btn_->setFixedSize(32, 18);
    edit_bpm_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c222c; color: #a0abbd; border: 1px solid #2e3848; border-radius: 2px; font-size: 9px; font-weight: bold; } "
        "QPushButton:hover { background: #2a3444; color: #fff; }"));
    edit_bpm_btn_->setToolTip(QStringLiteral("Manually edit exact BPM value"));
    header_layout->addWidget(edit_bpm_btn_);

    tap_btn_ = new QPushButton(QStringLiteral("TAP"), this);
    tap_btn_->setFixedSize(32, 18);
    tap_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c222c; color: #ffffff; border: 1px solid #2e3848; border-radius: 2px; font-size: 9px; font-weight: bold; } "
        "QPushButton:hover { background: #2a3444; } "
        "QPushButton:pressed { background: #0088aa; }"));
    tap_btn_->setToolTip(QStringLiteral("Tap along to beat to detect/set BPM"));
    header_layout->addWidget(tap_btn_);

    main_layout->addLayout(header_layout);
    main_layout->addStretch();

    connect(edit_bpm_btn_, &QPushButton::clicked, this, &WaveformWidget::handleEditBpm);
    connect(tap_btn_, &QPushButton::clicked, this, &WaveformWidget::handleTapTempo);

    tap_timer_.start();
}

void WaveformWidget::updateTrackInfo(const QString &file_name)
{
    if (file_label_) {
        file_label_->setText(file_name.isEmpty() ? QStringLiteral("Empty Deck - Click Load") : file_name);
        file_label_->setStyleSheet(file_name.isEmpty()
            ? QStringLiteral("color: #7b8596; font-size: 10px; font-weight: bold;")
            : QStringLiteral("color: #ffffff; font-size: 10px; font-weight: bold;"));
    }
    cached_duration_ms_ = 0;
    fallback_samples_.clear();
    update();
}

void WaveformWidget::handleEditBpm()
{
    if (!deck_)
        return;

    bool ok = false;
    const double current = deck_->bpm();
    const double val = QInputDialog::getDouble(
        this,
        QStringLiteral("Edit BPM"),
        QStringLiteral("Enter exact track BPM:"),
        current, 40.0, 300.0, 2, &ok);

    if (ok) {
        deck_->setBpm(val);
        emit bpmChanged(val);
        refreshWaveform();
    }
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
    if (!deck_)
        return;

    if (bpm_label_) {
        const double base = deck_->bpm();
        const double eff = deck_->effectiveBpm();
        const int spd = deck_->speed();
        if (spd == 100) {
            bpm_label_->setText(QStringLiteral("%1 BPM").arg(base, 0, 'f', 1));
        } else {
            const double diff = static_cast<double>(spd - 100);
            bpm_label_->setText(QStringLiteral("%1 (%2%3%)")
                .arg(eff, 0, 'f', 1)
                .arg(diff >= 0 ? QStringLiteral("+") : QStringLiteral(""))
                .arg(diff, 0, 'f', 0));
        }
    }

    if (time_label_) {
        const int64_t cur = deck_->currentTimeMs();
        const int64_t dur = deck_->durationMs();
        const int64_t rem = std::max<int64_t>(0, dur - cur);

        const int cur_sec = static_cast<int>(cur / 1000);
        const int rem_sec = static_cast<int>(rem / 1000);

        time_label_->setText(QStringLiteral("%1:%2 / -%3:%4")
            .arg(cur_sec / 60, 2, 10, QChar('0'))
            .arg(cur_sec % 60, 2, 10, QChar('0'))
            .arg(rem_sec / 60, 2, 10, QChar('0'))
            .arg(rem_sec % 60, 2, 10, QChar('0')));
    }

    update();
}

void WaveformWidget::ensureWaveformSamples(int64_t duration_ms)
{
    if (duration_ms <= 0) {
        fallback_samples_.clear();
        cached_duration_ms_ = 0;
        return;
    }

    if (cached_duration_ms_ == duration_ms && !fallback_samples_.empty())
        return;

    cached_duration_ms_ = duration_ms;
    constexpr size_t kNumPoints = 400;
    fallback_samples_.resize(kNumPoints);

    uint32_t seed = static_cast<uint32_t>(duration_ms);
    for (size_t i = 0; i < kNumPoints; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kNumPoints);
        seed = (seed * 1103515245U + 12345U) & 0x7fffffffU;
        const double noise = static_cast<double>(seed % 1000) / 1000.0;
        const double bass = std::sin(t * 3.14159 * 16.0) * 0.25;
        const double envelope = 0.35 + 0.35 * std::sin(t * 3.14159) + bass + noise * 0.2;
        fallback_samples_[i] = static_cast<float>(std::clamp(envelope, 0.05, 0.95));
    }
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const int wave_top = kHeaderHeight;
    const int wave_height = h - kHeaderHeight - kOverviewHeight - 2;
    const int wave_bottom = wave_top + wave_height;
    const int center_y = wave_top + wave_height / 2;
    const int playhead_x = w / 2;

    // 1. Dark background for scrolling waveform
    p.fillRect(0, wave_top, w, wave_height, QColor(10, 12, 16));

    // Horizontal baseline
    p.setPen(QPen(QColor(30, 36, 46), 1, Qt::DashLine));
    p.drawLine(0, center_y, w, center_y);

    if (!deck_)
        return;

    const int64_t duration = deck_->durationMs();
    const int64_t current = deck_->currentTimeMs();
    const int64_t grid_offset = deck_->gridOffsetMs();

    // Get samples (real decoded samples preferred, fallback to synthetic)
    std::vector<float> samples = deck_->waveformSamples();
    if (samples.empty()) {
        ensureWaveformSamples(duration);
        samples = fallback_samples_;
    }

    const double eff_bpm = getEffectiveBpm();
    const double beat_ms = (eff_bpm > 10.0) ? (60000.0 / eff_bpm) : 500.0;
    const double visible_ms = kZoomSeconds * 1000.0;
    const double ms_per_pixel = visible_ms / static_cast<double>(w);

    const int64_t start_ms = current - static_cast<int64_t>(playhead_x * ms_per_pixel);
    const int64_t end_ms = current + static_cast<int64_t>((w - playhead_x) * ms_per_pixel);

    // 2. Active Loop Region Overlay
    if (deck_->isLooping()) {
        const int64_t l_start = deck_->loopStartMs();
        const int64_t l_end = deck_->loopEndMs();
        const int x_start = static_cast<int>((l_start - current) / ms_per_pixel) + playhead_x;
        const int x_end = static_cast<int>((l_end - current) / ms_per_pixel) + playhead_x;

        if (x_end > 0 && x_start < w) {
            const int left = std::max(0, x_start);
            const int right = std::min(w, x_end);
            p.fillRect(left, wave_top, right - left, wave_height, QColor(0, 200, 100, 50));

            p.setPen(QPen(QColor(0, 255, 128, 220), 2));
            if (x_start >= 0 && x_start <= w)
                p.drawLine(x_start, wave_top, x_start, wave_bottom);
            if (x_end >= 0 && x_end <= w)
                p.drawLine(x_end, wave_top, x_end, wave_bottom);
        }
    }

    // 3. Beat Grid Lines (anchored at grid_offset)
    if (beat_ms > 10.0) {
        const int64_t rel_start = start_ms - grid_offset;
        const int64_t first_beat_idx = static_cast<int64_t>(std::floor(static_cast<double>(rel_start) / beat_ms));

        for (int64_t b = first_beat_idx; ; ++b) {
            const int64_t b_ms = grid_offset + static_cast<int64_t>(b * beat_ms);
            if (b_ms > end_ms)
                break;
            if (b_ms < 0)
                continue;
            if (duration > 0 && b_ms > duration)
                break;

            const int bx = static_cast<int>((b_ms - current) / ms_per_pixel) + playhead_x;
            if (bx < 0 || bx >= w)
                continue;

            const bool is_bar = (b % 4 == 0);
            if (is_bar) {
                p.setPen(QPen(QColor(255, 255, 255, 160), 1.5));
                p.drawLine(bx, wave_top, bx, wave_bottom);

                // Bar label
                p.setFont(QFont(QStringLiteral("Segoe UI"), 7, QFont::Bold));
                p.setPen(QColor(255, 255, 255, 190));
                p.drawText(bx + 2, wave_top + 9, QString::number(b / 4 + 1));
            } else {
                p.setPen(QPen(QColor(140, 160, 190, 60), 1, Qt::DotLine));
                p.drawLine(bx, wave_top + 2, bx, wave_bottom - 2);
            }
        }
    }

    // 4. Scrolling Waveform
    if (duration > 0 && !samples.empty()) {
        const QColor theme_main = is_deck_a_ ? QColor(0, 210, 255) : QColor(255, 119, 0);
        const QColor theme_core = is_deck_a_ ? QColor(0, 130, 200) : QColor(200, 75, 0);

        for (int x = 0; x < w; ++x) {
            const int64_t sample_time_ms = current + static_cast<int64_t>((x - playhead_x) * ms_per_pixel);
            if (sample_time_ms < 0 || sample_time_ms >= duration)
                continue;

            const double norm_pos = static_cast<double>(sample_time_ms) / static_cast<double>(duration);
            const size_t sample_idx = static_cast<size_t>(norm_pos * (samples.size() - 1));
            const float amp = samples[sample_idx];

            const int bar_h = static_cast<int>(amp * (wave_height / 2 - 2));

            // Outer wave body
            p.setPen(QPen(theme_core, 1));
            p.drawLine(x, center_y - bar_h, x, center_y + bar_h);

            // Inner high energy core
            if (bar_h > 2) {
                p.setPen(QPen(theme_main, 1));
                p.drawLine(x, center_y - bar_h / 2, x, center_y + bar_h / 2);
            }
        }
    }

    // 5. Center Playhead Needle
    p.setPen(QPen(QColor(255, 230, 0), 2));
    p.drawLine(playhead_x, wave_top, playhead_x, wave_bottom);

    QPainterPath marker_top;
    marker_top.moveTo(playhead_x - 4, wave_top);
    marker_top.lineTo(playhead_x + 4, wave_top);
    marker_top.lineTo(playhead_x, wave_top + 5);
    marker_top.closeSubpath();
    p.fillPath(marker_top, QColor(255, 230, 0));

    // 6. Track Overview Bar (Golden/Amber Traktor style strip)
    const int ov_top = h - kOverviewHeight - 1;
    p.fillRect(0, ov_top, w, kOverviewHeight, QColor(14, 17, 22));
    p.setPen(QColor(38, 44, 54));
    p.drawRect(0, ov_top, w - 1, kOverviewHeight - 1);

    if (duration > 0 && !samples.empty()) {
        const double progress = std::clamp(static_cast<double>(current) / static_cast<double>(duration), 0.0, 1.0);
        const int prog_x = static_cast<int>(progress * w);

        // Draw miniature envelope in overview bar
        p.setPen(QColor(212, 160, 23, 160)); // Amber
        const int ov_center_y = ov_top + kOverviewHeight / 2;
        for (int ox = 0; ox < w; ox += 2) {
            const double norm = static_cast<double>(ox) / static_cast<double>(w);
            const size_t s_idx = static_cast<size_t>(norm * (samples.size() - 1));
            const int mini_h = static_cast<int>(samples[s_idx] * (kOverviewHeight / 2 - 1));
            p.drawLine(ox, ov_center_y - mini_h, ox, ov_center_y + mini_h);
        }

        // Played progress tint
        p.fillRect(0, ov_top, prog_x, kOverviewHeight, QColor(255, 180, 0, 40));

        // Loop highlight on overview bar
        if (deck_->isLooping()) {
            const int lx_start = static_cast<int>((static_cast<double>(deck_->loopStartMs()) / duration) * w);
            const int lx_end = static_cast<int>((static_cast<double>(deck_->loopEndMs()) / duration) * w);
            p.fillRect(lx_start, ov_top, std::max(2, lx_end - lx_start), kOverviewHeight, QColor(0, 255, 128, 180));
        }

        // White position pin
        p.setPen(QPen(QColor(255, 255, 255), 2));
        p.drawLine(prog_x, ov_top, prog_x, ov_top + kOverviewHeight);
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    last_mouse_x_ = event->pos().x();

    if (event->button() == Qt::LeftButton) {
        is_dragging_seek_ = true;
        mouseMoveEvent(event);
    } else if (event->button() == Qt::RightButton) {
        is_sliding_grid_ = true;
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!deck_)
        return;

    const int w = width();
    const int h = height();
    const int ov_top = h - kOverviewHeight - 1;
    const int cur_x = event->pos().x();

    if (is_dragging_seek_) {
        if (event->pos().y() >= ov_top) {
            // Seek on overview bar
            const double norm = std::clamp(static_cast<double>(cur_x) / static_cast<double>(w), 0.0, 1.0);
            deck_->seek(norm);
            emit seekRequested(norm);
        } else {
            // Seek on main scrolling waveform
            const int64_t duration = deck_->durationMs();
            if (duration > 0) {
                const double visible_ms = kZoomSeconds * 1000.0;
                const double ms_per_pixel = visible_ms / static_cast<double>(w);
                const int delta_pixels = cur_x - (w / 2);
                const int64_t target_ms = deck_->currentTimeMs() + static_cast<int64_t>(delta_pixels * ms_per_pixel);
                const double norm = std::clamp(static_cast<double>(target_ms) / static_cast<double>(duration), 0.0, 1.0);
                deck_->seek(norm);
                emit seekRequested(norm);
            }
        }
        update();
    } else if (is_sliding_grid_) {
        // Right drag to slide beatgrid!
        const int delta_x = cur_x - last_mouse_x_;
        if (delta_x != 0) {
            const double visible_ms = kZoomSeconds * 1000.0;
            const double ms_per_pixel = visible_ms / static_cast<double>(w);
            const int64_t delta_ms = static_cast<int64_t>(delta_x * ms_per_pixel);
            deck_->nudgeGrid(delta_ms);
            emit gridOffsetChanged(deck_->gridOffsetMs());
            last_mouse_x_ = cur_x;
            update();
        }
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        is_dragging_seek_ = false;
    else if (event->button() == Qt::RightButton)
        is_sliding_grid_ = false;
}
