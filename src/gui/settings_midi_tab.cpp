#include "gui/settings_midi_tab.hpp"
#include "gui/settings_dialog.hpp"
#include "controllers/midi_handler.hpp"
#include "controllers/midi_dispatcher.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>

SettingsMidiTab::SettingsMidiTab(QWidget *parent)
    : QWidget(parent)
{
    builtin_profiles_ = MidiControllerProfile::getBuiltinProfiles();
    setupUi();
    populatePresets();

    // Setup learn callback
    MidiDispatcher::instance().setLearnCallback([this](MidiMsgType type, uint8_t ch, uint8_t ctrl, MidiAction action, int deck) {
        Q_UNUSED(action);
        Q_UNUSED(deck);
        QMetaObject::invokeMethod(this, [this, type, ch, ctrl]() {
            if (active_learn_row_ >= 0 && active_learn_row_ < learn_table_->rowCount()) {
                QString type_str = (type == MidiMsgType::CC) ? "CC" : "Note";
                learn_table_->setItem(active_learn_row_, 1, new QTableWidgetItem(QString("%1 %2 (Ch %3)").arg(type_str).arg(ctrl).arg(ch)));
                if (auto *btn = qobject_cast<QPushButton*>(learn_table_->cellWidget(active_learn_row_, 2))) {
                    btn->setText(QStringLiteral("🔴 LEARN"));
                    btn->setStyleSheet(QString());
                }
                active_learn_row_ = -1;
            }
        }, Qt::QueuedConnection);
    });

    // Setup live activity monitor callback
    MidiDispatcher::instance().setMonitorCallback([this](uint8_t status, uint8_t d1, uint8_t d2) {
        const uint8_t ch = (status & 0x0F) + 1;
        const uint8_t type_nib = status & 0xF0;
        QString type_str = (type_nib == 0xB0) ? "CC" : (type_nib == 0x90 ? "Note" : "Other");
        QMetaObject::invokeMethod(this, [this, ch, type_str, d1, d2]() {
            if (midi_activity_lbl_) {
                midi_activity_lbl_->setText(QStringLiteral("🟢 ÉLŐ JEL ÉRKEZETT: Ch %1 | %2 %3 | Érték: %4")
                    .arg(ch).arg(type_str).arg(d1).arg(d2));
                midi_activity_lbl_->setStyleSheet(QStringLiteral("color: #22c55e; font-weight: bold; font-size: 11px;"));
            }
        }, Qt::QueuedConnection);
    });
}

void SettingsMidiTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 1. Hardware Selection
    auto *hw_group = new QGroupBox(QStringLiteral("🎹 MIDI INTERFÉSZ ÉS GYÁRI KONTROLLER PROFILOK"), this);
    auto *hw_grid = new QGridLayout(hw_group);
    hw_grid->setSpacing(8);

    hw_grid->addWidget(new QLabel(QStringLiteral("MIDI Bemeneti Eszköz:"), hw_group), 0, 0);
    midi_in_combo_ = new QComboBox(hw_group);
    connect(midi_in_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int idx) {
        if (idx >= 0) {
            vdj_midi_open_input_device(idx);
        }
    });
    hw_grid->addWidget(midi_in_combo_, 0, 1, 1, 2);

    hw_grid->addWidget(new QLabel(QStringLiteral("MIDI Kimeneti Eszköz (Clock/LED):"), hw_group), 1, 0);
    midi_out_combo_ = new QComboBox(hw_group);
    connect(midi_out_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int idx) {
        if (idx >= 0) {
            vdj_midi_open_output_device(idx);
        }
    });
    hw_grid->addWidget(midi_out_combo_, 1, 1, 1, 2);

    hw_grid->addWidget(new QLabel(QStringLiteral("Gyári Kontroller Térkép (Mapping):"), hw_group), 2, 0);
    preset_combo_ = new QComboBox(hw_group);
    connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsMidiTab::onMidiPresetChanged);
    hw_grid->addWidget(preset_combo_, 2, 1);

    auto *preset_btn_layout = new QHBoxLayout();
    auto *export_btn = new QPushButton(QStringLiteral("📥 Export JSON"), hw_group);
    connect(export_btn, &QPushButton::clicked, this, &SettingsMidiTab::onExportClicked);
    preset_btn_layout->addWidget(export_btn);

    auto *import_btn = new QPushButton(QStringLiteral("📤 Import JSON"), hw_group);
    connect(import_btn, &QPushButton::clicked, this, &SettingsMidiTab::onImportClicked);
    preset_btn_layout->addWidget(import_btn);
    hw_grid->addLayout(preset_btn_layout, 2, 2);

    hw_grid->addWidget(new QLabel(QStringLiteral("Élő MIDI Visszajelzés:"), hw_group), 3, 0);
    midi_activity_lbl_ = new QLabel(QStringLiteral("⚪ Várakozás bemeneti MIDI jelre..."), hw_group);
    midi_activity_lbl_->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    hw_grid->addWidget(midi_activity_lbl_, 3, 1, 1, 2);

    layout->addWidget(hw_group);

    // 2. MIDI Learn Table
    auto *learn_group = new QGroupBox(QStringLiteral("🎯 INTERAKTÍV MIDI LEARN & VEZÉRLŐK TÁBLÁZATA"), this);
    auto *learn_vbox = new QVBoxLayout(learn_group);

    learn_table_ = new QTableWidget(learn_group);
    learn_table_->setColumnCount(3);
    learn_table_->setHorizontalHeaderLabels({QStringLiteral("Funkció"), QStringLiteral("Hozzárendelt MIDI Címke"), QStringLiteral("Művelet")});
    learn_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    learn_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    learn_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    learn_table_->setColumnWidth(2, 120);
    learn_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    learn_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    learn_table_->verticalHeader()->setVisible(false);

    refreshLearnTable();
    learn_vbox->addWidget(learn_table_);
    layout->addWidget(learn_group);
}

void SettingsMidiTab::populatePresets()
{
    preset_combo_->clear();
    for (const auto &p : builtin_profiles_) {
        preset_combo_->addItem(QString("[%1] %2").arg(p.manufacturer(), p.name()), p.name());
    }
}

void SettingsMidiTab::populateDevices()
{
    midi_in_combo_->clear();
    const auto midi_in = vdj_midi_get_input_devices();
    if (midi_in.empty()) {
        midi_in_combo_->addItem(QStringLiteral("Nincs csatlakoztatott MIDI bemenet"));
        midi_in_combo_->setEnabled(false);
    } else {
        for (const auto &dev : midi_in) midi_in_combo_->addItem(QString::fromStdString(dev));
        midi_in_combo_->setEnabled(true);
    }

    midi_out_combo_->clear();
    const auto midi_out = vdj_midi_get_output_devices();
    if (midi_out.empty()) {
        midi_out_combo_->addItem(QStringLiteral("Nincs csatlakoztatott MIDI kimenet"));
        midi_out_combo_->setEnabled(false);
    } else {
        for (const auto &dev : midi_out) midi_out_combo_->addItem(QString::fromStdString(dev));
        midi_out_combo_->setEnabled(true);
    }
}

