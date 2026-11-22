#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>

struct Playlist {
    QString id;
    QString name;
    QString genre;
    QString filter_keyword;
    std::vector<QString> track_paths;
};

class PlaylistManager {
public:
    PlaylistManager();

    const std::vector<Playlist>& playlists() const { return playlists_; }

    QString createPlaylist(const QString &name, const QString &genre = QString(), const QString &filter_keyword = QString());
    bool updatePlaylist(const QString &id, const QString &new_name, const QString &new_genre, const QString &new_filter);
    bool deletePlaylist(const QString &id);

    Playlist* findPlaylist(const QString &id);
    const Playlist* findPlaylist(const QString &id) const;

    // Quick Slots: slot_idx 0, 1, 2 corresponds to Slot 2, Slot 3, Slot 4 (Slot 1 is always fixed to "All")
    void setQuickSlot(int slot_idx, const QString &playlist_id);
    QString getQuickSlot(int slot_idx) const;

    void setActiveSlot(int slot) { active_slot_ = slot; }
    int activeSlot() const { return active_slot_; }
    QString activePlaylistId() const;

    bool addTrackToPlaylist(const QString &playlist_id, const QString &file_path);
    bool removeTrackFromPlaylist(const QString &playlist_id, const QString &file_path);
    bool isTrackInPlaylist(const QString &playlist_id, const QString &file_path, const QString &title, double bpm) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

private:
    std::vector<Playlist> playlists_;
    QString quick_slots_[3]; // Slot 2, Slot 3, Slot 4 IDs
    int active_slot_ = 0;    // 0 = All, 1 = Slot 2, 2 = Slot 3, 3 = Slot 4
};
