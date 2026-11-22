#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QTimer>
#include <vector>
#include "video/video_converter.hpp"
#include "gui/playlist_manager.hpp"

struct TrackItem {
    QString title;
    QString file_path;
    QString original_path;
    double bpm = 128.0;
    QString key = QStringLiteral("--");
    QString camelot = QStringLiteral("--");
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
    void reconvertSelectedTrack();
    void reconvertTrack(int row);
    void cancelActiveConversion();
    void loadLibraryJson();
    void saveLibraryJson();
    void updateTrackBpm(const QString &file_path, double bpm);
    void setTrackBpmDirect(int row, double new_bpm);
    void moveSelection(int delta);
    void loadCurrentSelection(int deck_channel);
    const std::vector<TrackItem>& tracks() const { return tracks_; }
    void applyWindowPosition();

    void relinkTrackFile(int real_track_index);

protected:
    void showEvent(QShowEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onBrowseFolder();
    void onImportButtonClicked();
    void onReconvertButtonClicked();
    void onReanalyzeButtonClicked();
    void onRefreshStatus();
    void onTableItemDoubleClicked(int row, int column);
    void onCustomContextMenuRequested(const QPoint &pos);

    void onSlotMainClicked(int slot_idx);
    void onSlotMenuClicked(int slot_offset);
    void onNewPlaylistClicked();
    void onSearchTextChanged(const QString &text);
    void onHeaderSectionClicked(int logicalIndex);

private:
    void setupUi();
    void updateTable();
    void updateRow(int row);
    void applyFilterAndPopulate();
    void updateSlotButtons();
    void onEditPlaylist(const QString &playlist_id);
    void onDeletePlaylist(const QString &playlist_id);

    void processNextConversion();
    void processNextAnalysis();
    void cancelActiveAnalysis();
    void loadTrackToDeck(int track_index, int deck_channel); // 0 = A, 1 = B
    void analyzeTrackAudio(int track_index, bool force = false);

    VideoConverter::TargetResolution selectedTargetResolution() const;

    QString library_dir_{};
    std::vector<TrackItem> tracks_;
    std::vector<int> filtered_track_indices_;
    PlaylistManager playlist_mgr_;

    std::vector<int> conversion_queue_;
    int active_conversion_idx_ = -1;

    std::vector<int> analysis_queue_;
    int active_analysis_idx_ = -1;
    int total_analysis_count_ = 0;
    int done_analysis_count_ = 0;

    VideoConverter *converter_ = nullptr;
    QLineEdit *dir_edit_ = nullptr;
    QComboBox *res_combo_ = nullptr;
    QPushButton *reconv_btn_ = nullptr;
    QPushButton *reanalyze_btn_ = nullptr;
    QPushButton *cancel_btn_ = nullptr;
    QProgressBar *progress_bar_ = nullptr;
    QLabel *status_lbl_ = nullptr;

    // Playlist Quick Slots Bar
    struct QuickSlotWidget {
        QPushButton *main_btn = nullptr;
        QPushButton *menu_btn = nullptr;
    };
    QPushButton *btn_slot_all_ = nullptr;
    QuickSlotWidget slot_widgets_[3]; // Slot 2, Slot 3, Slot 4
    QPushButton *btn_new_playlist_ = nullptr;
    QLineEdit *search_edit_ = nullptr;

    QTableWidget *table_ = nullptr;
    QTimer *status_timer_ = nullptr;

    int sort_column_ = -1; // -1 = eredeti beillesztési sorrend
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;
};

// Global accessor to import & load from anywhere (e.g. Deck window)
void vdj_import_and_load(const QString &file_path, int deck_channel);
void vdj_tracklist_move_selection(int delta);
void vdj_tracklist_load_selected(int deck_channel);
void vdj_open_tracklist_window();
void vdj_free_tracklist_window();
TracklistDock *vdj_get_tracklist_window();
bool vdj_is_track_in_library(const QString &file_path);
void vdj_notify_track_bpm_changed(const QString &file_path, double new_bpm);
