#include "tracklist-dock.hpp"
#include "deck-engine.hpp"
#include "audio-analyzer.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QMenu>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

static TracklistDock *g_tracklist_instance = nullptr;

void vdj_open_tracklist_window()
{
    if (!g_tracklist_instance) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_tracklist_instance = new TracklistDock(main_window);
    }
    g_tracklist_instance->show();
    g_tracklist_instance->raise();
    g_tracklist_instance->activateWindow();
}

void vdj_import_and_load(const QString &file_path, int deck_channel)
{
    if (!g_tracklist_instance) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_tracklist_instance = new TracklistDock(main_window);
    }
    g_tracklist_instance->addFile(file_path, deck_channel);
}

static bool isAudioOnlyFile(const QString &ext)
{
    const QString e = ext.toLower();
    return (e == "mp3" || e == "wav" || e == "flac" || e == "aac" || e == "ogg" || e == "m4a" || e == "wma");
}

static QString formatDuration(int64_t ms)
{
    if (ms <= 0) return QStringLiteral("--:--");
    const int total_sec = static_cast<int>(ms / 1000);
    const int mins = total_sec / 60;
    const int secs = total_sec % 60;
    return QStringLiteral("%1:%2")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

TracklistDock::TracklistDock(QWidget *parent)
    : QDialog(parent)
{
    g_tracklist_instance = this;
    setWindowTitle(QStringLiteral("OBS VJ Tracklist - Médiatár & 1080p Konverter"));
    setMinimumSize(880, 500);
    resize(940, 560);
    setAcceptDrops(true);

    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #12151c; color: #e0e4ec; font-family: 'Segoe UI', Arial, sans-serif; }"
        "QLabel { color: #cfd6e4; font-size: 11px; }"
        "QLineEdit { background: #181d26; color: #ffffff; border: 1px solid #2e3e54; border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QPushButton { background: #222834; border: 1px solid #384255; border-radius: 4px; color: #ffffff; padding: 4px 10px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background: #2f384a; border-color: #00d2ff; }"
        "QPushButton:pressed { background: #181d26; }"
        "QTableWidget { background-color: #151821; alternate-background-color: #191d28; color: #d6dce8; border: 1px solid #242c3c; gridline-color: #1e2432; font-size: 12px; }"
        "QTableWidget::item:selected { background-color: #1e3248; color: #00d2ff; }"
        "QHeaderView::section { background-color: #1c2230; color: #7f8fa4; font-weight: bold; font-size: 11px; border: none; border-bottom: 2px solid #283448; padding: 6px; }"
        "QProgressBar { background-color: #181d26; border: 1px solid #2e3e54; border-radius: 3px; text-align: center; color: #ffffff; font-weight: bold; font-size: 10px; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0088cc, stop:1 #00d2ff); border-radius: 2px; }"
    ));

    // Default library directory in Videos/VJ_Clips
    const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    library_dir_ = QDir(movies).filePath(QStringLiteral("VJ_Clips"));
    QDir().mkpath(library_dir_);

    converter_ = new VideoConverter(this);
    connect(converter_, &VideoConverter::progressChanged, this, [this](int pct) {
        if (progress_bar_) {
            progress_bar_->setValue(pct);
            if (active_conversion_idx_ >= 0 && active_conversion_idx_ < static_cast<int>(tracks_.size())) {
                tracks_[active_conversion_idx_].status = QStringLiteral("Conv %1%").arg(pct);
                updateRow(active_conversion_idx_);
            }
        }
    });

    connect(converter_, &VideoConverter::conversionFinished, this, [this](const QString &out_path, bool success, const QString &err) {
        if (active_conversion_idx_ >= 0 && active_conversion_idx_ < static_cast<int>(tracks_.size())) {
            auto &item = tracks_[active_conversion_idx_];
            const int deck_target = item.target_deck_after_conv;
            if (success) {
                item.file_path = out_path;
                item.is_converted = true;
                item.status = QStringLiteral("Ready");
                analyzeTrackAudio(active_conversion_idx_);

                if (deck_target >= 0) {
                    loadTrackToDeck(active_conversion_idx_, deck_target);
                }
            } else {
                item.status = QStringLiteral("Hiba: %1").arg(err);
                blog(LOG_WARNING, "[obs_vj_tracklist] Conversion failed: %s", err.toUtf8().constData());
            }
            updateRow(active_conversion_idx_);
            saveLibraryJson();
        }

        active_conversion_idx_ = -1;
        if (progress_bar_) {
            progress_bar_->setValue(100);
            progress_bar_->setVisible(false);
        }
        processNextConversion();
    });

    setupUi();
    loadLibraryJson();

    status_timer_ = new QTimer(this);
    connect(status_timer_, &QTimer::timeout, this, &TracklistDock::onRefreshStatus);
    status_timer_->start(500);
}

