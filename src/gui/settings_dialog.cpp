#include "gui/settings_dialog.hpp"
#include "controllers/midi_handler.hpp"
#include "engine/deck_engine.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <windows.h>
#include <mmsystem.h>

namespace {

SettingsDialog *g_settings_dialog = nullptr;
PluginSettingsConfig g_config;

std::vector<std::string> get_system_asio_drivers()
{
    std::vector<std::string> drivers;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASIO", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char subKeyName[256];
        DWORD index = 0;
        DWORD subKeyLen = sizeof(subKeyName);
        while (RegEnumKeyExA(hKey, index, subKeyName, &subKeyLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            drivers.push_back(subKeyName);
            index++;
            subKeyLen = sizeof(subKeyName);
        }
        RegCloseKey(hKey);
    }
    return drivers;
}

struct MonitoringDeviceItem {
    QString id;
    QString name;
};

bool enum_monitoring_devices_cb(void *param, const char *name, const char *id)
{
    auto *list = static_cast<std::vector<MonitoringDeviceItem> *>(param);
    if (list && name && id) {
        list->push_back({QString::fromUtf8(id), QString::fromUtf8(name)});
    }
    return true;
}

std::vector<MonitoringDeviceItem> get_wasapi_devices()
{
    std::vector<MonitoringDeviceItem> devices;
    devices.push_back({QStringLiteral("default"), QStringLiteral("Alapértelmezett Rendszer Hangkártya (Default)")});
    obs_enum_audio_monitoring_devices(enum_monitoring_devices_cb, &devices);
    return devices;
}

std::vector<std::string> get_winmm_input_devices()
{
    std::vector<std::string> devices;
    const UINT count = waveInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        WAVEINCAPSA caps = {};
        if (waveInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            devices.push_back(caps.szPname);
        }
    }
    return devices;
}

std::vector<std::string> get_winmm_output_devices()
{
    std::vector<std::string> devices;
    const UINT count = waveOutGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        WAVEOUTCAPSA caps = {};
        if (waveOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            devices.push_back(caps.szPname);
        }
    }
    return devices;
}

QString get_settings_file_path()
{
    const QString base_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base_dir + QStringLiteral("/obs-vj-mixer"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.absoluteFilePath(QStringLiteral("settings.json"));
}

} // namespace

PluginSettingsConfig &SettingsDialog::config()
{
    return g_config;
}

void SettingsDialog::populateChannelCombo(QComboBox *combo, int selected_ch, int max_ch)
{
    if (!combo) return;
    combo->clear();
    combo->addItem(QStringLiteral("-- Nincs (Disabled) --"), 0);
    for (int i = 1; i <= max_ch; ++i) {
        QString label = QString::asprintf("Ch %d", i);
        if (i == 1) label += QStringLiteral(" (Bal / Left)");
        else if (i == 2) label += QStringLiteral(" (Jobb / Right)");
        combo->addItem(label, i);
    }
    int idx = combo->findData(selected_ch);
    if (idx >= 0) combo->setCurrentIndex(idx);
    else combo->setCurrentIndex(0);
}

void SettingsDialog::loadSettings()
{
    const QString path = get_settings_file_path();
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;

    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;

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
}

