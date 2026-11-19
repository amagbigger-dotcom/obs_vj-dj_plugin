#pragma once

#include <QDialog>
#include <QString>
#include <vector>
#include <string>

class QTabWidget;
class QComboBox;
class QRadioButton;
class QSpinBox;
class QCheckBox;
class QTableWidget;
class QPushButton;
class QLabel;

struct PluginSettingsConfig {
    // Audio
    QString audio_driver = "WASAPI"; // "WASAPI", "ASIO"
    QString asio_device_name = "";
    QString wasapi_device_id = "default";
    int sample_rate = 48000;
    int buffer_size = 256;
    int audio_mode = 0; // 0 = Internal Mixer (Master + CUE), 1 = External Mixer / Split Deck (Deck A -> Ch 1-2, Deck B -> Ch 3-4, pl. Pioneer DJM-350)
    int master_ch_l = 1;
    int master_ch_r = 2;
    int cue_ch_l = 3;
    int cue_ch_r = 4;

    // Video
    int video_resolution = 0; // 0 = 1080p FHD, 1 = 720p HD, 2 = Canvas Match
    int default_scale_mode = 1; // 0 = FIT, 1 = FILL, 2 = STRETCH
    int shader_quality = 0; // 0 = Full (1080p), 1 = Half (540p)

    // MIDI
    QString midi_input_device = "";
    QString midi_output_device = "";
    QString active_preset = "Traktor Kontrol S2 MK3";

    // Language & General
    QString language = "hu"; // "hu", "en", "de", "fr"
    bool auto_save_on_exit = true;
};

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override = default;

    static PluginSettingsConfig &config();
    static void loadSettings();
    static void saveSettings();

private slots:
    void onSaveClicked();
    void onCancelClicked();
    void onAudioDriverChanged(int index);
    void onAudioModeChanged();
    void onMidiPresetChanged(int index);
    void onMidiExportClicked();
    void onMidiImportClicked();
    void onMidiLearnClicked(int row);

private:
    void setupUi();
    void setupAudioTab(QTabWidget *tabs);
    void setupVideoTab(QTabWidget *tabs);
    void setupMidiTab(QTabWidget *tabs);
    void setupGeneralTab(QTabWidget *tabs);

    void populateDevices();
    void applyConfigToUi();
    void readUiToConfig();

    // Audio Controls
    QComboBox *audio_driver_combo_ = nullptr;
    QComboBox *asio_device_combo_ = nullptr;
    QComboBox *wasapi_device_combo_ = nullptr;
    QComboBox *sample_rate_combo_ = nullptr;
    QComboBox *buffer_size_combo_ = nullptr;
    QRadioButton *mode_internal_radio_ = nullptr;
    QRadioButton *mode_external_radio_ = nullptr;
    QLabel *asio_device_label_ = nullptr;
    QLabel *wasapi_device_label_ = nullptr;

    // Video Controls
    QComboBox *video_res_combo_ = nullptr;
    QComboBox *video_scale_combo_ = nullptr;
    QComboBox *shader_quality_combo_ = nullptr;

    // MIDI Controls
    QComboBox *midi_in_combo_ = nullptr;
    QComboBox *midi_out_combo_ = nullptr;
    QComboBox *midi_preset_combo_ = nullptr;
    QTableWidget *midi_learn_table_ = nullptr;

    // General Controls
    QComboBox *lang_combo_ = nullptr;
    QCheckBox *auto_save_check_ = nullptr;
};

void vdj_open_settings_dialog();
void vdj_free_settings_dialog();
