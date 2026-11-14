#pragma once

#include <QWidget>
#include <QString>
#include <QElapsedTimer>
#include <QWheelEvent>
#include <vector>
#include <cstdint>

class DeckPlayer;
class QLabel;
class QPushButton;

class WaveformCanvas : public QWidget {
    Q_OBJECT
public:
    explicit WaveformCanvas(DeckPlayer *deck, bool is_deck_a, QWidget *parent = nullptr);
    ~WaveformCanvas() override = default;

    void refreshCanvas();
    void setDeck(DeckPlayer *deck, bool is_deck_a = true);

    double zoomSeconds() const noexcept { return zoom_seconds_; }
    void setZoomSeconds(double sec);
    void zoomIn();
    void zoomOut();

    QSize sizeHint() const override { return QSize(400, 135); }
    QSize minimumSizeHint() const override { return QSize(300, 110); }

signals:
    void seekRequested(double normalized_pos);
    void gridOffsetChanged(int64_t new_offset_ms);
    void zoomChanged(double new_zoom_seconds);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    DeckPlayer *deck_ = nullptr;
    bool is_deck_a_ = true;

    double zoom_seconds_ = 6.0; // Default 6.0s (smoother, natural scrolling rate)
    bool is_dragging_seek_ = false;
    bool is_sliding_grid_ = false;
    int last_mouse_x_ = 0;

    void ensureFallbackSamples(int64_t duration_ms);
    std::vector<float> fallback_detailed_;
    std::vector<float> fallback_overview_;
    int64_t cached_duration_ms_ = 0;
};

class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformWidget(DeckPlayer *deck, bool is_deck_a, QWidget *parent = nullptr);
    ~WaveformWidget() override = default;

    void updateTrackInfo(const QString &file_name);
    void refreshWaveform();

signals:
    void bpmChanged(double new_bpm);
    void seekRequested(double normalized_pos);
    void gridOffsetChanged(int64_t new_offset_ms);

private:
    void handleTapTempo();
    void handleEditBpm();

    DeckPlayer *deck_ = nullptr;
    bool is_deck_a_ = true;

    QLabel *file_label_ = nullptr;
    QLabel *time_label_ = nullptr;
    QLabel *bpm_label_ = nullptr;
    QPushButton *edit_bpm_btn_ = nullptr;
    QPushButton *tap_btn_ = nullptr;

    WaveformCanvas *canvas_ = nullptr;

    QElapsedTimer tap_timer_;
    std::vector<qint64> tap_intervals_;
    qint64 last_tap_time_ms_ = 0;
};