void SettingsDialog::saveSettings()
{
    const QString path = get_settings_file_path();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;

    QJsonObject obj;
    // Hardware
    obj[QStringLiteral("audio_driver")] = g_config.audio_driver;
    obj[QStringLiteral("asio_device_name")] = g_config.asio_device_name;
    obj[QStringLiteral("wasapi_device_id")] = g_config.wasapi_device_id;
    obj[QStringLiteral("sample_rate")] = g_config.sample_rate;
    obj[QStringLiteral("buffer_size")] = g_config.buffer_size;
    obj[QStringLiteral("audio_mode")] = g_config.audio_mode;

    // Outputs
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

    // Inputs
    obj[QStringLiteral("deck_a_in_dev")] = g_config.deck_a_in_dev;
    obj[QStringLiteral("deck_a_in_ch_l")] = g_config.deck_a_in_ch_l;
    obj[QStringLiteral("deck_a_in_ch_r")] = g_config.deck_a_in_ch_r;

    obj[QStringLiteral("deck_b_in_dev")] = g_config.deck_b_in_dev;
    obj[QStringLiteral("deck_b_in_ch_l")] = g_config.deck_b_in_ch_l;
    obj[QStringLiteral("deck_b_in_ch_r")] = g_config.deck_b_in_ch_r;

    obj[QStringLiteral("aux_in_dev")] = g_config.aux_in_dev;
    obj[QStringLiteral("aux_in_ch_l")] = g_config.aux_in_ch_l;
    obj[QStringLiteral("aux_in_ch_r")] = g_config.aux_in_ch_r;

    // Video
    obj[QStringLiteral("video_resolution")] = g_config.video_resolution;
    obj[QStringLiteral("default_scale_mode")] = g_config.default_scale_mode;
    obj[QStringLiteral("shader_quality")] = g_config.shader_quality;

    // MIDI
    obj[QStringLiteral("midi_input_device")] = g_config.midi_input_device;
    obj[QStringLiteral("midi_output_device")] = g_config.midi_output_device;
    obj[QStringLiteral("active_preset")] = g_config.active_preset;

    // General
    obj[QStringLiteral("language")] = g_config.language;
    obj[QStringLiteral("auto_save_on_exit")] = g_config.auto_save_on_exit;

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
    populateDevices();
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
        "QRadioButton { color: #cbd5e1; font-size: 11px; spacing: 6px; } "
        "QRadioButton::indicator:checked { background: #38bdf8; border: 2px solid #0f172a; border-radius: 6px; } "
        "QCheckBox { color: #cbd5e1; font-size: 11px; spacing: 6px; } "
        "QCheckBox::indicator:checked { background: #38bdf8; border: 1px solid #0f172a; border-radius: 2px; } "
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
    auto *sub_lbl = new QLabel(QStringLiteral("ASIO / WASAPI, Csatornakiosztás (Routing), Deckenkénti In/Out, MIDI & Nyelv"), this);
    sub_lbl->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    header_layout->addWidget(title_lbl);
    header_layout->addSpacing(8);
    header_layout->addWidget(sub_lbl);
    header_layout->addStretch();
    main_vbox->addLayout(header_layout);

    auto *tabs = new QTabWidget(this);
    setupAudioTab(tabs);
    setupVideoTab(tabs);
    setupMidiTab(tabs);
    setupGeneralTab(tabs);
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

void SettingsDialog::setupAudioTab(QTabWidget *tabs)
{
    auto *scroll_area = new QScrollArea();
    scroll_area->setWidgetResizable(true);

    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    // 1. Driver & Hardware Section
    auto *drv_group = new QGroupBox(QStringLiteral("🔊 HANGKÁRTYA ÉS ILLESZTŐPROGRAM (AUDIO DRIVER)"), page);
    auto *drv_grid = new QGridLayout(drv_group);
    drv_grid->setSpacing(8);

    drv_grid->addWidget(new QLabel(QStringLiteral("Hangkezelő Rendszer:"), drv_group), 0, 0);
    audio_driver_combo_ = new QComboBox(drv_group);
    audio_driver_combo_->addItem(QStringLiteral("WASAPI (Standard Windows Audio)"), QStringLiteral("WASAPI"));
    audio_driver_combo_->addItem(QStringLiteral("ASIO (Professzionális Alacsony Késleltetés)"), QStringLiteral("ASIO"));
    connect(audio_driver_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onAudioDriverChanged);
    drv_grid->addWidget(audio_driver_combo_, 0, 1, 1, 2);

    asio_device_label_ = new QLabel(QStringLiteral("ASIO Eszköz:"), drv_group);
    asio_device_combo_ = new QComboBox(drv_group);
    drv_grid->addWidget(asio_device_label_, 1, 0);
    drv_grid->addWidget(asio_device_combo_, 1, 1, 1, 2);

    wasapi_device_label_ = new QLabel(QStringLiteral("WASAPI Monitor Eszköz:"), drv_group);
    wasapi_device_combo_ = new QComboBox(drv_group);
    drv_grid->addWidget(wasapi_device_label_, 2, 0);
    drv_grid->addWidget(wasapi_device_combo_, 2, 1, 1, 2);

    drv_grid->addWidget(new QLabel(QStringLiteral("Mintavételezési Ráta:"), drv_group), 3, 0);
    sample_rate_combo_ = new QComboBox(drv_group);
    sample_rate_combo_->addItem(QStringLiteral("44100 Hz (CD Audio Standard)"), 44100);
    sample_rate_combo_->addItem(QStringLiteral("48000 Hz (OBS Stúdió Standard)"), 48000);
    sample_rate_combo_->addItem(QStringLiteral("96000 Hz (Hi-Res Stúdió Minőség)"), 96000);
    drv_grid->addWidget(sample_rate_combo_, 3, 1, 1, 2);

    drv_grid->addWidget(new QLabel(QStringLiteral("Pufferméret / Késleltetés:"), drv_group), 4, 0);
    buffer_size_combo_ = new QComboBox(drv_group);
    buffer_size_combo_->addItem(QStringLiteral("64 minta (~1.3 ms - Ultra alacsony)"), 64);
    buffer_size_combo_->addItem(QStringLiteral("128 minta (~2.6 ms - Alacsony)"), 128);
    buffer_size_combo_->addItem(QStringLiteral("256 minta (~5.3 ms - Optimális DJ)"), 256);
    buffer_size_combo_->addItem(QStringLiteral("512 minta (~10.6 ms - Biztonságos)"), 512);
    buffer_size_combo_->addItem(QStringLiteral("1024 minta (~21.3 ms - Magas)"), 1024);
    drv_grid->addWidget(buffer_size_combo_, 4, 1, 1, 2);

    drv_grid->addWidget(new QLabel(QStringLiteral("Keverési Architektúra:"), drv_group), 5, 0);
    auto *mode_btn_group = new QButtonGroup(page);
    mode_internal_radio_ = new QRadioButton(QStringLiteral("Belső Keverő (Internal Mixer: Master Out + CUE Füles Out)"), drv_group);
    mode_external_radio_ = new QRadioButton(QStringLiteral("Külső Keverő Mód / Split Deck (External Mixer: Deck A -> Ch 1-2, Deck B -> Ch 3-4 pl. Pioneer DJM-350)"), drv_group);
    mode_btn_group->addButton(mode_internal_radio_, 0);
    mode_btn_group->addButton(mode_external_radio_, 1);
    connect(mode_internal_radio_, &QRadioButton::toggled, this, &SettingsDialog::onAudioModeChanged);

    auto *mode_vbox = new QVBoxLayout();
    mode_vbox->addWidget(mode_internal_radio_);
    mode_vbox->addWidget(mode_external_radio_);
    drv_grid->addLayout(mode_vbox, 5, 1, 1, 2);

    layout->addWidget(drv_group);

    // 2. Output Routing Matrix
    auto *out_group = new QGroupBox(QStringLiteral("🎛 KIMENETI CSATORNÁK KIOSZTÁSA (OUTPUT ROUTING MATRIX)"), page);
    auto *out_grid = new QGridLayout(out_group);
    out_grid->setSpacing(6);

    out_grid->addWidget(new QLabel(QStringLiteral("<b>Kimeneti Funkció</b>"), out_group), 0, 0);
    out_grid->addWidget(new QLabel(QStringLiteral("<b>Hangkártya Eszköz</b>"), out_group), 0, 1);
    out_grid->addWidget(new QLabel(QStringLiteral("<b>Bal Csatorna (L)</b>"), out_group), 0, 2);
    out_grid->addWidget(new QLabel(QStringLiteral("<b>Jobb Csatorna (R)</b>"), out_group), 0, 3);

    // Row 1: Master
    out_grid->addWidget(new QLabel(QStringLiteral("🔊 Főkimenet (Master Out):"), out_group), 1, 0);
    master_dev_combo_ = new QComboBox(out_group);
    master_ch_l_combo_ = new QComboBox(out_group);
    master_ch_r_combo_ = new QComboBox(out_group);
    out_grid->addWidget(master_dev_combo_, 1, 1);
    out_grid->addWidget(master_ch_l_combo_, 1, 2);
    out_grid->addWidget(master_ch_r_combo_, 1, 3);

    // Row 2: Headphones CUE
    out_grid->addWidget(new QLabel(QStringLiteral("🎧 Fejhallgató CUE (PFL Out):"), out_group), 2, 0);
    cue_dev_combo_ = new QComboBox(out_group);
    cue_ch_l_combo_ = new QComboBox(out_group);
    cue_ch_r_combo_ = new QComboBox(out_group);
    out_grid->addWidget(cue_dev_combo_, 2, 1);
    out_grid->addWidget(cue_ch_l_combo_, 2, 2);
    out_grid->addWidget(cue_ch_r_combo_, 2, 3);

    // Row 3: Booth / Monitor
    out_grid->addWidget(new QLabel(QStringLiteral("🔈 Monitor / Booth Out:"), out_group), 3, 0);
    booth_dev_combo_ = new QComboBox(out_group);
    booth_ch_l_combo_ = new QComboBox(out_group);
    booth_ch_r_combo_ = new QComboBox(out_group);
    out_grid->addWidget(booth_dev_combo_, 3, 1);
    out_grid->addWidget(booth_ch_l_combo_, 3, 2);
    out_grid->addWidget(booth_ch_r_combo_, 3, 3);

    // Row 4: Deck A Out
    out_grid->addWidget(new QLabel(QStringLiteral("◀ Deck A Közvetlen Kimenet:"), out_group), 4, 0);
    deck_a_out_dev_combo_ = new QComboBox(out_group);
    deck_a_out_ch_l_combo_ = new QComboBox(out_group);
    deck_a_out_ch_r_combo_ = new QComboBox(out_group);
    out_grid->addWidget(deck_a_out_dev_combo_, 4, 1);
    out_grid->addWidget(deck_a_out_ch_l_combo_, 4, 2);
    out_grid->addWidget(deck_a_out_ch_r_combo_, 4, 3);

    // Row 5: Deck B Out
    out_grid->addWidget(new QLabel(QStringLiteral("▶ Deck B Közvetlen Kimenet:"), out_group), 5, 0);
    deck_b_out_dev_combo_ = new QComboBox(out_group);
    deck_b_out_ch_l_combo_ = new QComboBox(out_group);
    deck_b_out_ch_r_combo_ = new QComboBox(out_group);
    out_grid->addWidget(deck_b_out_dev_combo_, 5, 1);
    out_grid->addWidget(deck_b_out_ch_l_combo_, 5, 2);
    out_grid->addWidget(deck_b_out_ch_r_combo_, 5, 3);

    layout->addWidget(out_group);

    // 3. Input Routing Matrix
    auto *in_group = new QGroupBox(QStringLiteral("🎤 BEMENETI CSATORNÁK KIOSZTÁSA (INPUT ROUTING MATRIX)"), page);
    auto *in_grid = new QGridLayout(in_group);
    in_grid->setSpacing(6);

    in_grid->addWidget(new QLabel(QStringLiteral("<b>Bemeneti Funkció</b>"), in_group), 0, 0);
    in_grid->addWidget(new QLabel(QStringLiteral("<b>Bemeneti Eszköz</b>"), in_group), 0, 1);
    in_grid->addWidget(new QLabel(QStringLiteral("<b>Bal Bemenet (L)</b>"), in_group), 0, 2);
    in_grid->addWidget(new QLabel(QStringLiteral("<b>Jobb Bemenet (R)</b>"), in_group), 0, 3);

    // Row 1: Deck A In
    in_grid->addWidget(new QLabel(QStringLiteral("◀ Deck A Vonal/Phono In:"), in_group), 1, 0);
    deck_a_in_dev_combo_ = new QComboBox(in_group);
    deck_a_in_ch_l_combo_ = new QComboBox(in_group);
    deck_a_in_ch_r_combo_ = new QComboBox(in_group);
    in_grid->addWidget(deck_a_in_dev_combo_, 1, 1);
    in_grid->addWidget(deck_a_in_ch_l_combo_, 1, 2);
    in_grid->addWidget(deck_a_in_ch_r_combo_, 1, 3);

    // Row 2: Deck B In
    in_grid->addWidget(new QLabel(QStringLiteral("▶ Deck B Vonal/Phono In:"), in_group), 2, 0);
    deck_b_in_dev_combo_ = new QComboBox(in_group);
    deck_b_in_ch_l_combo_ = new QComboBox(in_group);
    deck_b_in_ch_r_combo_ = new QComboBox(in_group);
    in_grid->addWidget(deck_b_in_dev_combo_, 2, 1);
    in_grid->addWidget(deck_b_in_ch_l_combo_, 2, 2);
    in_grid->addWidget(deck_b_in_ch_r_combo_, 2, 3);

    // Row 3: Aux / Mic
    in_grid->addWidget(new QLabel(QStringLiteral("🎙 Aux / Mikrofon Bemenet:"), in_group), 3, 0);
    aux_in_dev_combo_ = new QComboBox(in_group);
    aux_in_ch_l_combo_ = new QComboBox(in_group);
    aux_in_ch_r_combo_ = new QComboBox(in_group);
    in_grid->addWidget(aux_in_dev_combo_, 3, 1);
    in_grid->addWidget(aux_in_ch_l_combo_, 3, 2);
    in_grid->addWidget(aux_in_ch_r_combo_, 3, 3);

    layout->addWidget(in_group);

    scroll_area->setWidget(page);
    tabs->addTab(scroll_area, QStringLiteral("🔊 Audió & Hangkártya"));
}

void SettingsDialog::setupVideoTab(QTabWidget *tabs)
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *vid_group = new QGroupBox(QStringLiteral("🎬 OBS VIDEÓ MEGJELENÍTÉS ÉS RENDERELÉS"), page);
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
    tabs->addTab(page, QStringLiteral("🎬 Videó & Shaders"));
}

