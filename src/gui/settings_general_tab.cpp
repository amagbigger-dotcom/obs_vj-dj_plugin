#include "gui/settings_general_tab.hpp"
#include "gui/settings_dialog.hpp"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>

SettingsGeneralTab::SettingsGeneralTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SettingsGeneralTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *lang_group = new QGroupBox(QStringLiteral("🌐 NYELVI ÉS RENDSZERBEÁLLÍTÁSOK (LOCALIZATION)"), this);
    auto *grid = new QGridLayout(lang_group);
    grid->setSpacing(8);

    grid->addWidget(new QLabel(QStringLiteral("Alkalmazás Nyelve:"), lang_group), 0, 0);
    lang_combo_ = new QComboBox(lang_group);
    lang_combo_->addItem(QStringLiteral("🇭🇺 Magyar (Hungarian)"), QStringLiteral("hu"));
    lang_combo_->addItem(QStringLiteral("🇬🇧 English (Angol)"), QStringLiteral("en"));
    lang_combo_->addItem(QStringLiteral("🇩🇪 Deutsch (Német)"), QStringLiteral("de"));
    lang_combo_->addItem(QStringLiteral("🇫🇷 Français (Francia)"), QStringLiteral("fr"));
    grid->addWidget(lang_combo_, 0, 1);

    auto *auto_group = new QGroupBox(QStringLiteral("💾 AUTOMATIKUS MENTÉS & ADATBÁZIS"), this);
    auto *auto_vbox = new QVBoxLayout(auto_group);
    auto_save_check_ = new QCheckBox(QStringLiteral("Beállítások, ablakpozíciók és MIDI kiosztás automatikus mentése kilépéskor"), auto_group);
    auto_save_check_->setChecked(true);
    auto_vbox->addWidget(auto_save_check_);

    layout->addWidget(lang_group);
    layout->addWidget(auto_group);
    layout->addStretch();
}

void SettingsGeneralTab::applyConfig(const PluginSettingsConfig &cfg)
{
    int lang_idx = lang_combo_->findData(cfg.language);
    if (lang_idx >= 0) lang_combo_->setCurrentIndex(lang_idx);
    auto_save_check_->setChecked(cfg.auto_save_on_exit);
}

void SettingsGeneralTab::readConfig(PluginSettingsConfig &cfg)
{
    cfg.language = lang_combo_->currentData().toString();
    cfg.auto_save_on_exit = auto_save_check_->isChecked();
}
