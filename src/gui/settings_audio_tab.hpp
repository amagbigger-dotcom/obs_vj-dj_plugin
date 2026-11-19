#pragma once

#include <QWidget>
#include <QString>
#include <vector>

struct PluginSettingsConfig;
class QComboBox;
class QRadioButton;
class QLabel;

class SettingsAudioTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsAudioTab(QWidget *parent = nullptr);
    ~SettingsAudioTab() override = default;

    void populateDevices();
    void applyConfig(const PluginSettingsConfig &cfg);
    void readConfig(PluginSettingsConfig &cfg);

private slots:
    void onAudioDriverChanged(int index);

private:
    void setupUi();
    static void populateChannelCombo(QComboBox *combo, int selected_ch, int max_ch = 16);

    // Audio Driver & Hardware
    QComboBox *audio_driver_combo_ = nullptr;
    QComboBox *asio_device_combo_ = nullptr;
    QComboBox *wasapi_device_combo_ = nullptr;
    QComboBox *sample_rate_combo_ = nullptr;
    QComboBox *buffer_size_combo_ = nullptr;
    QRadioButton *mode_internal_radio_ = nullptr;
    QRadioButton *mode_external_radio_ = nullptr;
    QLabel *asio_device_label_ = nullptr;
    QLabel *wasapi_device_label_ = nullptr;

    // Output Routing
    QComboBox *master_dev_combo_ = nullptr;
    QComboBox *master_ch_l_combo_ = nullptr;
    QComboBox *master_ch_r_combo_ = nullptr;

    QComboBox *cue_dev_combo_ = nullptr;
    QComboBox *cue_ch_l_combo_ = nullptr;
    QComboBox *cue_ch_r_combo_ = nullptr;

    QComboBox *booth_dev_combo_ = nullptr;
    QComboBox *booth_ch_l_combo_ = nullptr;
    QComboBox *booth_ch_r_combo_ = nullptr;

    QComboBox *deck_a_out_dev_combo_ = nullptr;
    QComboBox *deck_a_out_ch_l_combo_ = nullptr;
    QComboBox *deck_a_out_ch_r_combo_ = nullptr;

    QComboBox *deck_b_out_dev_combo_ = nullptr;
    QComboBox *deck_b_out_ch_l_combo_ = nullptr;
    QComboBox *deck_b_out_ch_r_combo_ = nullptr;

    // Input Routing
    QComboBox *deck_a_in_dev_combo_ = nullptr;
    QComboBox *deck_a_in_ch_l_combo_ = nullptr;
    QComboBox *deck_a_in_ch_r_combo_ = nullptr;

    QComboBox *deck_b_in_dev_combo_ = nullptr;
    QComboBox *deck_b_in_ch_l_combo_ = nullptr;
    QComboBox *deck_b_in_ch_r_combo_ = nullptr;

    QComboBox *aux_in_dev_combo_ = nullptr;
    QComboBox *aux_in_ch_l_combo_ = nullptr;
    QComboBox *aux_in_ch_r_combo_ = nullptr;
};