void SettingsDialog::setupMidiTab(QTabWidget *tabs)
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // Hardware selection
    auto *hw_group = new QGroupBox(QStringLiteral("🎹 MIDI KONTROLLER ÉS GYÁRI PROFIL"), page);
    auto *grid = new QGridLayout(hw_group);
    grid->setSpacing(8);

    grid->addWidget(new QLabel(QStringLiteral("MIDI Bemeneti Eszköz:"), hw_group), 0, 0);
    midi_in_combo_ = new QComboBox(hw_group);
    grid->addWidget(midi_in_combo_, 0, 1);

    grid->addWidget(new QLabel(QStringLiteral("MIDI Kimeneti Eszköz:"), hw_group), 1, 0);
    midi_out_combo_ = new QComboBox(hw_group);
    grid->addWidget(midi_out_combo_, 1, 1);

    grid->addWidget(new QLabel(QStringLiteral("Gyári Kontroller Profil:"), hw_group), 2, 0);
    midi_preset_combo_ = new QComboBox(hw_group);
    midi_preset_combo_->addItem(QStringLiteral("Native Instruments Traktor Kontrol S2 MK3"));
    midi_preset_combo_->addItem(QStringLiteral("Korg Kaoss DJ (Audio + MIDI + Touchpad VJ)"));
    midi_preset_combo_->addItem(QStringLiteral("Pioneer CDJ-350 / DDJ Széria"));
    connect(midi_preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onMidiPresetChanged);
    grid->addWidget(midi_preset_combo_, 2, 1);

    auto *prof_btn_layout = new QHBoxLayout();
    auto *exp_btn = new QPushButton(QStringLiteral("📥 Profil Exportálása JSON-ba..."), hw_group);
    connect(exp_btn, &QPushButton::clicked, this, &SettingsDialog::onMidiExportClicked);
    prof_btn_layout->addWidget(exp_btn);

    auto *imp_btn = new QPushButton(QStringLiteral("📤 Profil Betöltése JSON-ból..."), hw_group);
    connect(imp_btn, &QPushButton::clicked, this, &SettingsDialog::onMidiImportClicked);
    prof_btn_layout->addWidget(imp_btn);

    grid->addLayout(prof_btn_layout, 3, 0, 1, 2);
    layout->addWidget(hw_group);

    // Interactive MIDI Learn Table
    auto *learn_group = new QGroupBox(QStringLiteral("🔴 INTERAKTÍV MIDI LEARN FELÜLET"), page);
    auto *learn_vbox = new QVBoxLayout(learn_group);
    learn_vbox->setSpacing(6);

    midi_learn_table_ = new QTableWidget(learn_group);
    midi_learn_table_->setColumnCount(3);
    midi_learn_table_->setHorizontalHeaderLabels({QStringLiteral("Funkció"), QStringLiteral("Jelenlegi MIDI Hozzárendelés"), QStringLiteral("Művelet")});
    midi_learn_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    midi_learn_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    midi_learn_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    midi_learn_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    midi_learn_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    static const struct {
        const char *name;
        const char *binding;
    } DEFAULT_ROWS[] = {
        {"Master Crossfader", "Ch 1, CC 8 (0..127)"},
        {"Deck A Play / Pause", "Ch 1, Note 35"},
        {"Deck A CUE Pont", "Ch 1, Note 36"},
        {"Deck A SYNC Tempó", "Ch 1, Note 37"},
        {"Deck A Pitch Fader", "Ch 1, CC 21"},
        {"Deck A Hangerő Fader", "Ch 1, CC 20"},
        {"Deck A HI (Vörös EQ)", "Ch 1, CC 16"},
        {"Deck A MID (Zöld EQ)", "Ch 1, CC 17"},
        {"Deck A LOW (Kék EQ)", "Ch 1, CC 18"},
        {"Deck A Filter / Sat", "Ch 1, CC 19"},
        {"Deck B Play / Pause", "Ch 2, Note 35"},
        {"Deck B CUE Pont", "Ch 2, Note 36"},
        {"Deck B SYNC Tempó", "Ch 2, Note 37"},
        {"Deck B Pitch Fader", "Ch 2, CC 21"},
        {"Deck B Hangerő Fader", "Ch 2, CC 20"},
        {"Deck B HI (Vörös EQ)", "Ch 2, CC 16"},
        {"Deck B MID (Zöld EQ)", "Ch 2, CC 17"},
        {"Deck B LOW (Kék EQ)", "Ch 2, CC 18"},
        {"Deck B Filter / Sat", "Ch 2, CC 19"},
        {"Kaoss Touchpad X (VJ p0)", "Ch 1, CC 12"},
        {"Kaoss Touchpad Y (VJ p1)", "Ch 1, CC 13"}
    };

    midi_learn_table_->setRowCount(sizeof(DEFAULT_ROWS) / sizeof(DEFAULT_ROWS[0]));
    for (int r = 0; r < midi_learn_table_->rowCount(); ++r) {
        midi_learn_table_->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(DEFAULT_ROWS[r].name)));
        midi_learn_table_->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(DEFAULT_ROWS[r].binding)));
        auto *learn_btn = new QPushButton(QStringLiteral("🔴 LEARN"), midi_learn_table_);
        learn_btn->setFixedHeight(22);
        learn_btn->setStyleSheet(QStringLiteral("QPushButton { background: #3b0764; border: 1px solid #7e22ce; color: #f3e8ff; font-size: 10px; padding: 2px 8px; } QPushButton:hover { background: #6b21a8; }"));
        connect(learn_btn, &QPushButton::clicked, [this, r]() { onMidiLearnClicked(r); });
        midi_learn_table_->setCellWidget(r, 2, learn_btn);
    }

    learn_vbox->addWidget(midi_learn_table_);
    layout->addWidget(learn_group);

    tabs->addTab(page, QStringLiteral("🎹 MIDI & Kontrollerek"));
}