void SettingsMidiTab::refreshLearnTable()
{
    struct TargetItem { QString name; MidiAction action; int deck; };
    const std::vector<TargetItem> targets = {
        {QStringLiteral("◀ Deck A Play / Pause"), MidiAction::PLAY_PAUSE, 1},
        {QStringLiteral("◀ Deck A CUE"), MidiAction::CUE, 1},
        {QStringLiteral("◀ Deck A SYNC"), MidiAction::SYNC, 1},
        {QStringLiteral("◀ Deck A Hangerő (Volume)"), MidiAction::VOLUME, 1},
        {QStringLiteral("◀ Deck A Gain"), MidiAction::GAIN, 1},
        {QStringLiteral("◀ Deck A High EQ"), MidiAction::EQ_HI, 1},
        {QStringLiteral("◀ Deck A Mid EQ"), MidiAction::EQ_MID, 1},
        {QStringLiteral("◀ Deck A Low EQ"), MidiAction::EQ_LOW, 1},
        {QStringLiteral("◀ Deck A Filter"), MidiAction::FILTER, 1},
        {QStringLiteral("◀ Deck A Loop Toggle"), MidiAction::LOOP_TOGGLE, 1},
        {QStringLiteral("◀ Deck A Pitch Bend (<<)"), MidiAction::PITCH_BEND_DOWN, 1},
        {QStringLiteral("◀ Deck A Pitch Bend (>>)"), MidiAction::PITCH_BEND_UP, 1},
        {QStringLiteral("◀ Deck A Load Track"), MidiAction::LOAD_SELECTED_A, 1},

        {QStringLiteral("🎚 Master Crossfader"), MidiAction::CROSSFADER, 0},

        {QStringLiteral("▶ Deck B Play / Pause"), MidiAction::PLAY_PAUSE, 2},
        {QStringLiteral("▶ Deck B CUE"), MidiAction::CUE, 2},
        {QStringLiteral("▶ Deck B SYNC"), MidiAction::SYNC, 2},
        {QStringLiteral("▶ Deck B Hangerő (Volume)"), MidiAction::VOLUME, 2},
        {QStringLiteral("▶ Deck B Gain"), MidiAction::GAIN, 2},
        {QStringLiteral("▶ Deck B High EQ"), MidiAction::EQ_HI, 2},
        {QStringLiteral("▶ Deck B Mid EQ"), MidiAction::EQ_MID, 2},
        {QStringLiteral("▶ Deck B Low EQ"), MidiAction::EQ_LOW, 2},
        {QStringLiteral("▶ Deck B Filter"), MidiAction::FILTER, 2},
        {QStringLiteral("▶ Deck B Loop Toggle"), MidiAction::LOOP_TOGGLE, 2},
        {QStringLiteral("▶ Deck B Pitch Bend (<<)"), MidiAction::PITCH_BEND_DOWN, 2},
        {QStringLiteral("▶ Deck B Pitch Bend (>>)"), MidiAction::PITCH_BEND_UP, 2},
        {QStringLiteral("▶ Deck B Load Track"), MidiAction::LOAD_SELECTED_B, 2},

        {QStringLiteral("📁 Tracklist Browse Scroll"), MidiAction::BROWSE_SCROLL, 0},
        {QStringLiteral("📁 Tracklist Load Push"), MidiAction::LOAD_SELECTED, 0},
        {QStringLiteral("✨ VJ Touchpad X / Param P0"), MidiAction::VJ_PARAM_X, 0},
        {QStringLiteral("✨ VJ Touchpad Y / Param P1"), MidiAction::VJ_PARAM_Y, 0}
    };

    learn_table_->setRowCount(static_cast<int>(targets.size()));
    for (int r = 0; r < static_cast<int>(targets.size()); ++r) {
        const auto &t = targets[r];
        learn_table_->setItem(r, 0, new QTableWidgetItem(t.name));

        // Find existing binding
        QString bind_str = QStringLiteral("--");
        for (const auto &b : current_profile_.bindings()) {
            if (b.action == t.action && b.deck == t.deck) {
                QString type_str = (b.msg_type == MidiMsgType::CC) ? "CC" : "Note";
                bind_str = QString("%1 %2 (Ch %3)").arg(type_str).arg(b.control).arg(b.channel);
                break;
            }
        }
        learn_table_->setItem(r, 1, new QTableWidgetItem(bind_str));

        auto *btn = new QPushButton(QStringLiteral("🔴 LEARN"), learn_table_);
        connect(btn, &QPushButton::clicked, this, [this, r, t]() { onLearnClicked(r); });
        learn_table_->setCellWidget(r, 2, btn);
    }
}

void SettingsMidiTab::onMidiPresetChanged(int index)
{
    if (index >= 0 && index < static_cast<int>(builtin_profiles_.size())) {
        current_profile_ = builtin_profiles_[index];
        MidiDispatcher::instance().setProfile(current_profile_);
        MidiDispatcher::instance().sendFeedback(true);
        refreshLearnTable();
    }
}

