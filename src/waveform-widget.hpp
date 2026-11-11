#pragma once

#include <QWidget>
#include <QString>
#include <QElapsedTimer>
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

    QSize sizeHint() const override { return QSize(400, 90); }
    QSize minimumSizeHint() const override { return QSize(300, 80); }

signals:
    void seekRequested(double normalized_pos);
    void gridOffsetChanged(int64_t new_offset_ms);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    DeckPlayer *deck_ = nullptr;
    bool is_deck_a_ = true;

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
