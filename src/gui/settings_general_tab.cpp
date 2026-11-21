#include "gui/settings_general_tab.hpp"
#include "gui/settings_dialog.hpp"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>

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
    auto_save_check_ = new QCheckBox(QStringLiteral("Beállítások és MIDI kiosztás automatikus mentése kilépéskor"), auto_group);
    auto_save_check_->setChecked(true);
    auto_vbox->addWidget(auto_save_check_);

    auto *win_group = new QGroupBox(QStringLiteral("🪟 ABLAKELRENDEZÉS ÉS POZÍCIÓK (WINDOW LAYOUT)"), this);
    auto *win_vbox = new QVBoxLayout(win_group);
    win_vbox->setSpacing(8);

    auto_save_win_check_ = new QCheckBox(QStringLiteral("Ablakpozíciók folyamatos megjegyzése és automatikus mentése kilépéskor"), win_group);
    auto_save_win_check_->setChecked(true);
    win_vbox->addWidget(auto_save_win_check_);

    auto *btn_row = new QHBoxLayout();
    btn_row->setSpacing(10);

    save_pos_btn_ = new QPushButton(QStringLiteral("📍 Jelenlegi Ablakpozíciók Mentése"), win_group);
    save_pos_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0284c7; border: 1px solid #38bdf8; color: #ffffff; padding: 7px 16px; font-weight: bold; border-radius: 4px; } "
        "QPushButton:hover { background: #0369a1; }"));
    btn_row->addWidget(save_pos_btn_);

    reset_pos_btn_ = new QPushButton(QStringLiteral("↺ Alapértelmezett 5-Ablakos Layout"), win_group);
    reset_pos_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1e293b; border: 1px solid #334155; color: #e2e8f0; padding: 7px 16px; font-weight: bold; border-radius: 4px; } "
        "QPushButton:hover { background: #334155; color: #ffffff; }"));
    btn_row->addWidget(reset_pos_btn_);

    win_vbox->addLayout(btn_row);

    pos_status_lbl_ = new QLabel(win_group);
    pos_status_lbl_->setStyleSheet(QStringLiteral("color: #38bdf8; font-weight: bold; font-size: 11px;"));
    win_vbox->addWidget(pos_status_lbl_);

    connect(save_pos_btn_, &QPushButton::clicked, this, [this]() {
        vdj_save_current_window_positions();
        if (pos_status_lbl_) {
            pos_status_lbl_->setStyleSheet(QStringLiteral("color: #4ade80; font-weight: bold; font-size: 11px;"));
            pos_status_lbl_->setText(QStringLiteral("✓ Minden nyitott ablak aktuális pozíciója sikeresen elmentve!"));
            QTimer::singleShot(4000, this, [this]() { if (pos_status_lbl_) pos_status_lbl_->clear(); });
        }
    });

    connect(reset_pos_btn_, &QPushButton::clicked, this, [this]() {
        vdj_reset_window_positions();
        if (pos_status_lbl_) {
            pos_status_lbl_->setStyleSheet(QStringLiteral("color: #38bdf8; font-weight: bold; font-size: 11px;"));
            pos_status_lbl_->setText(QStringLiteral("✓ Alapértelmezett 5-ablakos layout visszaállítva és alkalmazva!"));
            QTimer::singleShot(4000, this, [this]() { if (pos_status_lbl_) pos_status_lbl_->clear(); });
        }
    });

    layout->addWidget(lang_group);
    layout->addWidget(auto_group);
    layout->addWidget(win_group);
    layout->addStretch();
}

void SettingsGeneralTab::applyConfig(const PluginSettingsConfig &cfg)
{
    int lang_idx = lang_combo_->findData(cfg.language);
    if (lang_idx >= 0) lang_combo_->setCurrentIndex(lang_idx);
    auto_save_check_->setChecked(cfg.auto_save_on_exit);
    auto_save_win_check_->setChecked(cfg.auto_save_window_pos);
}

void SettingsGeneralTab::readConfig(PluginSettingsConfig &cfg)
{
    cfg.language = lang_combo_->currentData().toString();
    cfg.auto_save_on_exit = auto_save_check_->isChecked();
    cfg.auto_save_window_pos = auto_save_win_check_->isChecked();
}