void SettingsMidiTab::onExportClicked()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("MIDI Profil Exportálása"), QString(), QStringLiteral("JSON Fájl (*.midimap.json)"));
    if (!path.isEmpty()) {
        if (current_profile_.saveToFile(path)) {
            QMessageBox::information(this, QStringLiteral("Siker"), QStringLiteral("A MIDI profil mentése sikeres!"));
        }
    }
}

void SettingsMidiTab::onImportClicked()
{
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("MIDI Profil Betöltése"), QString(), QStringLiteral("JSON Fájl (*.midimap.json *.json)"));
    if (!path.isEmpty()) {
        MidiControllerProfile p;
        if (p.loadFromFile(path)) {
            current_profile_ = p;
            MidiDispatcher::instance().setProfile(current_profile_);
            refreshLearnTable();
            QMessageBox::information(this, QStringLiteral("Siker"), QStringLiteral("A MIDI profil sikeresen betöltve: ") + p.name());
        }
    }
}

void SettingsMidiTab::onLearnClicked(int row)
{
    struct TargetAction { MidiAction action; int deck; };
    const std::vector<TargetAction> targets = {
        {MidiAction::PLAY_PAUSE, 1}, {MidiAction::CUE, 1}, {MidiAction::SYNC, 1}, {MidiAction::VOLUME, 1},
        {MidiAction::EQ_HI, 1}, {MidiAction::EQ_MID, 1}, {MidiAction::EQ_LOW, 1}, {MidiAction::FILTER, 1},
        {MidiAction::CROSSFADER, 0},
        {MidiAction::PLAY_PAUSE, 2}, {MidiAction::CUE, 2}, {MidiAction::SYNC, 2}, {MidiAction::VOLUME, 2},
        {MidiAction::EQ_HI, 2}, {MidiAction::EQ_MID, 2}, {MidiAction::EQ_LOW, 2}, {MidiAction::FILTER, 2},
        {MidiAction::VJ_PARAM_X, 0}, {MidiAction::VJ_PARAM_Y, 0}
    };
    if (row < 0 || row >= static_cast<int>(targets.size())) return;

    active_learn_row_ = row;
    auto *btn = qobject_cast<QPushButton*>(learn_table_->cellWidget(row, 2));
    if (btn) {
        btn->setText(QStringLiteral("⏳ Nyomj gombot..."));
        btn->setStyleSheet(QStringLiteral("background: #dc2626; color: white; font-weight: bold;"));
    }
    MidiDispatcher::instance().startLearn(targets[row].action, targets[row].deck);
}

void SettingsMidiTab::applyConfig(const PluginSettingsConfig &cfg)
{
    if (!cfg.midi_input_device.isEmpty()) {
        int idx = midi_in_combo_->findText(cfg.midi_input_device);
        if (idx >= 0) midi_in_combo_->setCurrentIndex(idx);
    }
    if (!cfg.midi_output_device.isEmpty()) {
        int idx = midi_out_combo_->findText(cfg.midi_output_device);
        if (idx >= 0) midi_out_combo_->setCurrentIndex(idx);
    }
    int p_idx = preset_combo_->findData(cfg.active_preset);
    if (p_idx >= 0) {
        preset_combo_->setCurrentIndex(p_idx);
        onMidiPresetChanged(p_idx);
    } else {
        // Use active profile from dispatcher (which may have been auto-detected by MidiHandler)
        current_profile_ = MidiDispatcher::instance().activeProfile();
        int active_idx = preset_combo_->findData(current_profile_.name());
        if (active_idx >= 0) {
            preset_combo_->setCurrentIndex(active_idx);
        }
        refreshLearnTable();
    }
}

void SettingsMidiTab::readConfig(PluginSettingsConfig &cfg)
{
    cfg.midi_input_device = midi_in_combo_->currentText();
    cfg.midi_output_device = midi_out_combo_->currentText();
    cfg.active_preset = preset_combo_->currentData().toString();
}
