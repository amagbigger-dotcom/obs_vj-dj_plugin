#pragma once

#include "controllers/midi_types.hpp"
#include <QString>
#include <QJsonObject>
#include <vector>
#include <unordered_map>

struct MidiControlBinding {
    MidiMsgType msg_type = MidiMsgType::CC;
    uint8_t channel = 1; // 1..16, 0 = any
    uint8_t control = 0; // CC # or Note #
    MidiAction action = MidiAction::NONE;
    int deck = 0;        // 0 = Master/Global, 1 = Deck A, 2 = Deck B
    MidiJogMode jog_mode = MidiJogMode::SPREAD_64;
    QString comment;

    uint32_t key() const {
        return (static_cast<uint32_t>(msg_type) << 16) |
               (static_cast<uint32_t>(channel) << 8) |
               static_cast<uint32_t>(control);
    }
};

class MidiControllerProfile {
public:
    MidiControllerProfile() = default;
    MidiControllerProfile(const QString &name, const QString &manufacturer);

    QString name() const { return name_; }
    QString manufacturer() const { return manufacturer_; }
    const std::vector<MidiControlBinding> &bindings() const { return bindings_; }

    void setName(const QString &n) { name_ = n; }
    void setManufacturer(const QString &m) { manufacturer_ = m; }

    void addBinding(const MidiControlBinding &b);
    void clearBindings();
    const MidiControlBinding *findBinding(MidiMsgType type, uint8_t channel, uint8_t control) const;

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &obj);
    bool saveToFile(const QString &path) const;
    bool loadFromFile(const QString &path);

    // Factory profiles directly mapped from hardware specs & reference repo
    static std::vector<MidiControllerProfile> getBuiltinProfiles();
    static MidiControllerProfile createTraktorS2MK3();
    static MidiControllerProfile createKorgKaossDJ();
    static MidiControllerProfile createPioneerDDJ400();
    static MidiControllerProfile createPioneerDDJFLX4();
    static MidiControllerProfile createNumarkMixtrackPro();
    static MidiControllerProfile createHerculesInpulse300();

private:
    QString name_;
    QString manufacturer_;
    std::vector<MidiControlBinding> bindings_;
    std::unordered_map<uint32_t, size_t> lookup_;

    void rebuildLookup();
};
