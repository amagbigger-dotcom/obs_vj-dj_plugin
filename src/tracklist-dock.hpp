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
};

class TracklistDock : public QDialog {
    Q_OBJECT

public:
    explicit TracklistDock(QWidget *parent = nullptr);
    ~TracklistDock() override;

    void addFile(const QString &path, bool auto_convert = true);
    void loadLibraryJson();
    void saveLibraryJson();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onBrowseFolder();
    void onImportButtonClicked();
    void onAddDirectButtonClicked();
    void onRefreshStatus();
    void onTableItemDoubleClicked(int row, int column);

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
