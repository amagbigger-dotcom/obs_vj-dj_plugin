#pragma once

#include <QWidget>
#include <QString>
#include "controllers/midi_mapping.hpp"

struct PluginSettingsConfig;
class QComboBox;
class QTableWidget;
class QPushButton;
class QLabel;

class SettingsMidiTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsMidiTab(QWidget *parent = nullptr);
    ~SettingsMidiTab() override = default;

    void populateDevices();
    void applyConfig(const PluginSettingsConfig &cfg);
    void readConfig(PluginSettingsConfig &cfg);

private slots:
    void onMidiPresetChanged(int index);
    void onExportClicked();
    void onImportClicked();
    void onLearnClicked(int row);

private:
    void setupUi();
    void populatePresets();
    void refreshLearnTable();

    QComboBox *midi_in_combo_ = nullptr;
    QComboBox *midi_out_combo_ = nullptr;
    QComboBox *preset_combo_ = nullptr;
    QLabel *midi_activity_lbl_ = nullptr;
    QTableWidget *learn_table_ = nullptr;

    std::vector<MidiControllerProfile> builtin_profiles_;
    MidiControllerProfile current_profile_;
    int active_learn_row_ = -1;
};
