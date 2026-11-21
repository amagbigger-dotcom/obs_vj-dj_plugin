#include "gui/settings_dialog.hpp"
#include "gui/settings_audio_tab.hpp"
#include "gui/settings_video_tab.hpp"
#include "gui/settings_midi_tab.hpp"
#include "gui/settings_general_tab.hpp"
#include "gui/deck_window.hpp"
#include "gui/mixer_window.hpp"
#include "gui/tracklist_dock.hpp"
#include "gui/vj_fx_dialog.hpp"
#include "controllers/midi_handler.hpp"
#include "engine/deck_engine.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

PluginSettingsConfig g_config;
SettingsDialog *g_settings_dialog = nullptr;

QString get_settings_file_path()
{
    const QString base_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base_dir);
    return base_dir + QStringLiteral("/obs_vj_settings.json");
}

} // namespace

PluginSettingsConfig &SettingsDialog::config()
{
    return g_config;
}

void SettingsDialog::loadSettings()
{
    const QString path = get_settings_file_path();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull() || !doc.isObject()) return;

    const QJsonObject obj = doc.object();
    // Hardware
    g_config.audio_driver = obj.value(QStringLiteral("audio_driver")).toString(QStringLiteral("WASAPI"));
    g_config.asio_device_name = obj.value(QStringLiteral("asio_device_name")).toString();
    g_config.wasapi_device_id = obj.value(QStringLiteral("wasapi_device_id")).toString(QStringLiteral("default"));
    g_config.sample_rate = obj.value(QStringLiteral("sample_rate")).toInt(48000);
    g_config.buffer_size = obj.value(QStringLiteral("buffer_size")).toInt(256);
    g_config.audio_mode = obj.value(QStringLiteral("audio_mode")).toInt(0);

    // Outputs
    g_config.master_out_dev = obj.value(QStringLiteral("master_out_dev")).toString(QStringLiteral("default"));
    g_config.master_out_ch_l = obj.value(QStringLiteral("master_out_ch_l")).toInt(1);
    g_config.master_out_ch_r = obj.value(QStringLiteral("master_out_ch_r")).toInt(2);

    g_config.cue_out_dev = obj.value(QStringLiteral("cue_out_dev")).toString(QStringLiteral("default"));
    g_config.cue_out_ch_l = obj.value(QStringLiteral("cue_out_ch_l")).toInt(3);
    g_config.cue_out_ch_r = obj.value(QStringLiteral("cue_out_ch_r")).toInt(4);

    g_config.booth_out_dev = obj.value(QStringLiteral("booth_out_dev")).toString();
    g_config.booth_out_ch_l = obj.value(QStringLiteral("booth_out_ch_l")).toInt(5);
    g_config.booth_out_ch_r = obj.value(QStringLiteral("booth_out_ch_r")).toInt(6);

    g_config.deck_a_out_dev = obj.value(QStringLiteral("deck_a_out_dev")).toString();
    g_config.deck_a_out_ch_l = obj.value(QStringLiteral("deck_a_out_ch_l")).toInt(1);
    g_config.deck_a_out_ch_r = obj.value(QStringLiteral("deck_a_out_ch_r")).toInt(2);

    g_config.deck_b_out_dev = obj.value(QStringLiteral("deck_b_out_dev")).toString();
    g_config.deck_b_out_ch_l = obj.value(QStringLiteral("deck_b_out_ch_l")).toInt(3);
    g_config.deck_b_out_ch_r = obj.value(QStringLiteral("deck_b_out_ch_r")).toInt(4);

    // Inputs
    g_config.deck_a_in_dev = obj.value(QStringLiteral("deck_a_in_dev")).toString();
    g_config.deck_a_in_ch_l = obj.value(QStringLiteral("deck_a_in_ch_l")).toInt(1);
    g_config.deck_a_in_ch_r = obj.value(QStringLiteral("deck_a_in_ch_r")).toInt(2);

    g_config.deck_b_in_dev = obj.value(QStringLiteral("deck_b_in_dev")).toString();
    g_config.deck_b_in_ch_l = obj.value(QStringLiteral("deck_b_in_ch_l")).toInt(3);
    g_config.deck_b_in_ch_r = obj.value(QStringLiteral("deck_b_in_ch_r")).toInt(4);

    g_config.aux_in_dev = obj.value(QStringLiteral("aux_in_dev")).toString();
    g_config.aux_in_ch_l = obj.value(QStringLiteral("aux_in_ch_l")).toInt(1);
    g_config.aux_in_ch_r = obj.value(QStringLiteral("aux_in_ch_r")).toInt(2);

    // Video
    g_config.video_resolution = obj.value(QStringLiteral("video_resolution")).toInt(0);
    g_config.default_scale_mode = obj.value(QStringLiteral("default_scale_mode")).toInt(1);
    g_config.shader_quality = obj.value(QStringLiteral("shader_quality")).toInt(0);

    // MIDI
    g_config.midi_input_device = obj.value(QStringLiteral("midi_input_device")).toString();
    g_config.midi_output_device = obj.value(QStringLiteral("midi_output_device")).toString();
    g_config.active_preset = obj.value(QStringLiteral("active_preset")).toString(QStringLiteral("Traktor Kontrol S2 MK3"));

    // General
    g_config.language = obj.value(QStringLiteral("language")).toString(QStringLiteral("hu"));
    g_config.auto_save_on_exit = obj.value(QStringLiteral("auto_save_on_exit")).toBool(true);

    // Window Positions
    g_config.win_deck_a_x = obj.value(QStringLiteral("win_deck_a_x")).toInt(-1);
    g_config.win_deck_a_y = obj.value(QStringLiteral("win_deck_a_y")).toInt(-1);
    g_config.win_deck_b_x = obj.value(QStringLiteral("win_deck_b_x")).toInt(-1);
    g_config.win_deck_b_y = obj.value(QStringLiteral("win_deck_b_y")).toInt(-1);
    g_config.win_mixer_x = obj.value(QStringLiteral("win_mixer_x")).toInt(-1);
    g_config.win_mixer_y = obj.value(QStringLiteral("win_mixer_y")).toInt(-1);
    g_config.win_tracklist_x = obj.value(QStringLiteral("win_tracklist_x")).toInt(-1);
    g_config.win_tracklist_y = obj.value(QStringLiteral("win_tracklist_y")).toInt(-1);
    g_config.win_vj_fx_x = obj.value(QStringLiteral("win_vj_fx_x")).toInt(-1);
    g_config.win_vj_fx_y = obj.value(QStringLiteral("win_vj_fx_y")).toInt(-1);
    g_config.auto_save_window_pos = obj.value(QStringLiteral("auto_save_window_pos")).toBool(true);

    // Sanitize old centered values
    if (g_config.win_deck_a_x == 630 && g_config.win_deck_a_y == 302) {
        g_config.win_deck_a_x = 10;
        g_config.win_deck_a_y = 654;
    }
    if (g_config.win_deck_b_x == 630 && g_config.win_deck_b_y == 302) {
        g_config.win_deck_b_x = 1255;
        g_config.win_deck_b_y = 654;
    }
    if (g_config.win_mixer_x == 655 && g_config.win_mixer_y == 317) {
        g_config.win_mixer_x = 660;
        g_config.win_mixer_y = 654;
    }
    if (g_config.win_tracklist_x == 630 && g_config.win_tracklist_y == 237) {
        g_config.win_tracklist_x = 1255;
        g_config.win_tracklist_y = 38;
    }
    if (g_config.win_vj_fx_x == 680 && g_config.win_vj_fx_y == 237) {
        g_config.win_vj_fx_x = 10;
        g_config.win_vj_fx_y = 38;
    }
}

