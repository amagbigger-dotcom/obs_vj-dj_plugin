#include "waveform-widget.hpp"
#include "deck-engine.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QColor>
#include <QFont>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kOverviewHeight = 32;
constexpr double kDefaultZoomSeconds = 6.0; // 6 seconds visible across width (smoother, natural scrolling rate)
}

// ============================================================================
// WaveformCanvas Implementation
// ============================================================================

WaveformCanvas::WaveformCanvas(DeckPlayer *deck, bool is_deck_a, QWidget *parent)
    : QWidget(parent),
      deck_(deck),
      is_deck_a_(is_deck_a),
      zoom_seconds_(kDefaultZoomSeconds)
{
    setFixedHeight(135);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setToolTip(QStringLiteral("Bal klikk: Pozíció keresés | Görgetés: Waveform Zoom | Shift + Húzás vagy Jobb klikk: Ütemrács"));
}

void WaveformCanvas::setZoomSeconds(double sec)
{
    zoom_seconds_ = std::clamp(sec, 2.0, 16.0);
    emit zoomChanged(zoom_seconds_);
    update();
}

void WaveformCanvas::zoomIn()
{
    setZoomSeconds(zoom_seconds_ - 1.0);
}

void WaveformCanvas::zoomOut()
{
    setZoomSeconds(zoom_seconds_ + 1.0);
}

void WaveformCanvas::wheelEvent(QWheelEvent *event)
{
    const int num_degrees = event->angleDelta().y();
    if (num_degrees > 0) {
        zoomIn();
    } else if (num_degrees < 0) {
        zoomOut();
    }
    event->accept();
}

void WaveformCanvas::refreshCanvas()
{
    update();
}

void WaveformCanvas::ensureFallbackSamples(int64_t duration_ms)
{
    if (duration_ms <= 0) {
        fallback_detailed_.clear();
        fallback_overview_.clear();
        cached_duration_ms_ = 0;
        return;
    }

    if (cached_duration_ms_ == duration_ms && !fallback_detailed_.empty())
        return;

    cached_duration_ms_ = duration_ms;
    const size_t num_detailed = static_cast<size_t>(duration_ms / 10);
    fallback_detailed_.resize(num_detailed, 0.05f);

    uint32_t seed = static_cast<uint32_t>(duration_ms);
    for (size_t i = 0; i < num_detailed; ++i) {
        const double t = static_cast<double>(i) / 100.0; // seconds
        seed = (seed * 1103515245U + 12345U) & 0x7fffffffU;
        const double noise = static_cast<double>(seed % 1000) / 1000.0;

        // Kick drum spike every 0.5s (120 BPM)
        const double beat_phase = std::fmod(t, 0.5);
        double kick = 0.0;
        if (beat_phase < 0.06) {
            kick = std::exp(-beat_phase * 40.0) * 0.85;
        }

        const double envelope = 0.2 + kick + noise * 0.15;
        fallback_detailed_[i] = static_cast<float>(std::clamp(envelope, 0.05, 0.98));
    }

    constexpr size_t kOverviewCount = 400;
    fallback_overview_.resize(kOverviewCount, 0.05f);
    const size_t step = std::max<size_t>(1, num_detailed / kOverviewCount);
    for (size_t b = 0; b < kOverviewCount; ++b) {
        const size_t start = b * step;
        const size_t end = std::min(start + step, num_detailed);
        float m = 0.05f;
        for (size_t j = start; j < end; ++j) {
            m = std::max(m, fallback_detailed_[j]);
        }
        fallback_overview_[b] = m;
    }
}

void WaveformCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const int wave_top = 0;
    const int wave_height = h - kOverviewHeight - 2;
    const int wave_bottom = wave_top + wave_height;
    const int center_y = wave_top + wave_height / 2;
    const int playhead_x = w / 2;

    // 1. Dark Background for scrolling wave
    p.fillRect(0, wave_top, w, wave_height, QColor(8, 10, 14));

    // Center Baseline
    p.setPen(QPen(QColor(26, 32, 42), 1, Qt::DashLine));
    p.drawLine(0, center_y, w, center_y);

    if (!deck_)
        return;

    const int64_t duration = deck_->durationMs();
    const int64_t current = deck_->currentTimeMs();
    const int64_t grid_offset = deck_->gridOffsetMs();

    std::vector<float> detailed = deck_->detailedSamples();
    std::vector<float> overview = deck_->overviewSamples();

    if (detailed.empty()) {
        ensureFallbackSamples(duration);
        detailed = fallback_detailed_;
        overview = fallback_overview_;
    }

    const double eff_bpm = deck_->effectiveBpm();
    const double beat_ms = (eff_bpm > 10.0) ? (60000.0 / eff_bpm) : 500.0;
    const double visible_ms = zoom_seconds_ * 1000.0;
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

    // 3. High-Resolution Scrolling Waveform (Every kick attack clearly visible!)
    if (duration > 0 && !detailed.empty()) {
        const QColor theme_main = is_deck_a_ ? QColor(0, 225, 255) : QColor(255, 128, 0);
        const QColor theme_core = is_deck_a_ ? QColor(0, 110, 180) : QColor(180, 60, 0);
        const QColor theme_kick = is_deck_a_ ? QColor(180, 245, 255) : QColor(255, 220, 150);

        for (int x = 0; x < w; ++x) {
            const int64_t sample_time_ms = current + static_cast<int64_t>((x - playhead_x) * ms_per_pixel);
            if (sample_time_ms < 0 || sample_time_ms >= duration)
                continue;

            // Map sample_time_ms (10ms per sample) to detailed array index
            const size_t sample_idx = static_cast<size_t>(sample_time_ms / 10);
            if (sample_idx >= detailed.size())
                continue;

            const float amp = detailed[sample_idx];
            const int bar_h = static_cast<int>(amp * (wave_height / 2 - 2));

            // Outer transient body
            p.setPen(QPen(theme_core, 1));
            p.drawLine(x, center_y - bar_h, x, center_y + bar_h);

            // Bright core transient spike
            if (bar_h > 4) {
                p.setPen(QPen(theme_main, 1));
                p.drawLine(x, center_y - bar_h * 2 / 3, x, center_y + bar_h * 2 / 3);
            }

            // Punchy kick core highlight
            if (amp > 0.65f) {
                p.setPen(QPen(theme_kick, 1));
                p.drawLine(x, center_y - bar_h / 3, x, center_y + bar_h / 3);
            }
        }
    }

    // 4. Beat Grid Lines (Drawn ON TOP of waveform so every kick 1, 2, 3, 4 is clearly visible!)
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

            const int beat_in_bar = ((b % 4) + 4) % 4; // 0, 1, 2, 3
            if (beat_in_bar == 0) {
                // Downbeat (Bar 1) - Solid bright white bar marker with badge
                p.setPen(QPen(QColor(255, 255, 255, 240), 2.0));
                p.drawLine(bx, wave_top, bx, wave_bottom);

                p.setFont(QFont(QStringLiteral("Segoe UI"), 7, QFont::Bold));
                p.setPen(QColor(255, 255, 255, 255));
                p.drawText(bx + 3, wave_top + 9, QString::number(b / 4 + 1));
            } else {
                // Beats 2, 3, 4 (Every kick!) - High contrast white line with top tick
                p.setPen(QPen(QColor(255, 255, 255, 150), 1.0));
                p.drawLine(bx, wave_top, bx, wave_bottom);

                p.setPen(QPen(QColor(255, 255, 255, 220), 2.0));
                p.drawLine(bx - 1, wave_top, bx + 1, wave_top);
            }
        }
    }

    // 5. Center Playhead Needle with Yellow Pointer Arrow
    p.setPen(QPen(QColor(255, 230, 0), 2));
    p.drawLine(playhead_x, wave_top, playhead_x, wave_bottom);

    QPainterPath marker_top;
    marker_top.moveTo(playhead_x - 4, wave_top);
    marker_top.lineTo(playhead_x + 4, wave_top);
    marker_top.lineTo(playhead_x, wave_top + 5);
    marker_top.closeSubpath();
    p.fillPath(marker_top, QColor(255, 230, 0));

    // 6. Track Overview Progress Strip (Rich Amber/Gold dynamic waveform)
    const int ov_top = h - kOverviewHeight;
    p.fillRect(0, ov_top, w, kOverviewHeight, QColor(10, 13, 18));
    p.setPen(QColor(36, 44, 56));
    p.drawRect(0, ov_top, w - 1, kOverviewHeight - 1);

    if (duration > 0 && !overview.empty()) {
        const double progress = std::clamp(static_cast<double>(current) / static_cast<double>(duration), 0.0, 1.0);
        const int prog_x = static_cast<int>(progress * w);
        const int ov_center_y = ov_top + kOverviewHeight / 2;

        // Draw clear dynamic audio waveform across the full width
        for (int ox = 0; ox < w; ++ox) {
            const double norm = static_cast<double>(ox) / static_cast<double>(w);
            const size_t s_idx = static_cast<size_t>(norm * (overview.size() - 1));
            const float amp = overview[s_idx];
            const int bar_h = static_cast<int>(amp * (kOverviewHeight / 2 - 2));

            // Outer energy contour
            p.setPen(QPen(QColor(160, 105, 15, 200), 1));
            p.drawLine(ox, ov_center_y - bar_h, ox, ov_center_y + bar_h);

            // Bright core transient spike for musical sections & drops
            if (bar_h > 2) {
                p.setPen(QPen(QColor(255, 195, 25, 240), 1));
                p.drawLine(ox, ov_center_y - bar_h * 2 / 3, ox, ov_center_y + bar_h * 2 / 3);
            }

            // High energy / kick spikes
            if (amp > 0.65f) {
                p.setPen(QPen(QColor(255, 245, 160, 255), 1));
                p.drawLine(ox, ov_center_y - bar_h / 3, ox, ov_center_y + bar_h / 3);
            }
        }

        // Played progress tint
        p.fillRect(0, ov_top, prog_x, kOverviewHeight, QColor(255, 180, 0, 45));

        // Active loop segment highlight on overview bar
        if (deck_->isLooping()) {
            const int lx_start = static_cast<int>((static_cast<double>(deck_->loopStartMs()) / duration) * w);
            const int lx_end = static_cast<int>((static_cast<double>(deck_->loopEndMs()) / duration) * w);
            p.fillRect(lx_start, ov_top, std::max(3, lx_end - lx_start), kOverviewHeight, QColor(0, 255, 128, 180));
        }

        // White position needle with top/bottom marker
        p.setPen(QPen(QColor(255, 255, 255), 2));
        p.drawLine(prog_x, ov_top, prog_x, ov_top + kOverviewHeight);
    }
}

