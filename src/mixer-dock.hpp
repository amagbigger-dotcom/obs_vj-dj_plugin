#pragma once

#include <QDialog>
#include <QWidget>
#include <obs.h>

class DeckPair;
class WaveformWidget;
class QSlider;
class QLabel;
class QPushButton;
class QComboBox;

class MixerWindow final : public QDialog {
    Q_OBJECT
public:
    explicit MixerWindow(DeckPair *decks, QWidget *parent = nullptr);
    ~MixerWindow() override = default;

private:
    void buildDeckPanel(QWidget *parent, bool is_deck_a);
    void buildMixerSection(QWidget *parent);
    void loadAudioMonitoringDevices();
    void refreshDeck(bool is_deck_a);
    void refreshMixer();
    void updateObsOutputButtons(int mode);
    static QString formatTime(qint64 ms);

    DeckPair *decks_ = nullptr;
    WaveformWidget *waveform_a_ = nullptr;
    WaveformWidget *waveform_b_ = nullptr;

    QSlider *seek_a_ = nullptr;
    QSlider *seek_b_ = nullptr;
    QLabel *time_a_ = nullptr;
    QLabel *time_b_ = nullptr;
    QPushButton *play_btn_a_ = nullptr;
    QPushButton *play_btn_b_ = nullptr;
    QLabel *rate_lbl_a_ = nullptr;
    QLabel *rate_lbl_b_ = nullptr;
    QSlider *pitch_slider_a_ = nullptr;
    QSlider *pitch_slider_b_ = nullptr;
    QComboBox *loop_combo_a_ = nullptr;
    QComboBox *loop_combo_b_ = nullptr;
    QPushButton *hp_cue_a_btn_ = nullptr;
    QPushButton *hp_cue_b_btn_ = nullptr;

    QSlider *hi_a_ = nullptr;
    QSlider *mid_a_ = nullptr;
    QSlider *low_a_ = nullptr;
    QSlider *flt_a_ = nullptr;
    QSlider *vol_a_ = nullptr;

    QSlider *hi_b_ = nullptr;
    QSlider *mid_b_ = nullptr;
    QSlider *low_b_ = nullptr;
    QSlider *flt_b_ = nullptr;
    QSlider *vol_b_ = nullptr;

    QSlider *crossfader_ = nullptr;
    QLabel *midi_status_lbl_ = nullptr;
    QComboBox *audio_monitor_devices_ = nullptr;

    QPushButton *obs_out_master_btn_ = nullptr;
    QPushButton *obs_out_deck_a_btn_ = nullptr;
    QPushButton *obs_out_deck_b_btn_ = nullptr;

    QSlider *hp_vol_slider_ = nullptr;
    QSlider *hp_mix_slider_ = nullptr;
    QLabel *hp_mix_lbl_ = nullptr;
};
