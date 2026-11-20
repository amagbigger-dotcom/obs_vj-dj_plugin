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

    // Fallback for NOTE_OFF: if a control was bound as NOTE_ON, allow finding it on NOTE_OFF
    if (type == MidiMsgType::NOTE_OFF) {
        const uint32_t note_on_exact = (static_cast<uint32_t>(MidiMsgType::NOTE_ON) << 16) |
                                       (static_cast<uint32_t>(channel) << 8) |
                                       static_cast<uint32_t>(control);
        it = lookup_.find(note_on_exact);
        if (it != lookup_.end()) {
            return &bindings_[it->second];
        }
        const uint32_t note_on_any = (static_cast<uint32_t>(MidiMsgType::NOTE_ON) << 16) |
                                     static_cast<uint32_t>(control);
        it = lookup_.find(note_on_any);
        if (it != lookup_.end()) {
            return &bindings_[it->second];
        }
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
