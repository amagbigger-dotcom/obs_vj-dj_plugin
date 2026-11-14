#pragma once

#include <QDialog>
#include <QWidget>
#include <QSlider>
#include <QMouseEvent>
#include <QProgressBar>
#include <QVBoxLayout>
#include <vector>
#include <obs.h>

class DeckPair;
class DeckPlayer;
class QLabel;
class QPushButton;
class QComboBox;

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

protected:
    void showEvent(QShowEvent *event) override;

private:
    void buildTopBar(QVBoxLayout *main_layout);
    QWidget *buildChannelStrip(bool is_deck_a);
    QWidget *buildCenterMasterPanel();
    void loadAudioDevices();
    void refreshMixer();
    void updateObsOutputButtons(int mode);
    void addMasterToActiveScene();
    void addCueToActiveScene();

    DeckPair *decks_ = nullptr;

    // Deck A Controls
    DJSlider *gain_a_ = nullptr;
    DJSlider *hi_a_ = nullptr;
    DJSlider *mid_a_ = nullptr;
    DJSlider *low_a_ = nullptr;
    DJSlider *flt_a_ = nullptr;
    DJSlider *vol_a_ = nullptr;
    QProgressBar *meter_a_ = nullptr;
    QPushButton *hp_cue_a_btn_ = nullptr;

    // Deck B Controls
    DJSlider *gain_b_ = nullptr;
    DJSlider *hi_b_ = nullptr;
    DJSlider *mid_b_ = nullptr;
    DJSlider *low_b_ = nullptr;
    DJSlider *flt_b_ = nullptr;
    DJSlider *vol_b_ = nullptr;
    QProgressBar *meter_b_ = nullptr;
    QPushButton *hp_cue_b_btn_ = nullptr;

    // Center Master Controls
    DJSlider *crossfader_ = nullptr;
    QComboBox *trans_combo_ = nullptr;
    DJSlider *hp_vol_slider_ = nullptr;
    DJSlider *hp_mix_slider_ = nullptr;
    QComboBox *headphone_device_combo_ = nullptr;

    // Top Bar Controls
    QLabel *midi_status_lbl_ = nullptr;
    QPushButton *obs_out_master_btn_ = nullptr;
    QPushButton *obs_out_deck_a_btn_ = nullptr;
    QPushButton *obs_out_deck_b_btn_ = nullptr;
    QPushButton *add_master_btn_ = nullptr;
    QPushButton *add_cue_btn_ = nullptr;
    QPushButton *tracklist_btn_ = nullptr;
};

void vdj_open_mixer_window();