void SettingsDialog::saveSettings()
{
    const QString path = get_settings_file_path();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;

    QJsonObject obj;
    obj[QStringLiteral("audio_driver")] = g_config.audio_driver;
    obj[QStringLiteral("asio_device_name")] = g_config.asio_device_name;
    obj[QStringLiteral("wasapi_device_id")] = g_config.wasapi_device_id;
    obj[QStringLiteral("sample_rate")] = g_config.sample_rate;
    obj[QStringLiteral("buffer_size")] = g_config.buffer_size;
    obj[QStringLiteral("audio_mode")] = g_config.audio_mode;

    obj[QStringLiteral("master_out_dev")] = g_config.master_out_dev;
    obj[QStringLiteral("master_out_ch_l")] = g_config.master_out_ch_l;
    obj[QStringLiteral("master_out_ch_r")] = g_config.master_out_ch_r;

    obj[QStringLiteral("cue_out_dev")] = g_config.cue_out_dev;
    obj[QStringLiteral("cue_out_ch_l")] = g_config.cue_out_ch_l;
    obj[QStringLiteral("cue_out_ch_r")] = g_config.cue_out_ch_r;

    obj[QStringLiteral("booth_out_dev")] = g_config.booth_out_dev;
    obj[QStringLiteral("booth_out_ch_l")] = g_config.booth_out_ch_l;
    obj[QStringLiteral("booth_out_ch_r")] = g_config.booth_out_ch_r;

    obj[QStringLiteral("deck_a_out_dev")] = g_config.deck_a_out_dev;
    obj[QStringLiteral("deck_a_out_ch_l")] = g_config.deck_a_out_ch_l;
    obj[QStringLiteral("deck_a_out_ch_r")] = g_config.deck_a_out_ch_r;

    obj[QStringLiteral("deck_b_out_dev")] = g_config.deck_b_out_dev;
    obj[QStringLiteral("deck_b_out_ch_l")] = g_config.deck_b_out_ch_l;
    obj[QStringLiteral("deck_b_out_ch_r")] = g_config.deck_b_out_ch_r;

    obj[QStringLiteral("deck_a_in_dev")] = g_config.deck_a_in_dev;
    obj[QStringLiteral("deck_a_in_ch_l")] = g_config.deck_a_in_ch_l;
    obj[QStringLiteral("deck_a_in_ch_r")] = g_config.deck_a_in_ch_r;

    obj[QStringLiteral("deck_b_in_dev")] = g_config.deck_b_in_dev;
    obj[QStringLiteral("deck_b_in_ch_l")] = g_config.deck_b_in_ch_l;
    obj[QStringLiteral("deck_b_in_ch_r")] = g_config.deck_b_in_ch_r;

    obj[QStringLiteral("aux_in_dev")] = g_config.aux_in_dev;
    obj[QStringLiteral("aux_in_ch_l")] = g_config.aux_in_ch_l;
    obj[QStringLiteral("aux_in_ch_r")] = g_config.aux_in_ch_r;

    obj[QStringLiteral("video_resolution")] = g_config.video_resolution;
    obj[QStringLiteral("default_scale_mode")] = g_config.default_scale_mode;
    obj[QStringLiteral("shader_quality")] = g_config.shader_quality;

    obj[QStringLiteral("midi_input_device")] = g_config.midi_input_device;
    obj[QStringLiteral("midi_output_device")] = g_config.midi_output_device;
    obj[QStringLiteral("active_preset")] = g_config.active_preset;

    obj[QStringLiteral("language")] = g_config.language;
    obj[QStringLiteral("auto_save_on_exit")] = g_config.auto_save_on_exit;

    // Window Positions
    obj[QStringLiteral("win_deck_a_x")] = g_config.win_deck_a_x;
    obj[QStringLiteral("win_deck_a_y")] = g_config.win_deck_a_y;
    obj[QStringLiteral("win_deck_b_x")] = g_config.win_deck_b_x;
    obj[QStringLiteral("win_deck_b_y")] = g_config.win_deck_b_y;
    obj[QStringLiteral("win_mixer_x")] = g_config.win_mixer_x;
    obj[QStringLiteral("win_mixer_y")] = g_config.win_mixer_y;
    obj[QStringLiteral("win_tracklist_x")] = g_config.win_tracklist_x;
    obj[QStringLiteral("win_tracklist_y")] = g_config.win_tracklist_y;
    obj[QStringLiteral("win_vj_fx_x")] = g_config.win_vj_fx_x;
    obj[QStringLiteral("win_vj_fx_y")] = g_config.win_vj_fx_y;
    obj[QStringLiteral("auto_save_window_pos")] = g_config.auto_save_window_pos;

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("OBS VJ/DJ Plugin - Rendszerbeállítások (Settings)"));
    setMinimumSize(740, 600);
    resize(760, 640);
    setupUi();
    applyConfigToUi();
}

