#include "gui/settings_audio_tab.hpp"
#include "gui/settings_dialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>

#include <obs-frontend-api.h>
#include <windows.h>
#include <mmsystem.h>

namespace {

struct MonitoringDeviceItem {
    QString name;
    QString id;
};

std::vector<std::string> get_system_asio_drivers()
{
    std::vector<std::string> drivers;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASIO", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char subKeyName[256];
        DWORD index = 0;
        DWORD nameLen = sizeof(subKeyName);
        while (RegEnumKeyExA(hKey, index, subKeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            drivers.push_back(subKeyName);
            index++;
            nameLen = sizeof(subKeyName);
        }
        RegCloseKey(hKey);
    }
    return drivers;
}

std::vector<MonitoringDeviceItem> get_wasapi_devices()
{
    std::vector<MonitoringDeviceItem> devices;
    devices.push_back({QStringLiteral("Alapértelmezett (Default)"), QStringLiteral("default")});

    auto enum_cb = [](void *param, const char *name, const char *id) -> bool {
        auto *list = static_cast<std::vector<MonitoringDeviceItem>*>(param);
        if (name && id) {
            list->push_back({QString::fromUtf8(name), QString::fromUtf8(id)});
        }
        return true;
    };
    obs_enum_audio_monitoring_devices(enum_cb, &devices);
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

} // namespace

SettingsAudioTab::SettingsAudioTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SettingsAudioTab::populateChannelCombo(QComboBox *combo, int selected_ch, int max_ch)
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
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void SettingsAudioTab::setupUi()
{
    auto *outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(0, 0, 0, 0);

    auto *scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);

    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    // 1. Hardware & Driver Section
    auto *drv_group = new QGroupBox(QStringLiteral("🔊 HANGKÁRTYA ÉS ILLESZTŐPROGRAM (AUDIO DRIVER)"), page);
    auto *drv_grid = new QGridLayout(drv_group);
    drv_grid->setSpacing(8);

    drv_grid->addWidget(new QLabel(QStringLiteral("Hangkezelő Rendszer:"), drv_group), 0, 0);
    audio_driver_combo_ = new QComboBox(drv_group);
    audio_driver_combo_->addItem(QStringLiteral("WASAPI (Standard Windows Audio)"), QStringLiteral("WASAPI"));
    audio_driver_combo_->addItem(QStringLiteral("ASIO (Professzionális Alacsony Késleltetés)"), QStringLiteral("ASIO"));
    connect(audio_driver_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsAudioTab::onAudioDriverChanged);
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

    auto add_out_row = [&](int row, const QString &label, QComboBox **dev, QComboBox **chl, QComboBox **chr) {
        out_grid->addWidget(new QLabel(label, out_group), row, 0);
        *dev = new QComboBox(out_group);
        *chl = new QComboBox(out_group);
        *chr = new QComboBox(out_group);
        out_grid->addWidget(*dev, row, 1);
        out_grid->addWidget(*chl, row, 2);
        out_grid->addWidget(*chr, row, 3);
    };

    add_out_row(1, QStringLiteral("🔊 Főkimenet (Master Out):"), &master_dev_combo_, &master_ch_l_combo_, &master_ch_r_combo_);
    add_out_row(2, QStringLiteral("🎧 Fejhallgató CUE (PFL Out):"), &cue_dev_combo_, &cue_ch_l_combo_, &cue_ch_r_combo_);
    add_out_row(3, QStringLiteral("🔈 Monitor / Booth Out:"), &booth_dev_combo_, &booth_ch_l_combo_, &booth_ch_r_combo_);
    add_out_row(4, QStringLiteral("◀ Deck A Közvetlen Kimenet:"), &deck_a_out_dev_combo_, &deck_a_out_ch_l_combo_, &deck_a_out_ch_r_combo_);
    add_out_row(5, QStringLiteral("▶ Deck B Közvetlen Kimenet:"), &deck_b_out_dev_combo_, &deck_b_out_ch_l_combo_, &deck_b_out_ch_r_combo_);
    layout->addWidget(out_group);

    // 3. Input Routing Matrix
    auto *in_group = new QGroupBox(QStringLiteral("🎤 BEMENETI CSATORNÁK KIOSZTÁSA (INPUT ROUTING MATRIX)"), page);
    auto *in_grid = new QGridLayout(in_group);
    in_grid->setSpacing(6);

    in_grid->addWidget(new QLabel(QStringLiteral("<b>Bemeneti Funkció</b>"), in_group), 0, 0);
    in_grid->addWidget(new QLabel(QStringLiteral("<b>Bemeneti Eszköz</b>"), in_group), 0, 1);
    in_grid->addWidget(new QLabel(QStringLiteral("<b>Bal Bemenet (L)</b>"), in_group), 0, 2);
    in_grid->addWidget(new QLabel(QStringLiteral("<b>Jobb Bemenet (R)</b>"), in_group), 0, 3);

