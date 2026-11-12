#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <vector>

class DeckPlayer;
class WaveformCanvas;

class DeckWindow : public QDialog {
    Q_OBJECT

public:
    explicit DeckWindow(DeckPlayer *deck, QWidget *parent = nullptr);
    ~DeckWindow() override;

private slots:
    void onRefresh();

private:
    void setupUi();
    void openFileDialog();

    DeckPlayer *deck_ = nullptr;
    QTimer *refresh_timer_ = nullptr;
    QTimer *pitch_throttle_timer_ = nullptr;
    int pending_throttle_speed_ = 100;

    QLabel *file_lbl_ = nullptr;
    QLabel *elapsed_lbl_ = nullptr;
    QLabel *remaining_lbl_ = nullptr;
    QLineEdit *bpm_edit_ = nullptr;
    QPushButton *tap_bpm_btn_ = nullptr;

    WaveformCanvas *waveform_canvas_ = nullptr;

    // Move / Beatgrid
    QPushButton *grid_nudge_minus_ = nullptr;
    QPushButton *grid_set1_btn_ = nullptr;
    QPushButton *grid_nudge_plus_ = nullptr;
    QLabel *grid_offset_lbl_ = nullptr;

    // Loops
    std::vector<QPushButton *> loop_btns_;

    // Transport
    QPushButton *cue_btn_ = nullptr;
    QPushButton *play_btn_ = nullptr;
    QPushButton *sync_btn_ = nullptr;

    // Tempo
    QSlider *pitch_slider_ = nullptr;
    QLabel *pitch_val_lbl_ = nullptr;
    QPushButton *pitch_reset_btn_ = nullptr;
    QPushButton *bend_minus_btn_ = nullptr;
    QPushButton *bend_plus_btn_ = nullptr;

    // Volume
    QSlider *volume_slider_ = nullptr;

    // Tap tempo helpers
    std::vector<qint64> tap_times_;
};
