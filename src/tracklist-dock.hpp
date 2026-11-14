#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QTimer>
#include <vector>
#include "video-converter.hpp"

struct TrackItem {
    QString title;
    QString file_path;
    QString original_path;
    double bpm = 128.0;
    int64_t duration_ms = 0;
    bool is_converted = false;
    bool played = false;
    QString status = QStringLiteral("Ready");
    int target_deck_after_conv = -1; // -1 = none, 0 = A, 1 = B
};

class TracklistDock : public QDialog {
    Q_OBJECT

public:
    explicit TracklistDock(QWidget *parent = nullptr);
    ~TracklistDock() override;

    void addFile(const QString &path, int target_deck = -1);
    void deleteSelectedTrack();
    void loadLibraryJson();
    void saveLibraryJson();
    const std::vector<TrackItem>& tracks() const { return tracks_; }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onBrowseFolder();
    void onImportButtonClicked();
    void onRefreshStatus();
    void onTableItemDoubleClicked(int row, int column);
    void onCustomContextMenuRequested(const QPoint &pos);

private:
    void setupUi();
    void updateTable();
    void updateRow(int row);
    void processNextConversion();
    void loadTrackToDeck(int track_index, int deck_channel); // 0 = A, 1 = B
    void analyzeTrackAudio(int track_index);

    QString library_dir_{};
    std::vector<TrackItem> tracks_;
    std::vector<int> conversion_queue_;
    int active_conversion_idx_ = -1;

    VideoConverter *converter_ = nullptr;
    QLineEdit *dir_edit_ = nullptr;
    QProgressBar *progress_bar_ = nullptr;
    QLabel *status_lbl_ = nullptr;
    QTableWidget *table_ = nullptr;
    QTimer *status_timer_ = nullptr;
};

// Global accessor to import & load from anywhere (e.g. Deck window)
void vdj_import_and_load(const QString &file_path, int deck_channel);
void vdj_open_tracklist_window();
bool vdj_is_track_in_library(const QString &file_path);
