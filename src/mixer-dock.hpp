#pragma once

#include <QDialog>
#include <QWidget>
#include <QSlider>
#include <QMouseEvent>
#include <QProgressBar>
#include <QVBoxLayout>
#include <vector>
#include <chrono>
#include <obs.h>

class DeckPair;
class DeckPlayer;
class QLabel;
class QPushButton;

// Custom slider that resets to its default value on double-click
class DJSlider : public QSlider {
    Q_OBJECT
public:
    explicit DJSlider(Qt::Orientation orientation, int default_val, QWidget *parent = nullptr)
        : QSlider(orientation, parent), default_val_(default_val) {}

    void setDefaultValue(int val) { default_val_ = val; }
    int defaultValue() const { return default_val_; }

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            setValue(default_val_);
            emit sliderMoved(default_val_);
            event->accept();
            return;
        }
        QSlider::mouseDoubleClickEvent(event);
    }

private:
    int default_val_ = 0;
};

class MixerWindow final : public QDialog {
    Q_OBJECT
public:
    explicit MixerWindow(DeckPair *decks, QWidget *parent = nullptr);
    ~MixerWindow() override = default;

private:
    QWidget *buildChannelStrip(bool is_deck_a);
    QWidget *buildCenterMasterPanel();
    void refreshMixer();

    DeckPair *decks_ = nullptr;

    // Deck A Controls
    DJSlider *gain_a_ = nullptr;
    DJSlider *hi_a_ = nullptr;
    DJSlider *mid_a_ = nullptr;
    DJSlider *low_a_ = nullptr;
    DJSlider *flt_a_ = nullptr;
    DJSlider *vol_a_ = nullptr;
    QLabel *gain_val_a_ = nullptr;
    QLabel *hi_val_a_ = nullptr;
    QLabel *mid_val_a_ = nullptr;
    QLabel *low_val_a_ = nullptr;
    QLabel *flt_val_a_ = nullptr;
    QLabel *vol_val_a_ = nullptr;
    QProgressBar *meter_a_ = nullptr;
    QPushButton *hp_cue_a_btn_ = nullptr;

    // Deck B Controls
    DJSlider *gain_b_ = nullptr;
    DJSlider *hi_b_ = nullptr;
    DJSlider *mid_b_ = nullptr;
    DJSlider *low_b_ = nullptr;
    DJSlider *flt_b_ = nullptr;
    DJSlider *vol_b_ = nullptr;
    QLabel *gain_val_b_ = nullptr;
    QLabel *hi_val_b_ = nullptr;
    QLabel *mid_val_b_ = nullptr;
    QLabel *low_val_b_ = nullptr;
    QLabel *flt_val_b_ = nullptr;
    QLabel *vol_val_b_ = nullptr;
    QProgressBar *meter_b_ = nullptr;
    QPushButton *hp_cue_b_btn_ = nullptr;

    // Center Master Controls
    QPushButton *master_clock_btn_ = nullptr;
    QPushButton *master_clock_ext_btn_ = nullptr;
    QPushButton *master_clock_send_btn_ = nullptr;
    QLabel *master_clock_bpm_lbl_ = nullptr;
    QPushButton *master_clock_tap_btn_ = nullptr;
    QPushButton *master_clock_minus_btn_ = nullptr;
    QPushButton *master_clock_plus_btn_ = nullptr;
    std::vector<std::chrono::steady_clock::time_point> master_tap_times_;

    DJSlider *crossfader_ = nullptr;
    QLabel *cf_val_lbl_ = nullptr;
    DJSlider *hp_vol_slider_ = nullptr;
    QLabel *hp_vol_val_lbl_ = nullptr;
    DJSlider *hp_mix_slider_ = nullptr;
    QLabel *hp_mix_val_lbl_ = nullptr;

    // Bottom Bar Controls
    QLabel *midi_status_lbl_ = nullptr;
    QLabel *bottom_bpm_lbl_ = nullptr;
};

void vdj_open_mixer_window();