void SettingsDialog::setupGeneralTab(QTabWidget *tabs)
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *lang_group = new QGroupBox(QStringLiteral("🌐 NYELVKEZELÉS (MULTILINGUAL / I18N)"), page);
    auto *grid = new QGridLayout(lang_group);
    grid->setSpacing(8);

    grid->addWidget(new QLabel(QStringLiteral("Alkalmazás Nyelve:"), lang_group), 0, 0);
    lang_combo_ = new QComboBox(lang_group);
    lang_combo_->addItem(QStringLiteral("🇭🇺 Magyar (Anyanyelvi DJ terminológia)"), QStringLiteral("hu"));
    lang_combo_->addItem(QStringLiteral("🇬🇧 English (Standard International DJ)"), QStringLiteral("en"));
    lang_combo_->addItem(QStringLiteral("🇩🇪 Deutsch (Deutsche Benutzeroberfläche)"), QStringLiteral("de"));
    lang_combo_->addItem(QStringLiteral("🇫🇷 Français (Interface utilisateur en français)"), QStringLiteral("fr"));
    grid->addWidget(lang_combo_, 0, 1);

    auto *save_group = new QGroupBox(QStringLiteral("⚙ MENTÉS ÉS INDÍTÁS"), page);
    auto *sg_vbox = new QVBoxLayout(save_group);
    sg_vbox->setSpacing(6);

    auto_save_check_ = new QCheckBox(QStringLiteral("Beállítások és ablakpozíciók automatikus mentése kilépéskor"), save_group);
    auto_save_check_->setChecked(true);
    sg_vbox->addWidget(auto_save_check_);

    layout->addWidget(lang_group);
    layout->addWidget(save_group);
    layout->addStretch();
    tabs->addTab(page, QStringLiteral("🌐 Nyelv & Rendszer"));
}

