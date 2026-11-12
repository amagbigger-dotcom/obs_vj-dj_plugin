#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

class DeckPlayer;
class WaveformWidget;

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
    void editBpmDialog();

    DeckPlayer *deck_ = nullptr;
    QTimer *refresh_timer_ = nullptr;

    QLabel *file_lbl_ = nullptr;
    QLabel *elapsed_lbl_ = nullptr;
    QLabel *remaining_lbl_ = nullptr;
    QLabel *bpm_lbl_ = nullptr;
    QPushButton *edit_bpm_btn_ = nullptr;
    QPushButton *tap_bpm_btn_ = nullptr;

    WaveformWidget *waveform_widget_ = nullptr;

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
