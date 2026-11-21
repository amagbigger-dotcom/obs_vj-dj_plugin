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
class LoopBar;
class TransportBar;
class TempoControl;

class DeckWindow : public QDialog {
    Q_OBJECT

public:
    explicit DeckWindow(DeckPlayer *deck, QWidget *parent = nullptr);
    ~DeckWindow() override;

    void bindDeck(DeckPlayer *deck);
    void fitTo1080p();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onRefresh();

private:
    void setupUi();
    void openFileDialog();
    void updateDeckButtonStates();
    void applyTheme(bool is_a);

    DeckPlayer *deck_ = nullptr;
    QTimer *refresh_timer_ = nullptr;

    // Kezelőszervek és gombok (UI Controls & Buttons)
    QPushButton *open_btn_ = nullptr;
    QPushButton *tracklist_btn_ = nullptr;

    QLabel *file_lbl_ = nullptr;
    QFrame *time_container_ = nullptr;
    QLabel *elapsed_lbl_ = nullptr;
    QLabel *remaining_lbl_ = nullptr;
    QLineEdit *bpm_edit_ = nullptr;
    QPushButton *bpm_half_btn_ = nullptr;
    QPushButton *bpm_double_btn_ = nullptr;
    QLabel *key_badge_lbl_ = nullptr;
    QPushButton *tap_bpm_btn_ = nullptr;

    WaveformCanvas *waveform_canvas_ = nullptr;

    // Move / Beatgrid
    QPushButton *grid_nudge_minus_ = nullptr;
    QPushButton *grid_nudge_minus_1ms_ = nullptr;
    QPushButton *grid_set1_btn_ = nullptr;
    QPushButton *grid_nudge_plus_1ms_ = nullptr;
    QPushButton *grid_nudge_plus_ = nullptr;
    QLabel *grid_offset_lbl_ = nullptr;
    QPushButton *zoom_out_btn_ = nullptr;
    QLabel *zoom_lbl_ = nullptr;
    QPushButton *zoom_in_btn_ = nullptr;

    // Subsystem Widgets (Loop, Transport, Tempo)
    LoopBar *loop_bar_ = nullptr;
    TransportBar *transport_bar_ = nullptr;
    TempoControl *tempo_control_ = nullptr;

    // Tap tempo helpers
    std::vector<qint64> tap_times_;
};