void SettingsDialog::setupUi()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #12161f; color: #d8e2ef; font-family: 'Segoe UI', Arial, sans-serif; } "
        "QTabWidget::pane { border: 1px solid #283344; background: #161b26; border-radius: 4px; } "
        "QTabBar::tab { background: #1a2230; color: #94a5bd; padding: 8px 16px; border: 1px solid #283344; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; font-weight: bold; font-size: 11px; margin-right: 2px; } "
        "QTabBar::tab:selected { background: #232d3d; color: #38bdf8; border-color: #38bdf8; } "
        "QTabBar::tab:hover { background: #1f2838; color: #ffffff; } "
        "QGroupBox { background: #151a24; border: 1px solid #283344; border-radius: 5px; margin-top: 14px; padding: 10px; font-weight: bold; font-size: 11px; color: #38bdf8; } "
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; } "
        "QLabel { color: #cbd5e1; font-size: 11px; } "
        "QComboBox { background: #1e2636; border: 1px solid #334155; border-radius: 3px; color: #f8fafc; padding: 4px 8px; font-size: 11px; min-height: 20px; } "
        "QComboBox:hover { border-color: #38bdf8; } "
        "QComboBox QAbstractItemView { background: #1e2636; color: #f8fafc; selection-background-color: #0284c7; } "
        "QPushButton { background: #1e293b; border: 1px solid #334155; color: #e2e8f0; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 11px; } "
        "QPushButton:hover { background: #334155; border-color: #38bdf8; color: #ffffff; } "
        "QTableWidget { background: #12161f; border: 1px solid #283344; color: #e2e8f0; gridline-color: #1e293b; selection-background-color: #0284c7; font-size: 11px; } "
        "QHeaderView::section { background: #1e293b; color: #94a5bd; padding: 4px; border: 1px solid #283344; font-weight: bold; font-size: 10px; } "
        "QScrollArea { border: none; background: transparent; }"));

    auto *main_vbox = new QVBoxLayout(this);
    main_vbox->setContentsMargins(12, 12, 12, 12);
    main_vbox->setSpacing(10);

    // Header
    auto *header_layout = new QHBoxLayout();
    auto *title_lbl = new QLabel(QStringLiteral("⚙ RENDSZERBEÁLLÍTÁSOK"), this);
    title_lbl->setStyleSheet(QStringLiteral("color: #38bdf8; font-weight: bold; font-size: 14px;"));
    auto *sub_lbl = new QLabel(QStringLiteral("ASIO / WASAPI, Mátrix Routing, Adatvezérelt MIDI Kontrollerek & Nyelv"), this);
    sub_lbl->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    header_layout->addWidget(title_lbl);
    header_layout->addSpacing(8);
    header_layout->addWidget(sub_lbl);
    header_layout->addStretch();
    main_vbox->addLayout(header_layout);

    auto *tabs = new QTabWidget(this);
    audio_tab_ = new SettingsAudioTab(tabs);
    video_tab_ = new SettingsVideoTab(tabs);
    midi_tab_ = new SettingsMidiTab(tabs);
    general_tab_ = new SettingsGeneralTab(tabs);

    tabs->addTab(audio_tab_, QStringLiteral("🔊 Audió & Hangkártya"));
    tabs->addTab(video_tab_, QStringLiteral("🎬 Videó & Shaders"));
    tabs->addTab(midi_tab_, QStringLiteral("🎹 MIDI & Kontrollerek"));
    tabs->addTab(general_tab_, QStringLiteral("🌐 Nyelv & Rendszer"));
    main_vbox->addWidget(tabs);

    // Footer Buttons
    auto *btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    auto *cancel_btn = new QPushButton(QStringLiteral("✖ Mégse"), this);
    connect(cancel_btn, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    btn_layout->addWidget(cancel_btn);

    auto *save_btn = new QPushButton(QStringLiteral("💾 Beállítások Mentése"), this);
    save_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0284c7; border: 1px solid #38bdf8; color: #ffffff; padding: 7px 22px; font-weight: bold; font-size: 11px; border-radius: 4px; } "
        "QPushButton:hover { background: #0369a1; }"));
    connect(save_btn, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    btn_layout->addWidget(save_btn);

    main_vbox->addLayout(btn_layout);
}

