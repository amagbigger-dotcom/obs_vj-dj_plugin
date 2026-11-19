#pragma once

#include <QWidget>

struct PluginSettingsConfig;
class QComboBox;

class SettingsVideoTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsVideoTab(QWidget *parent = nullptr);
    ~SettingsVideoTab() override = default;

    void applyConfig(const PluginSettingsConfig &cfg);
    void readConfig(PluginSettingsConfig &cfg);

private:
    void setupUi();

    QComboBox *video_res_combo_ = nullptr;
    QComboBox *video_scale_combo_ = nullptr;
    QComboBox *shader_quality_combo_ = nullptr;
};