TracklistDock::~TracklistDock()
{
    if (g_tracklist_instance == this) {
        g_tracklist_instance = nullptr;
    }
    saveLibraryJson();
    if (status_timer_) {
        status_timer_->stop();
        delete status_timer_;
    }
}

void TracklistDock::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(10);

    // ==========================================
    // Top Bar: Library path, Browse, Actions
    // ==========================================
    auto *top_bar = new QHBoxLayout();
    top_bar->setSpacing(8);

    auto *lbl_folder = new QLabel(QStringLiteral("📁 VJ Mappa:"), this);
    lbl_folder->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4;"));
    top_bar->addWidget(lbl_folder);

    dir_edit_ = new QLineEdit(library_dir_, this);
    dir_edit_->setReadOnly(true);
    top_bar->addWidget(dir_edit_, 1);

    auto *browse_btn = new QPushButton(QStringLiteral("Tallózás..."), this);
    connect(browse_btn, &QPushButton::clicked, this, &TracklistDock::onBrowseFolder);
    top_bar->addWidget(browse_btn);

    auto *import_btn = new QPushButton(QStringLiteral("⚡ IMPORT MEDIA"), this);
    import_btn->setStyleSheet(QStringLiteral("background: #0088cc; border: 1px solid #00aaff; color: #ffffff; padding: 5px 12px; font-size: 11px;"));
    import_btn->setToolTip(QStringLiteral("Médiafájlok behúzása: a rendszer automatikusan észleli a formátumot, és csak a videókat alakítja 1080p-re az RTX 3060-nal"));
    connect(import_btn, &QPushButton::clicked, this, &TracklistDock::onImportButtonClicked);
    top_bar->addWidget(import_btn);

    auto *del_btn = new QPushButton(QStringLiteral("🗑 TÖRLÉS"), this);
    del_btn->setStyleSheet(QStringLiteral("background: #3a1c1c; border: 1px solid #703838; color: #ff8888; padding: 5px 10px; font-size: 11px;"));
    del_btn->setToolTip(QStringLiteral("Kijelölt szám törlése a listából (Delete gomb)"));
    connect(del_btn, &QPushButton::clicked, this, &TracklistDock::deleteSelectedTrack);
    top_bar->addWidget(del_btn);

    main_layout->addLayout(top_bar);

    // Status bar & Progress
    auto *status_bar = new QHBoxLayout();
    status_bar->setSpacing(8);

    status_lbl_ = new QLabel(QStringLiteral("Húzz ide tetszőleges videót vagy dalt. Az új videókat az RTX 3060 automatikusan 1080p All-Intra formátumra készíti elő."), this);
    status_lbl_->setStyleSheet(QStringLiteral("color: #7f8fa4; font-size: 11px;"));
    status_bar->addWidget(status_lbl_, 1);

    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setFixedWidth(160);
    progress_bar_->setFixedHeight(18);
    progress_bar_->setVisible(false);
    status_bar->addWidget(progress_bar_);

    main_layout->addLayout(status_bar);

    // ==========================================
    // Table: Status, Title, BPM, Time, Format, Load
    // ==========================================
    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("ST"),
        QStringLiteral("CÍM / FÁJLNÉV"),
        QStringLiteral("BPM"),
        QStringLiteral("IDŐ"),
        QStringLiteral("FORMÁTUM"),
        QStringLiteral("BETÖLTÉS A DECKBE")
    });

    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(true);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->setColumnWidth(0, 48);

    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->setColumnWidth(2, 75);

    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->setColumnWidth(3, 70);

    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    table_->setColumnWidth(4, 105);

    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    table_->setColumnWidth(5, 175);

    connect(table_, &QTableWidget::cellDoubleClicked, this, &TracklistDock::onTableItemDoubleClicked);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &TracklistDock::onCustomContextMenuRequested);

    // Keyboard shortcut: Delete key
    auto *del_shortcut = new QShortcut(QKeySequence::Delete, table_);
    connect(del_shortcut, &QShortcut::activated, this, &TracklistDock::deleteSelectedTrack);

    main_layout->addWidget(table_, 1);
}