void SettingsDialog::applyConfigToUi()
{
    if (audio_tab_) {
        audio_tab_->populateDevices();
        audio_tab_->applyConfig(g_config);
    }
    if (video_tab_) video_tab_->applyConfig(g_config);
    if (midi_tab_) {
        midi_tab_->populateDevices();
        midi_tab_->applyConfig(g_config);
    }
    if (general_tab_) general_tab_->applyConfig(g_config);
}

void SettingsDialog::readUiToConfig()
{
    if (audio_tab_) audio_tab_->readConfig(g_config);
    if (video_tab_) video_tab_->readConfig(g_config);
    if (midi_tab_) midi_tab_->readConfig(g_config);
    if (general_tab_) general_tab_->readConfig(g_config);
}

void SettingsDialog::onSaveClicked()
{
    readUiToConfig();
    saveSettings();

    if (auto *decks = vdj_decks()) {
        decks->setVideoScaleMode(g_config.default_scale_mode);
    }

    // Apply MIDI Input Device immediately
    if (!g_config.midi_input_device.isEmpty()) {
        const auto in_devs = vdj_midi_get_input_devices();
        for (size_t i = 0; i < in_devs.size(); ++i) {
            if (QString::fromStdString(in_devs[i]) == g_config.midi_input_device) {
                vdj_midi_open_input_device(static_cast<int>(i));
                break;
            }
        }
    }

    // Apply MIDI Output Device immediately
    if (!g_config.midi_output_device.isEmpty()) {
        const auto out_devs = vdj_midi_get_output_devices();
        for (size_t i = 0; i < out_devs.size(); ++i) {
            if (QString::fromStdString(out_devs[i]) == g_config.midi_output_device) {
                vdj_midi_open_output_device(static_cast<int>(i));
                break;
            }
        }
    }

    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}

