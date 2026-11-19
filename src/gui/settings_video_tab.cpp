#include "gui/settings_video_tab.hpp"
#include "gui/settings_dialog.hpp"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <algorithm>

SettingsVideoTab::SettingsVideoTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SettingsVideoTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *vid_group = new QGroupBox(QStringLiteral("🎬 OBS VIDEÓ MEGJELENÍTÉS ÉS RENDERELÉS"), this);
    auto *grid = new QGridLayout(vid_group);
    grid->setSpacing(8);

    grid->addWidget(new QLabel(QStringLiteral("Kimeneti Felbontás:"), vid_group), 0, 0);
    video_res_combo_ = new QComboBox(vid_group);
    video_res_combo_->addItem(QStringLiteral("1920x1080 (1080p Full HD)"), 0);
    video_res_combo_->addItem(QStringLiteral("1280x720 (720p HD)"), 1);
    video_res_combo_->addItem(QStringLiteral("OBS Vászon Méretéhez Igazodó"), 2);
    grid->addWidget(video_res_combo_, 0, 1);

    grid->addWidget(new QLabel(QStringLiteral("Vászon Kitöltési Mód (Scaling):"), vid_group), 1, 0);
    video_scale_combo_ = new QComboBox(vid_group);
    video_scale_combo_->addItem(QStringLiteral("FIT (Aránytartó méretezés fekete sávokkal)"), 0);
    video_scale_combo_->addItem(QStringLiteral("FILL (Teljes vászonkitöltés levágással)"), 1);
    video_scale_combo_->addItem(QStringLiteral("STRETCH (Nyújtás a teljes képernyőre)"), 2);
    grid->addWidget(video_scale_combo_, 1, 1);

    grid->addWidget(new QLabel(QStringLiteral("VJ Shader Minőség (GPU Load):"), vid_group), 2, 0);
    shader_quality_combo_ = new QComboBox(vid_group);
    shader_quality_combo_->addItem(QStringLiteral("Teljes 1080p (Maximális Vizuális Minőség)"), 0);
    shader_quality_combo_->addItem(QStringLiteral("Feles Felbontás 960x540 (Alacsonyabb GPU Terhelés)"), 1);
    grid->addWidget(shader_quality_combo_, 2, 1);

    layout->addWidget(vid_group);
    layout->addStretch();
}

void SettingsVideoTab::applyConfig(const PluginSettingsConfig &cfg)
{
    video_res_combo_->setCurrentIndex(std::clamp(cfg.video_resolution, 0, 2));
    video_scale_combo_->setCurrentIndex(std::clamp(cfg.default_scale_mode, 0, 2));
    shader_quality_combo_->setCurrentIndex(std::clamp(cfg.shader_quality, 0, 1));
}

void SettingsVideoTab::readConfig(PluginSettingsConfig &cfg)
{
    cfg.video_resolution = video_res_combo_->currentIndex();
    cfg.default_scale_mode = video_scale_combo_->currentIndex();
    cfg.shader_quality = shader_quality_combo_->currentIndex();
}