void TracklistDock::onBrowseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("VJ Mappa Kiválasztása"),
        library_dir_
    );
    if (!dir.isEmpty() && dir != library_dir_) {
        library_dir_ = dir;
        if (dir_edit_) dir_edit_->setText(library_dir_);
        loadLibraryJson();
    }
}

void TracklistDock::onImportButtonClicked()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Médiafájlok kiválasztása"),
        QString(),
        QStringLiteral("Media Files (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.ts *.mp3 *.wav *.flac *.aac);;All Files (*.*)")
    );
    for (const auto &f : files) {
        addFile(f, -1);
    }
}

void TracklistDock::deleteSelectedTrack()
{
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(tracks_.size())) {
        return;
    }

    const auto &item = tracks_[row];
    const auto res = QMessageBox::question(
        this,
        QStringLiteral("Track törlése a listából"),
        QStringLiteral("Biztosan törölni szeretnéd a listából a következőt:\n\n\"%1\"?").arg(item.title),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (res == QMessageBox::Yes) {
        // Also delete the .vdj.json sidecar cache file so it never gets stale!
        const QString orig_cache = item.original_path + QStringLiteral(".vdj.json");
        const QString conv_cache = item.converted_path + QStringLiteral(".vdj.json");
        if (QFile::exists(orig_cache)) QFile::remove(orig_cache);
        if (QFile::exists(conv_cache)) QFile::remove(conv_cache);

        tracks_.erase(tracks_.begin() + row);
        updateTable();
        saveLibraryJson();
    }
}

bool vdj_is_track_in_library(const QString &file_path)
{
    if (!g_tracklist_instance) return false;
    for (const auto &t : g_tracklist_instance->tracks()) {
        if (t.original_path == file_path || t.converted_path == file_path)
            return true;
    }
    return false;
}

void TracklistDock::onCustomContextMenuRequested(const QPoint &pos)
{
    const int row = table_->rowAt(pos.y());
    if (row < 0 || row >= static_cast<int>(tracks_.size())) {
        return;
    }

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: #1a2230; color: #ffffff; border: 1px solid #2e3e54; }"
        "QMenu::item:selected { background-color: #0088cc; }"
    ));

    auto *act_a = menu.addAction(QStringLiteral("▶ Betöltés a DECK A-ba"));
    auto *act_b = menu.addAction(QStringLiteral("▶ Betöltés a DECK B-be"));
    menu.addSeparator();
    auto *act_del = menu.addAction(QStringLiteral("🗑 Törlés a listából"));

    QAction *selected = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (selected == act_a) {
        loadTrackToDeck(row, 0);
    } else if (selected == act_b) {
        loadTrackToDeck(row, 1);
    } else if (selected == act_del) {
        deleteSelectedTrack();
    }
}

void TracklistDock::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TracklistDock::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TracklistDock::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const auto &u : urls) {
        const QString path = u.toLocalFile();
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            addFile(path, -1);
        }
    }
    event->acceptProposedAction();
}