void WaveformCanvas::mousePressEvent(QMouseEvent *event)
{
    last_mouse_x_ = event->pos().x();

    const bool is_shift = (event->modifiers() & Qt::ShiftModifier);
    if (event->button() == Qt::RightButton || (event->button() == Qt::LeftButton && is_shift)) {
        is_sliding_grid_ = true;
        is_dragging_seek_ = false;
        setCursor(Qt::SizeHorCursor);
    } else if (event->button() == Qt::LeftButton) {
        is_dragging_seek_ = true;
        is_sliding_grid_ = false;
        mouseMoveEvent(event);
    }
}

void WaveformCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!deck_)
        return;

    const int w = width();
    const int h = height();
    const int ov_top = h - kOverviewHeight;
    const int cur_x = event->pos().x();

    if (is_dragging_seek_) {
        if (event->pos().y() >= ov_top) {
            // Jump on overview bar
            const double norm = std::clamp(static_cast<double>(cur_x) / static_cast<double>(w), 0.0, 1.0);
            deck_->seek(norm);
            emit seekRequested(norm);
        } else {
            // Scrub on scrolling wave
            const int64_t duration = deck_->durationMs();
            if (duration > 0) {
                const double visible_ms = zoom_seconds_ * 1000.0;
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
        // Drag to slide beatgrid!
        const int delta_x = cur_x - last_mouse_x_;
        if (delta_x != 0) {
            const double visible_ms = zoom_seconds_ * 1000.0;
            const double ms_per_pixel = visible_ms / static_cast<double>(w);
            const int64_t delta_ms = static_cast<int64_t>(delta_x * ms_per_pixel);
            deck_->nudgeGrid(delta_ms);
            emit gridOffsetChanged(deck_->gridOffsetMs());
            last_mouse_x_ = cur_x;
            update();
        }
    }
}

void WaveformCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        is_dragging_seek_ = false;
    
    if (event->button() == Qt::RightButton || (event->button() == Qt::LeftButton && is_sliding_grid_)) {
        is_sliding_grid_ = false;
        setCursor(Qt::PointingHandCursor);
    }
}

// ============================================================================
// WaveformWidget Implementation (Header + Canvas)
// ============================================================================

