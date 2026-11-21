#pragma once

#include <QDialog>
#include <QString>

class QTabWidget;
class QPushButton;
class SettingsAudioTab;
class SettingsVideoTab;
class SettingsMidiTab;
class SettingsGeneralTab;

struct PluginSettingsConfig {
    // Audio Driver & Hardware
    QString audio_driver = "WASAPI"; // "WASAPI", "ASIO"
    QString asio_device_name = "";
    QString wasapi_device_id = "default";
    int sample_rate = 48000;
    int buffer_size = 256;
    int audio_mode = 0; // 0 = Internal Mixer (Master + CUE), 1 = External Mixer / Split Deck (Deck A -> Ch 1-2, Deck B -> Ch 3-4)

    // Output Routing
    QString master_out_dev = "default";
    int master_out_ch_l = 1;
    int master_out_ch_r = 2;

    QString cue_out_dev = "default";
    int cue_out_ch_l = 3;
    int cue_out_ch_r = 4;

    QString booth_out_dev = "";
    int booth_out_ch_l = 5;
    int booth_out_ch_r = 6;

    QString deck_a_out_dev = "";
    int deck_a_out_ch_l = 1;
    int deck_a_out_ch_r = 2;

    QString deck_b_out_dev = "";
    int deck_b_out_ch_l = 3;
    int deck_b_out_ch_r = 4;

    // Input Routing
    QString deck_a_in_dev = "";
    int deck_a_in_ch_l = 1;
    int deck_a_in_ch_r = 2;

    QString deck_b_in_dev = "";
    int deck_b_in_ch_l = 3;
    int deck_b_in_ch_r = 4;

    QString aux_in_dev = "";
    int aux_in_ch_l = 1;
    int aux_in_ch_r = 2;

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

    // Window Positions (-1 = uninitialized / use default layout)
    int win_deck_a_x = -1;
    int win_deck_a_y = -1;
    int win_deck_b_x = -1;
    int win_deck_b_y = -1;
    int win_mixer_x = -1;
    int win_mixer_y = -1;
    int win_tracklist_x = -1;
    int win_tracklist_y = -1;
    int win_vj_fx_x = -1;
    int win_vj_fx_y = -1;
    bool auto_save_window_pos = true;
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

private:
    void setupUi();
    void applyConfigToUi();
    void readUiToConfig();

    SettingsAudioTab *audio_tab_ = nullptr;
    SettingsVideoTab *video_tab_ = nullptr;
    SettingsMidiTab *midi_tab_ = nullptr;
    SettingsGeneralTab *general_tab_ = nullptr;
};

void vdj_open_settings_dialog();
void vdj_free_settings_dialog();
void vdj_save_current_window_positions();
void vdj_reset_window_positions();