void TracklistDock::addFile(const QString &path, int target_deck)
{
    const QFileInfo fi(path);
    if (!fi.exists()) return;

    // Check if already in list
    for (size_t i = 0; i < tracks_.size(); i++) {
        if (tracks_[i].original_path == path || tracks_[i].file_path == path) {
            if (target_deck >= 0) {
                loadTrackToDeck(static_cast<int>(i), target_deck);
            }
            return;
        }
    }

    TrackItem item;
    item.title = fi.completeBaseName();
    item.original_path = path;
    item.file_path = path;
    item.played = false;
    item.target_deck_after_conv = target_deck;

    const QString ext = fi.suffix().toLower();
    const bool is_audio = isAudioOnlyFile(ext);

    // 1. Audio-only files do not need video conversion
    if (is_audio) {
        item.is_converted = true;
        item.status = QStringLiteral("Ready");
        tracks_.push_back(item);
        const int idx = static_cast<int>(tracks_.size()) - 1;
        updateTable();
        analyzeTrackAudio(idx);
        saveLibraryJson();
        if (target_deck >= 0) {
            loadTrackToDeck(idx, target_deck);
        }
        return;
    }

    // 2. Check if already a converted 1080p VJ file
    if (path.endsWith(QStringLiteral("_vj1080.mp4"))) {
        item.is_converted = true;
        item.status = QStringLiteral("Ready");
        tracks_.push_back(item);
        const int idx = static_cast<int>(tracks_.size()) - 1;
        updateTable();
        analyzeTrackAudio(idx);
        saveLibraryJson();
        if (target_deck >= 0) {
            loadTrackToDeck(idx, target_deck);
        }
        return;
    }

    // 3. Check if converted version already exists in library_dir_
    const QString existing_conv = QDir(library_dir_).filePath(fi.completeBaseName() + QStringLiteral("_vj1080.mp4"));
    if (QFileInfo::exists(existing_conv)) {
        item.file_path = existing_conv;
        item.is_converted = true;
        item.status = QStringLiteral("Ready");
        tracks_.push_back(item);
        const int idx = static_cast<int>(tracks_.size()) - 1;
        updateTable();
        analyzeTrackAudio(idx);
        saveLibraryJson();
        if (target_deck >= 0) {
            loadTrackToDeck(idx, target_deck);
        }
        return;
    }

    // 4. Requires NVENC 1080p conversion
    item.status = QStringLiteral("Várakozik...");
    tracks_.push_back(item);
    const int idx = static_cast<int>(tracks_.size()) - 1;
    conversion_queue_.push_back(idx);
    updateTable();
    processNextConversion();
}

void TracklistDock::processNextConversion()
{
    if (active_conversion_idx_ >= 0 || conversion_queue_.empty()) {
        return;
    }

    active_conversion_idx_ = conversion_queue_.front();
    conversion_queue_.erase(conversion_queue_.begin());

    if (active_conversion_idx_ < 0 || active_conversion_idx_ >= static_cast<int>(tracks_.size())) {
        active_conversion_idx_ = -1;
        return;
    }

    auto &item = tracks_[active_conversion_idx_];
    item.status = QStringLiteral("Conv 0%");
    updateRow(active_conversion_idx_);

    if (progress_bar_) {
        progress_bar_->setValue(0);
        progress_bar_->setVisible(true);
    }
    if (status_lbl_) {
        status_lbl_->setText(QStringLiteral("Konvertálás (RTX 3060 NVENC 1080p): %1").arg(item.title));
    }

    const bool started = converter_->convertTo1080p(item.original_path, library_dir_);
    if (!started) {
        item.status = QStringLiteral("Nem indítható");
        updateRow(active_conversion_idx_);
        active_conversion_idx_ = -1;
        if (progress_bar_) progress_bar_->setVisible(false);
        processNextConversion();
    }
}

void TracklistDock::analyzeTrackAudio(int track_index)
{
    if (track_index < 0 || track_index >= static_cast<int>(tracks_.size())) return;
    const QString fpath = tracks_[track_index].file_path;

    AudioAnalyzer::analyzeFileAsync(fpath, [this, track_index](TrackAnalysisResult res) {
        if (res.success && track_index < static_cast<int>(tracks_.size())) {
            tracks_[track_index].bpm = res.detected_bpm;
            tracks_[track_index].duration_ms = res.duration_ms;
            updateRow(track_index);
            saveLibraryJson();
        }
    });
}

void TracklistDock::updateTable()
{
    table_->setRowCount(static_cast<int>(tracks_.size()));
    for (int i = 0; i < static_cast<int>(tracks_.size()); i++) {
        updateRow(i);
    }
}