void vdj_open_settings_dialog()
{
    if (!g_settings_dialog) {
        g_settings_dialog = new SettingsDialog();
    }
    g_settings_dialog->show();
    g_settings_dialog->raise();
    g_settings_dialog->activateWindow();
}

void vdj_free_settings_dialog()
{
    if (g_settings_dialog) {
        delete g_settings_dialog;
        g_settings_dialog = nullptr;
    }
}

extern DeckWindow *vdj_get_deck_window_a();
extern DeckWindow *vdj_get_deck_window_b();
extern MixerWindow *vdj_get_mixer_window();
extern TracklistDock *vdj_get_tracklist_window();
extern VjFxDialog *vdj_get_vj_fx_dialog();

void vdj_save_current_window_positions()
{
    auto &cfg = SettingsDialog::config();
    if (auto *w = vdj_get_deck_window_a()) {
        if (w->isVisible()) {
            cfg.win_deck_a_x = w->x();
            cfg.win_deck_a_y = w->y();
        }
    }
    if (auto *w = vdj_get_deck_window_b()) {
        if (w->isVisible()) {
            cfg.win_deck_b_x = w->x();
            cfg.win_deck_b_y = w->y();
        }
    }
    if (auto *w = vdj_get_mixer_window()) {
        if (w->isVisible()) {
            cfg.win_mixer_x = w->x();
            cfg.win_mixer_y = w->y();
        }
    }
    if (auto *w = vdj_get_tracklist_window()) {
        if (w->isVisible()) {
            cfg.win_tracklist_x = w->x();
            cfg.win_tracklist_y = w->y();
        }
    }
    if (auto *w = vdj_get_vj_fx_dialog()) {
        if (w->isVisible()) {
            cfg.win_vj_fx_x = w->x();
            cfg.win_vj_fx_y = w->y();
        }
    }
    SettingsDialog::saveSettings();
}

void vdj_reset_window_positions()
{
    auto &cfg = SettingsDialog::config();
    // Default 5-window layout from ideal screenshot (Image 2)
    cfg.win_deck_a_x = 10;
    cfg.win_deck_a_y = 654;
    cfg.win_deck_b_x = 1255;
    cfg.win_deck_b_y = 654;
    cfg.win_mixer_x = 660;
    cfg.win_mixer_y = 654;
    cfg.win_vj_fx_x = 10;
    cfg.win_vj_fx_y = 38;
    cfg.win_tracklist_x = 1255;
    cfg.win_tracklist_y = 38;

    SettingsDialog::saveSettings();

    if (auto *w = vdj_get_deck_window_a()) w->applyWindowPosition();
    if (auto *w = vdj_get_deck_window_b()) w->applyWindowPosition();
    if (auto *w = vdj_get_mixer_window()) w->applyWindowPosition();
    if (auto *w = vdj_get_vj_fx_dialog()) w->applyWindowPosition();
    if (auto *w = vdj_get_tracklist_window()) w->applyWindowPosition();
}