void SettingsDialog::populateDevices()
{
    // ASIO Devices
    asio_device_combo_->clear();
    const auto asio_drivers = get_system_asio_drivers();
    if (asio_drivers.empty()) {
        asio_device_combo_->addItem(QStringLiteral("Nincs telepített ASIO illesztőprogram"));
        asio_device_combo_->setEnabled(false);
    } else {
        for (const auto &drv : asio_drivers) {
            asio_device_combo_->addItem(QString::fromStdString(drv));
        }
        asio_device_combo_->setEnabled(true);
    }

    // WASAPI Output Devices
    wasapi_device_combo_->clear();
    const auto wasapi_devs = get_wasapi_devices();
    for (const auto &d : wasapi_devs) {
        wasapi_device_combo_->addItem(d.name, d.id);
    }

    // Populate all Output Routing device dropdowns
    auto populate_out_devs = [&](QComboBox *combo) {
        if (!combo) return;
        combo->clear();
        combo->addItem(QStringLiteral("Alapértelmezett (Default Master)"), QStringLiteral("default"));
        for (const auto &d : wasapi_devs) {
            if (d.id != QStringLiteral("default")) {
                combo->addItem(d.name, d.id);
            }
        }
        for (const auto &drv : asio_drivers) {
            combo->addItem(QStringLiteral("ASIO: ") + QString::fromStdString(drv), QString::fromStdString(drv));
        }
    };

    populate_out_devs(master_dev_combo_);
    populate_out_devs(cue_dev_combo_);
    populate_out_devs(booth_dev_combo_);
    populate_out_devs(deck_a_out_dev_combo_);
    populate_out_devs(deck_b_out_dev_combo_);

    // Populate Input Routing devices
    const auto in_devs = get_winmm_input_devices();
    auto populate_in_devs = [&](QComboBox *combo) {
        if (!combo) return;
        combo->clear();
        combo->addItem(QStringLiteral("-- Nincs Bemenet (Disabled) --"), QString());
        for (const auto &indev : in_devs) {
            combo->addItem(QString::fromStdString(indev), QString::fromStdString(indev));
        }
        for (const auto &drv : asio_drivers) {
            combo->addItem(QStringLiteral("ASIO In: ") + QString::fromStdString(drv), QString::fromStdString(drv));
        }
    };

    populate_in_devs(deck_a_in_dev_combo_);
    populate_in_devs(deck_b_in_dev_combo_);
    populate_in_devs(aux_in_dev_combo_);

    // Populate Channels (Ch 1 - Ch 16)
    populateChannelCombo(master_ch_l_combo_, g_config.master_out_ch_l);
    populateChannelCombo(master_ch_r_combo_, g_config.master_out_ch_r);
    populateChannelCombo(cue_ch_l_combo_, g_config.cue_out_ch_l);
    populateChannelCombo(cue_ch_r_combo_, g_config.cue_out_ch_r);
    populateChannelCombo(booth_ch_l_combo_, g_config.booth_out_ch_l);
    populateChannelCombo(booth_ch_r_combo_, g_config.booth_out_ch_r);
    populateChannelCombo(deck_a_out_ch_l_combo_, g_config.deck_a_out_ch_l);
    populateChannelCombo(deck_a_out_ch_r_combo_, g_config.deck_a_out_ch_r);
    populateChannelCombo(deck_b_out_ch_l_combo_, g_config.deck_b_out_ch_l);
    populateChannelCombo(deck_b_out_ch_r_combo_, g_config.deck_b_out_ch_r);

    populateChannelCombo(deck_a_in_ch_l_combo_, g_config.deck_a_in_ch_l);
    populateChannelCombo(deck_a_in_ch_r_combo_, g_config.deck_a_in_ch_r);
    populateChannelCombo(deck_b_in_ch_l_combo_, g_config.deck_b_in_ch_l);
    populateChannelCombo(deck_b_in_ch_r_combo_, g_config.deck_b_in_ch_r);
    populateChannelCombo(aux_in_ch_l_combo_, g_config.aux_in_ch_l);
    populateChannelCombo(aux_in_ch_r_combo_, g_config.aux_in_ch_r);

    // MIDI In
    midi_in_combo_->clear();
    const auto midi_in = vdj_midi_get_input_devices();
    if (midi_in.empty()) {
        midi_in_combo_->addItem(QStringLiteral("Nincs csatlakoztatott MIDI bemenet"));
    } else {
        for (const auto &dev : midi_in) {
            midi_in_combo_->addItem(QString::fromStdString(dev));
        }
    }

    // MIDI Out
    midi_out_combo_->clear();
    const auto midi_out = vdj_midi_get_output_devices();
    if (midi_out.empty()) {
        midi_out_combo_->addItem(QStringLiteral("Nincs csatlakoztatott MIDI kimenet"));
    } else {
        for (const auto &dev : midi_out) {
            midi_out_combo_->addItem(QString::fromStdString(dev));
        }
    }
}