void TracklistDock::updateRow(int row)
{
    if (row < 0 || row >= static_cast<int>(tracks_.size())) return;
    const auto &item = tracks_[row];

    // Col 0: Status
    auto *st_item = table_->item(row, 0);
    if (!st_item) {
        st_item = new QTableWidgetItem();
        st_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 0, st_item);
    }

    const QString fn = item.file_path;
    bool is_deck_a = (vj_deck_a() && vj_deck_a()->filePath() == fn && vj_deck_a()->isPlaying());
    bool is_deck_b = (vj_deck_b() && vj_deck_b()->filePath() == fn && vj_deck_b()->isPlaying());

    if (is_deck_a) {
        st_item->setText(QStringLiteral("▶ A"));
        st_item->setForeground(QColor(0, 210, 255));
    } else if (is_deck_b) {
        st_item->setText(QStringLiteral("▶ B"));
        st_item->setForeground(QColor(255, 140, 0));
    } else if (item.played) {
        st_item->setText(QStringLiteral("✓"));
        st_item->setForeground(QColor(0, 255, 120));
    } else {
        st_item->setText(item.status == QStringLiteral("Ready") ? QString() : QStringLiteral("⏳"));
        st_item->setForeground(QColor(160, 175, 195));
    }

    // Col 1: Title
    auto *title_item = table_->item(row, 1);
    if (!title_item) {
        title_item = new QTableWidgetItem();
        table_->setItem(row, 1, title_item);
    }
    title_item->setText(item.title);
    if (item.played && !is_deck_a && !is_deck_b) {
        title_item->setForeground(QColor(130, 145, 165));
    } else {
        title_item->setForeground(QColor(230, 236, 245));
    }

    // Col 2: BPM
    auto *bpm_item = table_->item(row, 2);
    if (!bpm_item) {
        bpm_item = new QTableWidgetItem();
        bpm_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 2, bpm_item);
    }
    bpm_item->setText(item.bpm > 20.0 ? QStringLiteral("%1").arg(item.bpm, 0, 'f', 2) : QStringLiteral("--"));
    bpm_item->setForeground(QColor(0, 210, 255));

    // Col 3: Duration
    auto *dur_item = table_->item(row, 3);
    if (!dur_item) {
        dur_item = new QTableWidgetItem();
        dur_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 3, dur_item);
    }
    dur_item->setText(formatDuration(item.duration_ms));
    dur_item->setForeground(QColor(255, 170, 0));

    // Col 4: Format
    auto *fmt_item = table_->item(row, 4);
    if (!fmt_item) {
        fmt_item = new QTableWidgetItem();
        fmt_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 4, fmt_item);
    }
    if (item.is_converted) {
        fmt_item->setText(isAudioOnlyFile(QFileInfo(item.file_path).suffix()) ? QStringLiteral("Audio") : QStringLiteral("1080p VJ"));
        fmt_item->setForeground(QColor(0, 255, 136));
    } else {
        fmt_item->setText(item.status);
        fmt_item->setForeground(QColor(160, 175, 195));
    }

    // Col 5: Load Actions Widget
    if (!table_->cellWidget(row, 5)) {
        auto *action_widget = new QWidget(table_);
        auto *act_layout = new QHBoxLayout(action_widget);
        act_layout->setContentsMargins(4, 2, 4, 2);
        act_layout->setSpacing(4);

        auto *load_a = new QPushButton(QStringLiteral("LOAD A"), action_widget);
        load_a->setStyleSheet(QStringLiteral(
            "QPushButton { background: #006699; border: 1px solid #00aaff; color: #ffffff; font-weight: bold; font-size: 10px; border-radius: 3px; padding: 3px 6px; }"
            "QPushButton:hover { background: #0088cc; }"
        ));
        connect(load_a, &QPushButton::clicked, this, [this, row]() {
            loadTrackToDeck(row, 0);
        });
        act_layout->addWidget(load_a);

        auto *load_b = new QPushButton(QStringLiteral("LOAD B"), action_widget);
        load_b->setStyleSheet(QStringLiteral(
            "QPushButton { background: #a04400; border: 1px solid #ff7700; color: #ffffff; font-weight: bold; font-size: 10px; border-radius: 3px; padding: 3px 6px; }"
            "QPushButton:hover { background: #c05500; }"
        ));
        connect(load_b, &QPushButton::clicked, this, [this, row]() {
            loadTrackToDeck(row, 1);
        });
        act_layout->addWidget(load_b);

        table_->setCellWidget(row, 5, action_widget);
    }
}