    auto add_in_row = [&](int row, const QString &label, QComboBox **dev, QComboBox **chl, QComboBox **chr) {
        in_grid->addWidget(new QLabel(label, in_group), row, 0);
        *dev = new QComboBox(in_group);
        *chl = new QComboBox(in_group);
        *chr = new QComboBox(in_group);
        in_grid->addWidget(*dev, row, 1);
        in_grid->addWidget(*chl, row, 2);
        in_grid->addWidget(*chr, row, 3);
    };

    add_in_row(1, QStringLiteral("◀ Deck A Vonal/Phono In:"), &deck_a_in_dev_combo_, &deck_a_in_ch_l_combo_, &deck_a_in_ch_r_combo_);
    add_in_row(2, QStringLiteral("▶ Deck B Vonal/Phono In:"), &deck_b_in_dev_combo_, &deck_b_in_ch_l_combo_, &deck_b_in_ch_r_combo_);
    add_in_row(3, QStringLiteral("🎙 Aux / Mikrofon Bemenet:"), &aux_in_dev_combo_, &aux_in_ch_l_combo_, &aux_in_ch_r_combo_);
    layout->addWidget(in_group);

    scroll_area->setWidget(page);
    outer_layout->addWidget(scroll_area);
}

void SettingsAudioTab::populateDevices()
{
    const auto asio_drivers = get_system_asio_drivers();
    asio_device_combo_->clear();
    if (asio_drivers.empty()) {
        asio_device_combo_->addItem(QStringLiteral("Nincs telepített ASIO illesztőprogram"));
        asio_device_combo_->setEnabled(false);
    } else {
        for (const auto &drv : asio_drivers) asio_device_combo_->addItem(QString::fromStdString(drv));
        asio_device_combo_->setEnabled(true);
    }

    wasapi_device_combo_->clear();
    const auto wasapi_devs = get_wasapi_devices();
    for (const auto &d : wasapi_devs) wasapi_device_combo_->addItem(d.name, d.id);

    auto populate_out_devs = [&](QComboBox *combo) {
        if (!combo) return;
        combo->clear();
        combo->addItem(QStringLiteral("Alapértelmezett (Default Master)"), QStringLiteral("default"));
        for (const auto &d : wasapi_devs) {
            if (d.id != QStringLiteral("default")) combo->addItem(d.name, d.id);
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

    const auto in_devs = get_winmm_input_devices();
    auto populate_in_devs = [&](QComboBox *combo) {
        if (!combo) return;
        combo->clear();
        combo->addItem(QStringLiteral("-- Nincs Bemenet (Disabled) --"), QString());
        for (const auto &indev : in_devs) combo->addItem(QString::fromStdString(indev), QString::fromStdString(indev));
        for (const auto &drv : asio_drivers) {
            combo->addItem(QStringLiteral("ASIO In: ") + QString::fromStdString(drv), QString::fromStdString(drv));
        }
    };
    populate_in_devs(deck_a_in_dev_combo_);
    populate_in_devs(deck_b_in_dev_combo_);
    populate_in_devs(aux_in_dev_combo_);
}

void SettingsAudioTab::onAudioDriverChanged(int index)
{
    const bool is_asio = (index == 1);
    asio_device_label_->setVisible(is_asio);
    asio_device_combo_->setVisible(is_asio);
    wasapi_device_label_->setVisible(!is_asio);
    wasapi_device_combo_->setVisible(!is_asio);
}

void SettingsAudioTab::applyConfig(const PluginSettingsConfig &cfg)
{
    int drv_idx = audio_driver_combo_->findData(cfg.audio_driver);
    if (drv_idx >= 0) audio_driver_combo_->setCurrentIndex(drv_idx);

    int asio_idx = asio_device_combo_->findText(cfg.asio_device_name);
    if (asio_idx >= 0) asio_device_combo_->setCurrentIndex(asio_idx);

    int wasapi_idx = wasapi_device_combo_->findData(cfg.wasapi_device_id);
    if (wasapi_idx >= 0) wasapi_device_combo_->setCurrentIndex(wasapi_idx);

    int sr_idx = sample_rate_combo_->findData(cfg.sample_rate);
    if (sr_idx >= 0) sample_rate_combo_->setCurrentIndex(sr_idx);

    int buf_idx = buffer_size_combo_->findData(cfg.buffer_size);
    if (buf_idx >= 0) buffer_size_combo_->setCurrentIndex(buf_idx);

    if (cfg.audio_mode == 1) mode_external_radio_->setChecked(true);
    else mode_internal_radio_->setChecked(true);

    onAudioDriverChanged(audio_driver_combo_->currentIndex());

    auto set_dev_and_ch = [](QComboBox *dev_c, QComboBox *chl_c, QComboBox *chr_c,
                             const QString &dev, int chl, int chr) {
        if (dev_c) {
            int idx = dev_c->findData(dev);
            if (idx < 0) idx = dev_c->findText(dev);
            if (idx >= 0) dev_c->setCurrentIndex(idx);
        }
        populateChannelCombo(chl_c, chl);
        populateChannelCombo(chr_c, chr);
    };

    set_dev_and_ch(master_dev_combo_, master_ch_l_combo_, master_ch_r_combo_, cfg.master_out_dev, cfg.master_out_ch_l, cfg.master_out_ch_r);
    set_dev_and_ch(cue_dev_combo_, cue_ch_l_combo_, cue_ch_r_combo_, cfg.cue_out_dev, cfg.cue_out_ch_l, cfg.cue_out_ch_r);
    set_dev_and_ch(booth_dev_combo_, booth_ch_l_combo_, booth_ch_r_combo_, cfg.booth_out_dev, cfg.booth_out_ch_l, cfg.booth_out_ch_r);
    set_dev_and_ch(deck_a_out_dev_combo_, deck_a_out_ch_l_combo_, deck_a_out_ch_r_combo_, cfg.deck_a_out_dev, cfg.deck_a_out_ch_l, cfg.deck_a_out_ch_r);
    set_dev_and_ch(deck_b_out_dev_combo_, deck_b_out_ch_l_combo_, deck_b_out_ch_r_combo_, cfg.deck_b_out_dev, cfg.deck_b_out_ch_l, cfg.deck_b_out_ch_r);

    set_dev_and_ch(deck_a_in_dev_combo_, deck_a_in_ch_l_combo_, deck_a_in_ch_r_combo_, cfg.deck_a_in_dev, cfg.deck_a_in_ch_l, cfg.deck_a_in_ch_r);
    set_dev_and_ch(deck_b_in_dev_combo_, deck_b_in_ch_l_combo_, deck_b_in_ch_r_combo_, cfg.deck_b_in_dev, cfg.deck_b_in_ch_l, cfg.deck_b_in_ch_r);
    set_dev_and_ch(aux_in_dev_combo_, aux_in_ch_l_combo_, aux_in_ch_r_combo_, cfg.aux_in_dev, cfg.aux_in_ch_l, cfg.aux_in_ch_r);
}

void SettingsAudioTab::readConfig(PluginSettingsConfig &cfg)
{
    cfg.audio_driver = audio_driver_combo_->currentData().toString();
    cfg.asio_device_name = asio_device_combo_->currentText();
    cfg.wasapi_device_id = wasapi_device_combo_->currentData().toString();
    cfg.sample_rate = sample_rate_combo_->currentData().toInt();
    cfg.buffer_size = buffer_size_combo_->currentData().toInt();
    cfg.audio_mode = mode_external_radio_->isChecked() ? 1 : 0;

    auto read_dev_and_ch = [](QComboBox *dev_c, QComboBox *chl_c, QComboBox *chr_c,
                              QString &dev, int &chl, int &chr) {
        if (dev_c) dev = dev_c->currentData().toString();
        if (chl_c) chl = chl_c->currentData().toInt();
        if (chr_c) chr = chr_c->currentData().toInt();
    };

    read_dev_and_ch(master_dev_combo_, master_ch_l_combo_, master_ch_r_combo_, cfg.master_out_dev, cfg.master_out_ch_l, cfg.master_out_ch_r);
    read_dev_and_ch(cue_dev_combo_, cue_ch_l_combo_, cue_ch_r_combo_, cfg.cue_out_dev, cfg.cue_out_ch_l, cfg.cue_out_ch_r);
    read_dev_and_ch(booth_dev_combo_, booth_ch_l_combo_, booth_ch_r_combo_, cfg.booth_out_dev, cfg.booth_out_ch_l, cfg.booth_out_ch_r);
    read_dev_and_ch(deck_a_out_dev_combo_, deck_a_out_ch_l_combo_, deck_a_out_ch_r_combo_, cfg.deck_a_out_dev, cfg.deck_a_out_ch_l, cfg.deck_a_out_ch_r);
    read_dev_and_ch(deck_b_out_dev_combo_, deck_b_out_ch_l_combo_, deck_b_out_ch_r_combo_, cfg.deck_b_out_dev, cfg.deck_b_out_ch_l, cfg.deck_b_out_ch_r);

    read_dev_and_ch(deck_a_in_dev_combo_, deck_a_in_ch_l_combo_, deck_a_in_ch_r_combo_, cfg.deck_a_in_dev, cfg.deck_a_in_ch_l, cfg.deck_a_in_ch_r);
    read_dev_and_ch(deck_b_in_dev_combo_, deck_b_in_ch_l_combo_, deck_b_in_ch_r_combo_, cfg.deck_b_in_dev, cfg.deck_b_in_ch_l, cfg.deck_b_in_ch_r);
    read_dev_and_ch(aux_in_dev_combo_, aux_in_ch_l_combo_, aux_in_ch_r_combo_, cfg.aux_in_dev, cfg.aux_in_ch_l, cfg.aux_in_ch_r);
}