WaveformWidget::WaveformWidget(DeckPlayer *deck, bool is_deck_a, QWidget *parent)
    : QWidget(parent),
      deck_(deck),
      is_deck_a_(is_deck_a)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(2);

    // 1. Dedicated Top DISP Header Frame (Black background, zero overlap with waveform!)
    auto *header_frame = new QFrame(this);
    header_frame->setFixedHeight(32);
    header_frame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #0c0f14; border: 1px solid #1e2532; border-radius: 3px; }"));
    auto *header_layout = new QHBoxLayout(header_frame);
    header_layout->setContentsMargins(6, 2, 6, 2);
    header_layout->setSpacing(6);

    file_label_ = new QLabel(QStringLiteral("Empty Deck - Click Load"), header_frame);
    file_label_->setStyleSheet(
        QStringLiteral("color: #d0d6e2; font-size: 11px; font-weight: bold; border: none; font-family: 'Segoe UI', sans-serif;"));
    header_layout->addWidget(file_label_, 1);

    time_label_ = new QLabel(QStringLiteral("00:00 / -00:00"), header_frame);
    time_label_->setFixedWidth(120);
    time_label_->setAlignment(Qt::AlignCenter);
    time_label_->setStyleSheet(
        QStringLiteral("color: #00d2ff; font-size: 12px; font-weight: bold; font-family: monospace; border: none;"));
    header_layout->addWidget(time_label_);

    bpm_label_ = new QLabel(QStringLiteral("128.0 BPM"), header_frame);
    bpm_label_->setFixedWidth(80);
    bpm_label_->setAlignment(Qt::AlignCenter);
    const QString bpm_color = is_deck_a_ ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700");
    bpm_label_->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; font-weight: bold; font-family: monospace; border: none;").arg(bpm_color));
    bpm_label_->setToolTip(QStringLiteral("Track BPM. Click [edit] to type exact BPM."));
    header_layout->addWidget(bpm_label_);

    edit_bpm_btn_ = new QPushButton(QStringLiteral("edit"), header_frame);
    edit_bpm_btn_->setFixedSize(36, 22);
    edit_bpm_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c222c; color: #a0abbd; border: 1px solid #2e3848; border-radius: 2px; font-size: 10px; font-weight: bold; } "
        "QPushButton:hover { background: #2a3444; color: #fff; }"));
    edit_bpm_btn_->setToolTip(QStringLiteral("Manually edit exact BPM value"));
    header_layout->addWidget(edit_bpm_btn_);

    tap_btn_ = new QPushButton(QStringLiteral("TAP"), header_frame);
    tap_btn_->setFixedSize(36, 22);
    tap_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c222c; color: #ffffff; border: 1px solid #2e3848; border-radius: 2px; font-size: 10px; font-weight: bold; } "
        "QPushButton:hover { background: #2a3444; } "
        "QPushButton:pressed { background: #0088aa; }"));
    tap_btn_->setToolTip(QStringLiteral("Tap along to beat to detect/set BPM"));
    header_layout->addWidget(tap_btn_);

    root_layout->addWidget(header_frame);

    // 2. Waveform Canvas strictly below the header!
    canvas_ = new WaveformCanvas(deck, is_deck_a, this);
    root_layout->addWidget(canvas_);

    connect(edit_bpm_btn_, &QPushButton::clicked, this, &WaveformWidget::handleEditBpm);
    connect(tap_btn_, &QPushButton::clicked, this, &WaveformWidget::handleTapTempo);

    connect(canvas_, &WaveformCanvas::seekRequested, this, &WaveformWidget::seekRequested);
    connect(canvas_, &WaveformCanvas::gridOffsetChanged, this, &WaveformWidget::gridOffsetChanged);

    tap_timer_.start();
}

void WaveformWidget::updateTrackInfo(const QString &file_name)
{
    if (file_label_) {
        file_label_->setText(file_name.isEmpty() ? QStringLiteral("Empty Deck - Click Load") : file_name);
        file_label_->setStyleSheet(file_name.isEmpty()
            ? QStringLiteral("color: #7b8596; font-size: 10px; font-weight: bold; border: none;")
            : QStringLiteral("color: #ffffff; font-size: 10px; font-weight: bold; border: none;"));
    }
    if (canvas_)
        canvas_->refreshCanvas();
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

    if (canvas_)
        canvas_->refreshCanvas();
}