void TracklistDock::loadTrackToDeck(int track_index, int deck_channel)
{
    if (track_index < 0 || track_index >= static_cast<int>(tracks_.size())) return;
    auto &item = tracks_[track_index];

    DeckPlayer *target_deck = (deck_channel == 1) ? vj_deck_b() : vj_deck_a();
    if (!target_deck) {
        target_deck = vj_active_deck();
    }

    if (target_deck) {
        target_deck->loadFile(item.file_path);
        item.played = true;
        updateRow(track_index);
        saveLibraryJson();

        // Auto snap scene item to 1080p
        obs_source_t *scene_source = obs_frontend_get_current_scene();
        if (scene_source) {
            obs_scene_t *scene = obs_scene_from_source(scene_source);
            if (scene) {
                const char *src_names[] = {"OBS VJ Deck", "OBS VJ Deck A", "OBS VJ Deck B"};
                for (const char *name : src_names) {
                    obs_sceneitem_t *s_item = obs_scene_find_source(scene, name);
                    if (s_item) {
                        struct vec2 pos = {0.0f, 0.0f};
                        struct vec2 scale = {1.0f, 1.0f};
                        obs_sceneitem_set_pos(s_item, &pos);
                        obs_sceneitem_set_scale(s_item, &scale);
                        obs_sceneitem_set_bounds_type(s_item, OBS_BOUNDS_NONE);
                    }
                }
            }
            obs_source_release(scene_source);
        }
    }
}

void TracklistDock::onTableItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    loadTrackToDeck(row, 0); // Default load to Deck A on double-click
}

void TracklistDock::onRefreshStatus()
{
    for (int i = 0; i < static_cast<int>(tracks_.size()); i++) {
        updateRow(i);
    }
}

void TracklistDock::saveLibraryJson()
{
    if (library_dir_.isEmpty()) return;
    const QString json_path = QDir(library_dir_).filePath(QStringLiteral("vj_tracklist.json"));

    QJsonArray arr;
    for (const auto &item : tracks_) {
        QJsonObject obj;
        obj[QStringLiteral("title")] = item.title;
        obj[QStringLiteral("file_path")] = item.file_path;
        obj[QStringLiteral("original_path")] = item.original_path;
        obj[QStringLiteral("bpm")] = item.bpm;
        obj[QStringLiteral("duration_ms")] = static_cast<qint64>(item.duration_ms);
        obj[QStringLiteral("is_converted")] = item.is_converted;
        obj[QStringLiteral("played")] = item.played;
        obj[QStringLiteral("status")] = item.status;
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    QFile f(json_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();
    }
}

void TracklistDock::loadLibraryJson()
{
    tracks_.clear();
    const QString json_path = QDir(library_dir_).filePath(QStringLiteral("vj_tracklist.json"));
    QFile f(json_path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const auto &val : arr) {
                const QJsonObject obj = val.toObject();
                TrackItem item;
                item.title = obj[QStringLiteral("title")].toString();
                item.file_path = obj[QStringLiteral("file_path")].toString();
                item.original_path = obj[QStringLiteral("original_path")].toString();
                item.bpm = obj[QStringLiteral("bpm")].toDouble(128.0);
                item.duration_ms = obj[QStringLiteral("duration_ms")].toInteger();
                item.is_converted = obj[QStringLiteral("is_converted")].toBool();
                item.played = obj[QStringLiteral("played")].toBool();
                QString st = obj[QStringLiteral("status")].toString(QStringLiteral("Ready"));
                // Reset any stuck conversion status from previous crash/restart
                if (st.startsWith(QStringLiteral("Conv")) || st == QStringLiteral("Várakozik...")) {
                    st = QStringLiteral("Ready");
                }
                item.status = st;

                if (QFileInfo::exists(item.file_path) || QFileInfo::exists(item.original_path)) {
                    tracks_.push_back(item);
                }
            }
        }
    }
    updateTable();
}
