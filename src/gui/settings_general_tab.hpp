#pragma once

#include <QWidget>

struct PluginSettingsConfig;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

class SettingsGeneralTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsGeneralTab(QWidget *parent = nullptr);
    ~SettingsGeneralTab() override = default;

    void applyConfig(const PluginSettingsConfig &cfg);
    void readConfig(PluginSettingsConfig &cfg);

private:
    void setupUi();

    QComboBox *lang_combo_ = nullptr;
    QCheckBox *auto_save_check_ = nullptr;
    QCheckBox *auto_save_win_check_ = nullptr;
    QPushButton *save_pos_btn_ = nullptr;
    QPushButton *reset_pos_btn_ = nullptr;
    QLabel *pos_status_lbl_ = nullptr;
};
