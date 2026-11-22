#include "gui/playlist_manager.hpp"
#include <QDateTime>
#include <algorithm>
#include <cmath>

PlaylistManager::PlaylistManager()
{
    quick_slots_[0] = QString();
    quick_slots_[1] = QString();
    quick_slots_[2] = QString();
}

QString PlaylistManager::createPlaylist(const QString &name, const QString &genre, const QString &filter_keyword)
{
    const QString trimmed_name = name.trimmed();
    if (trimmed_name.isEmpty()) {
        return QString();
    }

    Playlist pl;
    pl.id = QStringLiteral("pl_%1").arg(QDateTime::currentMSecsSinceEpoch());
    pl.name = trimmed_name;
    pl.genre = genre.trimmed();
    pl.filter_keyword = filter_keyword.trimmed();

    playlists_.push_back(pl);

    // If any quick slot is empty, auto assign this newly created playlist
    for (int i = 0; i < 3; ++i) {
        if (quick_slots_[i].isEmpty()) {
            quick_slots_[i] = pl.id;
            break;
        }
    }

    return pl.id;
}

bool PlaylistManager::updatePlaylist(const QString &id, const QString &new_name, const QString &new_genre, const QString &new_filter)
{
    Playlist *pl = findPlaylist(id);
    if (!pl) return false;

    const QString trimmed_name = new_name.trimmed();
    if (!trimmed_name.isEmpty()) {
        pl->name = trimmed_name;
    }
    pl->genre = new_genre.trimmed();
    pl->filter_keyword = new_filter.trimmed();
    return true;
}

bool PlaylistManager::deletePlaylist(const QString &id)
{
    auto it = std::remove_if(playlists_.begin(), playlists_.end(), [&id](const Playlist &p) {
        return p.id == id;
    });
    if (it == playlists_.end()) {
        return false;
    }
    playlists_.erase(it, playlists_.end());

    // Clear from quick slots if assigned
    for (int i = 0; i < 3; ++i) {
        if (quick_slots_[i] == id) {
            quick_slots_[i].clear();
        }
    }

    if (activePlaylistId() == id) {
        active_slot_ = 0; // fallback to All
    }

    return true;
}

Playlist* PlaylistManager::findPlaylist(const QString &id)
{
    if (id.isEmpty()) return nullptr;
    for (auto &p : playlists_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const Playlist* PlaylistManager::findPlaylist(const QString &id) const
{
    if (id.isEmpty()) return nullptr;
    for (const auto &p : playlists_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

void PlaylistManager::setQuickSlot(int slot_idx, const QString &playlist_id)
{
    if (slot_idx >= 0 && slot_idx < 3) {
        quick_slots_[slot_idx] = playlist_id;
    }
}

QString PlaylistManager::getQuickSlot(int slot_idx) const
{
    if (slot_idx >= 0 && slot_idx < 3) {
        return quick_slots_[slot_idx];
    }
    return QString();
}

QString PlaylistManager::activePlaylistId() const
{
    if (active_slot_ >= 1 && active_slot_ <= 3) {
        return quick_slots_[active_slot_ - 1];
    }
    return QString();
}

bool PlaylistManager::addTrackToPlaylist(const QString &playlist_id, const QString &file_path)
{
    Playlist *pl = findPlaylist(playlist_id);
    if (!pl || file_path.isEmpty()) return false;

    // Check if already in list
    for (const auto &path : pl->track_paths) {
        if (path == file_path) return false;
    }

    pl->track_paths.push_back(file_path);
    return true;
}

bool PlaylistManager::removeTrackFromPlaylist(const QString &playlist_id, const QString &file_path)
{
    Playlist *pl = findPlaylist(playlist_id);
    if (!pl || file_path.isEmpty()) return false;

    auto it = std::remove(pl->track_paths.begin(), pl->track_paths.end(), file_path);
    if (it != pl->track_paths.end()) {
        pl->track_paths.erase(it, pl->track_paths.end());
        return true;
    }
    return false;
}

bool PlaylistManager::isTrackInPlaylist(const QString &playlist_id, const QString &file_path, const QString &title, double bpm) const
{
    const Playlist *pl = findPlaylist(playlist_id);
    if (!pl) return false;

    // 1. Explicit track paths
    for (const auto &p : pl->track_paths) {
        if (p == file_path) return true;
    }

    // 2. Smart filter keyword (if defined)
    if (!pl->filter_keyword.isEmpty()) {
        const QString kw = pl->filter_keyword.toLower();
        if (title.toLower().contains(kw) || file_path.toLower().contains(kw) || pl->genre.toLower().contains(kw)) {
            return true;
        }
        // Also check if keyword is a BPM target e.g. "128"
        bool ok = false;
        double target_bpm = kw.toDouble(&ok);
        if (ok && std::abs(bpm - target_bpm) <= 1.0) {
            return true;
        }
    }

    return false;
}

QJsonObject PlaylistManager::toJson() const
{
    QJsonObject root;
    QJsonArray pl_arr;
    for (const auto &pl : playlists_) {
        QJsonObject o;
        o[QStringLiteral("id")] = pl.id;
        o[QStringLiteral("name")] = pl.name;
        o[QStringLiteral("genre")] = pl.genre;
        o[QStringLiteral("filter_keyword")] = pl.filter_keyword;

        QJsonArray paths;
        for (const auto &path : pl.track_paths) {
            paths.append(path);
        }
        o[QStringLiteral("track_paths")] = paths;
        pl_arr.append(o);
    }
    root[QStringLiteral("playlists")] = pl_arr;

    QJsonArray slots_arr;
    for (int i = 0; i < 3; ++i) {
        slots_arr.append(quick_slots_[i]);
    }
    root[QStringLiteral("quick_slots")] = slots_arr;
    root[QStringLiteral("active_slot")] = active_slot_;

    return root;
}

void PlaylistManager::fromJson(const QJsonObject &obj)
{
    playlists_.clear();
    const QJsonArray pl_arr = obj[QStringLiteral("playlists")].toArray();
    for (const auto &val : pl_arr) {
        const QJsonObject o = val.toObject();
        Playlist pl;
        pl.id = o[QStringLiteral("id")].toString();
        pl.name = o[QStringLiteral("name")].toString();
        pl.genre = o[QStringLiteral("genre")].toString();
        pl.filter_keyword = o[QStringLiteral("filter_keyword")].toString();

        const QJsonArray paths = o[QStringLiteral("track_paths")].toArray();
        for (const auto &p_val : paths) {
            pl.track_paths.push_back(p_val.toString());
        }
        if (!pl.id.isEmpty() && !pl.name.isEmpty()) {
            playlists_.push_back(pl);
        }
    }

    const QJsonArray slots_arr = obj[QStringLiteral("quick_slots")].toArray();
    for (int i = 0; i < 3; ++i) {
        if (i < slots_arr.size()) {
            quick_slots_[i] = slots_arr[i].toString();
            // Validate slot points to an existing playlist
            if (!quick_slots_[i].isEmpty() && !findPlaylist(quick_slots_[i])) {
                quick_slots_[i].clear();
            }
        } else {
            quick_slots_[i].clear();
        }
    }

    active_slot_ = obj[QStringLiteral("active_slot")].toInt(0);
    if (active_slot_ < 0 || active_slot_ > 3) {
        active_slot_ = 0;
    }
}