void SettingsDialog::applyConfigToUi()
{
    // Audio Hardware
    int drv_idx = audio_driver_combo_->findData(g_config.audio_driver);
    if (drv_idx >= 0) audio_driver_combo_->setCurrentIndex(drv_idx);

    int asio_idx = asio_device_combo_->findText(g_config.asio_device_name);
    if (asio_idx >= 0) asio_device_combo_->setCurrentIndex(asio_idx);

    int wasapi_idx = wasapi_device_combo_->findData(g_config.wasapi_device_id);
    if (wasapi_idx >= 0) wasapi_device_combo_->setCurrentIndex(wasapi_idx);

    int sr_idx = sample_rate_combo_->findData(g_config.sample_rate);
    if (sr_idx >= 0) sample_rate_combo_->setCurrentIndex(sr_idx);

    int buf_idx = buffer_size_combo_->findData(g_config.buffer_size);
    if (buf_idx >= 0) buffer_size_combo_->setCurrentIndex(buf_idx);

    if (g_config.audio_mode == 1) {
        mode_external_radio_->setChecked(true);
    } else {
        mode_internal_radio_->setChecked(true);
    }

    onAudioDriverChanged(audio_driver_combo_->currentIndex());

    // Outputs
    auto set_combo_val = [](QComboBox *c, const QString &val) {
        if (!c) return;
        int idx = c->findData(val);
        if (idx < 0) idx = c->findText(val);
        if (idx >= 0) c->setCurrentIndex(idx);
    };

    set_combo_val(master_dev_combo_, g_config.master_out_dev);
    set_combo_val(cue_dev_combo_, g_config.cue_out_dev);
    set_combo_val(booth_dev_combo_, g_config.booth_out_dev);
    set_combo_val(deck_a_out_dev_combo_, g_config.deck_a_out_dev);
    set_combo_val(deck_b_out_dev_combo_, g_config.deck_b_out_dev);

    // Inputs
    set_combo_val(deck_a_in_dev_combo_, g_config.deck_a_in_dev);
    set_combo_val(deck_b_in_dev_combo_, g_config.deck_b_in_dev);
    set_combo_val(aux_in_dev_combo_, g_config.aux_in_dev);

    // Video
    video_res_combo_->setCurrentIndex(std::clamp(g_config.video_resolution, 0, 2));
    video_scale_combo_->setCurrentIndex(std::clamp(g_config.default_scale_mode, 0, 2));
    shader_quality_combo_->setCurrentIndex(std::clamp(g_config.shader_quality, 0, 1));

    // MIDI
    int m_in_idx = midi_in_combo_->findText(g_config.midi_input_device);
    if (m_in_idx >= 0) midi_in_combo_->setCurrentIndex(m_in_idx);

    int m_out_idx = midi_out_combo_->findText(g_config.midi_output_device);
    if (m_out_idx >= 0) midi_out_combo_->setCurrentIndex(m_out_idx);

    int pres_idx = midi_preset_combo_->findText(g_config.active_preset);
    if (pres_idx >= 0) midi_preset_combo_->setCurrentIndex(pres_idx);

    // General
    int lang_idx = lang_combo_->findData(g_config.language);
    if (lang_idx >= 0) lang_combo_->setCurrentIndex(lang_idx);

    auto_save_check_->setChecked(g_config.auto_save_on_exit);
}

