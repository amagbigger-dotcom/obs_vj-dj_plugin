#include "controllers/midi_mapping.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

MidiControllerProfile::MidiControllerProfile(const QString &name, const QString &manufacturer)
    : name_(name), manufacturer_(manufacturer)
{
}

void MidiControllerProfile::rebuildLookup()
{
    lookup_.clear();
    for (size_t i = 0; i < bindings_.size(); ++i) {
        lookup_[bindings_[i].key()] = i;
    }
}

void MidiControllerProfile::addBinding(const MidiControlBinding &b)
{
    bindings_.push_back(b);
    lookup_[b.key()] = bindings_.size() - 1;
}

void MidiControllerProfile::clearBindings()
{
    bindings_.clear();
    lookup_.clear();
}

const MidiControlBinding *MidiControllerProfile::findBinding(MidiMsgType type, uint8_t channel, uint8_t control) const
{
    const uint32_t exact_key = (static_cast<uint32_t>(type) << 16) |
                               (static_cast<uint32_t>(channel) << 8) |
                               static_cast<uint32_t>(control);
    auto it = lookup_.find(exact_key);
    if (it != lookup_.end()) {
        return &bindings_[it->second];
    }
    // Fallback to any channel (0)
    const uint32_t any_key = (static_cast<uint32_t>(type) << 16) |
                             static_cast<uint32_t>(control);
    it = lookup_.find(any_key);
    if (it != lookup_.end()) {
        return &bindings_[it->second];
    }
    return nullptr;
}

QJsonObject MidiControllerProfile::toJson() const
{
    QJsonObject obj;
    obj["name"] = name_;
    obj["manufacturer"] = manufacturer_;

    QJsonArray arr;
    for (const auto &b : bindings_) {
        QJsonObject bo;
        bo["msg_type"] = static_cast<int>(b.msg_type);
        bo["channel"] = static_cast<int>(b.channel);
        bo["control"] = static_cast<int>(b.control);
        bo["action"] = midi_action_to_name(b.action);
        bo["deck"] = b.deck;
        bo["jog_mode"] = static_cast<int>(b.jog_mode);
        bo["comment"] = b.comment;
        arr.append(bo);
    }
    obj["bindings"] = arr;
    return obj;
}

bool MidiControllerProfile::fromJson(const QJsonObject &obj)
{
    name_ = obj.value("name").toString();
    manufacturer_ = obj.value("manufacturer").toString();
    clearBindings();

    const QJsonArray arr = obj.value("bindings").toArray();
    for (const auto &val : arr) {
        const QJsonObject bo = val.toObject();
        MidiControlBinding b;
        b.msg_type = static_cast<MidiMsgType>(bo.value("msg_type").toInt(0));
        b.channel = static_cast<uint8_t>(bo.value("channel").toInt(1));
        b.control = static_cast<uint8_t>(bo.value("control").toInt(0));
        b.action = midi_name_to_action(bo.value("action").toString().toStdString());
        b.deck = bo.value("deck").toInt(0);
        b.jog_mode = static_cast<MidiJogMode>(bo.value("jog_mode").toInt(0));
        b.comment = bo.value("comment").toString();
        addBinding(b);
    }
    return true;
}

bool MidiControllerProfile::saveToFile(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool MidiControllerProfile::loadFromFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull() || !doc.isObject()) return false;
    return fromJson(doc.object());
}

MidiControllerProfile MidiControllerProfile::createTraktorS2MK3()
{
    MidiControllerProfile p("Traktor Kontrol S2 MK3", "Native Instruments");
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Master Crossfader"});
    // Deck A
    p.addBinding({MidiMsgType::CC, 1, 16, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 17, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 18, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 19, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 1, 20, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 1, 21, MidiAction::TEMPO_SPEED, 1, MidiJogMode::SPREAD_64, "Tempo Fader"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 35, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 36, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 37, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 38, MidiAction::HEADPHONE_CUE, 1, MidiJogMode::SPREAD_64, "Headphone CUE"});

    // Deck B
    p.addBinding({MidiMsgType::CC, 2, 16, MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 2, 17, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 2, 18, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 2, 19, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::CC, 2, 20, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 2, 21, MidiAction::TEMPO_SPEED, 2, MidiJogMode::SPREAD_64, "Tempo Fader"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 35, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 36, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 37, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 38, MidiAction::HEADPHONE_CUE, 2, MidiJogMode::SPREAD_64, "Headphone CUE"});
    return p;
}

