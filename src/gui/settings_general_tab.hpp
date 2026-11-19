#pragma once

#include <QWidget>

struct PluginSettingsConfig;
class QComboBox;
class QCheckBox;

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
};