void SettingsDialog::readUiToConfig()
{
    g_config.audio_driver = audio_driver_combo_->currentData().toString();
    g_config.asio_device_name = asio_device_combo_->currentText();
    g_config.wasapi_device_id = wasapi_device_combo_->currentData().toString();
    g_config.sample_rate = sample_rate_combo_->currentData().toInt();
    g_config.buffer_size = buffer_size_combo_->currentData().toInt();
    g_config.audio_mode = mode_external_radio_->isChecked() ? 1 : 0;

    // Outputs
    g_config.master_out_dev = master_dev_combo_->currentData().toString();
    g_config.master_out_ch_l = master_ch_l_combo_->currentData().toInt();
    g_config.master_out_ch_r = master_ch_r_combo_->currentData().toInt();

    g_config.cue_out_dev = cue_dev_combo_->currentData().toString();
    g_config.cue_out_ch_l = cue_ch_l_combo_->currentData().toInt();
    g_config.cue_out_ch_r = cue_ch_r_combo_->currentData().toInt();

    g_config.booth_out_dev = booth_dev_combo_->currentData().toString();
    g_config.booth_out_ch_l = booth_ch_l_combo_->currentData().toInt();
    g_config.booth_out_ch_r = booth_ch_r_combo_->currentData().toInt();

    g_config.deck_a_out_dev = deck_a_out_dev_combo_->currentData().toString();
    g_config.deck_a_out_ch_l = deck_a_out_ch_l_combo_->currentData().toInt();
    g_config.deck_a_out_ch_r = deck_a_out_ch_r_combo_->currentData().toInt();

    g_config.deck_b_out_dev = deck_b_out_dev_combo_->currentData().toString();
    g_config.deck_b_out_ch_l = deck_b_out_ch_l_combo_->currentData().toInt();
    g_config.deck_b_out_ch_r = deck_b_out_ch_r_combo_->currentData().toInt();

    // Inputs
    g_config.deck_a_in_dev = deck_a_in_dev_combo_->currentData().toString();
    g_config.deck_a_in_ch_l = deck_a_in_ch_l_combo_->currentData().toInt();
    g_config.deck_a_in_ch_r = deck_a_in_ch_r_combo_->currentData().toInt();

    g_config.deck_b_in_dev = deck_b_in_dev_combo_->currentData().toString();
    g_config.deck_b_in_ch_l = deck_b_in_ch_l_combo_->currentData().toInt();
    g_config.deck_b_in_ch_r = deck_b_in_ch_r_combo_->currentData().toInt();

    g_config.aux_in_dev = aux_in_dev_combo_->currentData().toString();
    g_config.aux_in_ch_l = aux_in_ch_l_combo_->currentData().toInt();
    g_config.aux_in_ch_r = aux_in_ch_r_combo_->currentData().toInt();

    // Video
    g_config.video_resolution = video_res_combo_->currentIndex();
    g_config.default_scale_mode = video_scale_combo_->currentIndex();
    g_config.shader_quality = shader_quality_combo_->currentIndex();

    // MIDI
    g_config.midi_input_device = midi_in_combo_->currentText();
    g_config.midi_output_device = midi_out_combo_->currentText();
    g_config.active_preset = midi_preset_combo_->currentText();

    // General
    g_config.language = lang_combo_->currentData().toString();
    g_config.auto_save_on_exit = auto_save_check_->isChecked();
}