MidiControllerProfile MidiControllerProfile::createKorgKaossDJ()
{
    MidiControllerProfile p("Kaoss DJ", "Korg");
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader"});
    // Kaoss Touchpad mapped to VJ Shader
    p.addBinding({MidiMsgType::CC, 1, 12, MidiAction::VJ_PARAM_X, 0, MidiJogMode::ABSOLUTE, "Kaoss Touchpad X -> VJ P0"});
    p.addBinding({MidiMsgType::CC, 1, 13, MidiAction::VJ_PARAM_Y, 0, MidiJogMode::ABSOLUTE, "Kaoss Touchpad Y -> VJ P1"});

    // Deck A (Ch 1)
    p.addBinding({MidiMsgType::CC, 1, 19, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 1, 20, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 21, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 22, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 23, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 42, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 43, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 44, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});

    // Deck B (Ch 2)
    p.addBinding({MidiMsgType::CC, 2, 19, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 2, 20, MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 2, 21, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 2, 22, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 2, 23, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 42, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 43, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 44, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync"});
    return p;
}

MidiControllerProfile MidiControllerProfile::createPioneerDDJ400()
{
    MidiControllerProfile p("DDJ-400", "Pioneer DJ");
    p.addBinding({MidiMsgType::CC, 1, 31, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader"});
    // Deck A (Ch 1)
    p.addBinding({MidiMsgType::CC, 1, 19, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Channel Fader"});
    p.addBinding({MidiMsgType::CC, 1, 7,  MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 11, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 15, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 23, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 11, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 12, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 88, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});

    // Deck B (Ch 2)
    p.addBinding({MidiMsgType::CC, 2, 19, MidiAction::VOLUME, 2, MidiJogMode::SPREAD_64, "Channel Fader"});
    p.addBinding({MidiMsgType::CC, 2, 7,  MidiAction::EQ_HI, 2, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 2, 11, MidiAction::EQ_MID, 2, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 2, 15, MidiAction::EQ_LOW, 2, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 2, 23, MidiAction::FILTER, 2, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 11, MidiAction::PLAY_PAUSE, 2, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 12, MidiAction::CUE, 2, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 2, 88, MidiAction::SYNC, 2, MidiJogMode::SPREAD_64, "Sync"});
    return p;
}

MidiControllerProfile MidiControllerProfile::createPioneerDDJFLX4()
{
    MidiControllerProfile p = createPioneerDDJ400();
    p.setName("DDJ-FLX4");
    return p;
}

MidiControllerProfile MidiControllerProfile::createNumarkMixtrackPro()
{
    MidiControllerProfile p("Mixtrack Pro / FX", "Numark");
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::BIN_OFFSET, "Crossfader"});
    // Deck A
    p.addBinding({MidiMsgType::CC, 1, 0, MidiAction::TEMPO_SPEED, 1, MidiJogMode::BIN_OFFSET, "Pitch Fader"});
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::VOLUME, 1, MidiJogMode::BIN_OFFSET, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 1, 12, MidiAction::EQ_HI, 1, MidiJogMode::BIN_OFFSET, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 13, MidiAction::EQ_MID, 1, MidiJogMode::BIN_OFFSET, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 14, MidiAction::EQ_LOW, 1, MidiJogMode::BIN_OFFSET, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 15, MidiAction::FILTER, 1, MidiJogMode::BIN_OFFSET, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 0, MidiAction::PLAY_PAUSE, 1, MidiJogMode::BIN_OFFSET, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 1, MidiAction::CUE, 1, MidiJogMode::BIN_OFFSET, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 2, MidiAction::SYNC, 1, MidiJogMode::BIN_OFFSET, "Sync"});
    return p;
}

MidiControllerProfile MidiControllerProfile::createHerculesInpulse300()
{
    MidiControllerProfile p("DJControl Inpulse 300", "Hercules");
    p.addBinding({MidiMsgType::CC, 1, 8, MidiAction::CROSSFADER, 0, MidiJogMode::SPREAD_64, "Crossfader"});
    // Deck A
    p.addBinding({MidiMsgType::CC, 1, 1, MidiAction::VOLUME, 1, MidiJogMode::SPREAD_64, "Volume Fader"});
    p.addBinding({MidiMsgType::CC, 1, 2, MidiAction::EQ_HI, 1, MidiJogMode::SPREAD_64, "High EQ"});
    p.addBinding({MidiMsgType::CC, 1, 3, MidiAction::EQ_MID, 1, MidiJogMode::SPREAD_64, "Mid EQ"});
    p.addBinding({MidiMsgType::CC, 1, 4, MidiAction::EQ_LOW, 1, MidiJogMode::SPREAD_64, "Low EQ"});
    p.addBinding({MidiMsgType::CC, 1, 5, MidiAction::FILTER, 1, MidiJogMode::SPREAD_64, "Filter"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 1, MidiAction::PLAY_PAUSE, 1, MidiJogMode::SPREAD_64, "Play/Pause"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 2, MidiAction::CUE, 1, MidiJogMode::SPREAD_64, "CUE"});
    p.addBinding({MidiMsgType::NOTE_ON, 1, 3, MidiAction::SYNC, 1, MidiJogMode::SPREAD_64, "Sync"});
    return p;
}

std::vector<MidiControllerProfile> MidiControllerProfile::getBuiltinProfiles()
{
    return {
        createTraktorS2MK3(),
        createKorgKaossDJ(),
        createPioneerDDJ400(),
        createPioneerDDJFLX4(),
        createNumarkMixtrackPro(),
        createHerculesInpulse300()
    };
}