void SettingsDialog::onAudioDriverChanged(int index)
{
    bool is_asio = (audio_driver_combo_->currentData().toString() == QStringLiteral("ASIO"));
    asio_device_label_->setVisible(is_asio);
    asio_device_combo_->setVisible(is_asio);
    wasapi_device_label_->setVisible(!is_asio);
    wasapi_device_combo_->setVisible(!is_asio);
}

void SettingsDialog::onAudioModeChanged()
{
    // External Split Deck vs Internal
}

void SettingsDialog::onMidiPresetChanged(int index)
{
    // Change Preset
}

void SettingsDialog::onMidiExportClicked()
{
    const QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("MIDI Profil Exportálása"),
                                                    QStringLiteral("controller_preset.midimap.json"),
                                                    QStringLiteral("MIDI Térkép (*.midimap.json *.json)"));
    if (fn.isEmpty())
        return;

    QJsonObject root;
    root[QStringLiteral("name")] = midi_preset_combo_->currentText();
    root[QStringLiteral("version")] = 1;

    QJsonArray bindings;
    for (int r = 0; r < midi_learn_table_->rowCount(); ++r) {
        QJsonObject b;
        b[QStringLiteral("action")] = midi_learn_table_->item(r, 0)->text();
        b[QStringLiteral("binding")] = midi_learn_table_->item(r, 1)->text();
        bindings.append(b);
    }
    root[QStringLiteral("bindings")] = bindings;

    QFile file(fn);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        QMessageBox::information(this, QStringLiteral("Sikeres Export"), QStringLiteral("A MIDI profil sikeresen elmentve:\n") + fn);
    }
}

void SettingsDialog::onMidiImportClicked()
{
    const QString fn = QFileDialog::getOpenFileName(this, QStringLiteral("MIDI Profil Betöltése"),
                                                    QString(),
                                                    QStringLiteral("MIDI Térkép (*.midimap.json *.json)"));
    if (fn.isEmpty())
        return;

    QFile file(fn);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    const QString name = root.value(QStringLiteral("name")).toString(QStringLiteral("Egyedi Profil"));
    const QJsonArray bindings = root.value(QStringLiteral("bindings")).toArray();

    for (int r = 0; r < midi_learn_table_->rowCount() && r < bindings.size(); ++r) {
        const QJsonObject b = bindings[r].toObject();
        if (midi_learn_table_->item(r, 1)) {
            midi_learn_table_->item(r, 1)->setText(b.value(QStringLiteral("binding")).toString());
        }
    }
    QMessageBox::information(this, QStringLiteral("Sikeres Betöltés"), QStringLiteral("A '") + name + QStringLiteral("' profil sikeresen betöltve!"));
}

void SettingsDialog::onMidiLearnClicked(int row)
{
    if (auto *btn = qobject_cast<QPushButton *>(midi_learn_table_->cellWidget(row, 2))) {
        btn->setText(QStringLiteral("👂 FIGYEL..."));
        btn->setStyleSheet(QStringLiteral("QPushButton { background: #b91c1c; border: 1px solid #ef4444; color: #fff; font-size: 10px; font-weight: bold; }"));
        // Live MIDI learn hook
    }
}

void SettingsDialog::onSaveClicked()
{
    readUiToConfig();
    saveSettings();

    // Apply scale mode to DeckPair immediately
    if (auto *decks = vdj_decks()) {
        decks->setVideoScaleMode(g_config.default_scale_mode);
    }

    // Apply MIDI device change immediately
    if (!g_config.midi_input_device.isEmpty()) {
        int dev_idx = midi_in_combo_->currentIndex();
        if (dev_idx >= 0) {
            vdj_midi_open_input_device(dev_idx);
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
        QWidget *main_win = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_settings_dialog = new SettingsDialog(main_win);
    }
    g_settings_dialog->show();
    g_settings_dialog->raise();
    g_settings_dialog->activateWindow();
}

void vdj_free_settings_dialog()
{
    if (g_settings_dialog) {
        g_settings_dialog->close();
        delete g_settings_dialog;
        g_settings_dialog = nullptr;
    }
}
