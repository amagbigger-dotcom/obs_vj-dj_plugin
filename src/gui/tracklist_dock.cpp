#include "gui/tracklist_dock.hpp"
#include "gui/settings_dialog.hpp"
#include "gui/playlist_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "audio/audio_analyzer.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <set>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QMenu>
#include <QInputDialog>
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
#include <QGuiApplication>
#include <QScreen>

static TracklistDock *g_tracklist_instance = nullptr;

void vdj_open_tracklist_window()
{
    if (!g_tracklist_instance) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_tracklist_instance = new TracklistDock(main_window);
    }
    g_tracklist_instance->applyWindowPosition();
    g_tracklist_instance->show();
    g_tracklist_instance->raise();
    g_tracklist_instance->activateWindow();
}

void vdj_free_tracklist_window()
{
    if (g_tracklist_instance) {
        g_tracklist_instance->close();
        delete g_tracklist_instance;
        g_tracklist_instance = nullptr;
    }
}

TracklistDock *vdj_get_tracklist_window()
{
    return g_tracklist_instance;
}

void vdj_import_and_load(const QString &file_path, int deck_channel)
{
    if (!g_tracklist_instance) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_tracklist_instance = new TracklistDock(main_window);
    }
    g_tracklist_instance->addFile(file_path, deck_channel);
}

void vdj_tracklist_move_selection(int delta)
{
    if (!g_tracklist_instance) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_tracklist_instance = new TracklistDock(main_window);
    }
    QMetaObject::invokeMethod(g_tracklist_instance, [delta]() {
        if (g_tracklist_instance) {
            if (!g_tracklist_instance->isVisible()) {
                g_tracklist_instance->show();
                g_tracklist_instance->raise();
            }
            g_tracklist_instance->moveSelection(delta);
        }
    }, Qt::QueuedConnection);
}

void vdj_tracklist_load_selected(int deck_channel)
{
    if (!g_tracklist_instance) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_tracklist_instance = new TracklistDock(main_window);
    }
    QMetaObject::invokeMethod(g_tracklist_instance, [deck_channel]() {
        if (g_tracklist_instance) {
            g_tracklist_instance->loadCurrentSelection(deck_channel);
        }
    }, Qt::QueuedConnection);
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
    setFixedSize(640, 560);
    setAttribute(Qt::WA_Moved, true);
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
        "QHeaderView::section:hover { background-color: #242c3c; color: #00d2ff; }"
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
                applyFilterAndPopulate();
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
                    DeckPlayer *target = (deck_target == 1) ? vj_deck_b() : vj_deck_a();
                    if (target && target->isPlaying()) {
                        blog(LOG_INFO, "[obs_vj_tracklist] Target deck %s is currently playing; newly converted track marked Ready without disrupting playback",
                             target->deckId());
                    } else {
                        loadTrackToDeck(active_conversion_idx_, deck_target);
                    }
                }
            } else {
                item.status = QStringLiteral("Hiba: %1").arg(err);
                blog(LOG_WARNING, "[obs_vj_tracklist] Conversion failed: %s", err.toUtf8().constData());
            }
            saveLibraryJson();
            applyFilterAndPopulate();
        }

        active_conversion_idx_ = -1;
        if (progress_bar_) {
            progress_bar_->setValue(100);
            progress_bar_->setVisible(false);
        }
        if (cancel_btn_) {
            cancel_btn_->setVisible(false);
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
    main_layout->setContentsMargins(12, 10, 12, 10);
    main_layout->setSpacing(8);

    // ==========================================
    // Top Bar 1: VJ Library Folder, Browse, Import
    // ==========================================
    auto *top_row1 = new QHBoxLayout();
    top_row1->setSpacing(6);

    auto *lbl_folder = new QLabel(QStringLiteral("📁 VJ:"), this);
    lbl_folder->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 10px;"));
    top_row1->addWidget(lbl_folder);

    dir_edit_ = new QLineEdit(library_dir_, this);
    dir_edit_->setReadOnly(true);
    dir_edit_->setStyleSheet(QStringLiteral("background: #181d26; color: #ffffff; border: 1px solid #2e3e54; border-radius: 3px; padding: 2px 6px; font-size: 10px;"));
    top_row1->addWidget(dir_edit_, 1);

    auto *browse_btn = new QPushButton(QStringLiteral("Tallózás..."), this);
    browse_btn->setStyleSheet(QStringLiteral("background: #222834; border: 1px solid #384255; color: #ffffff; padding: 2px 8px; font-size: 10px; border-radius: 3px;"));
    connect(browse_btn, &QPushButton::clicked, this, &TracklistDock::onBrowseFolder);
    top_row1->addWidget(browse_btn);

    auto *import_btn = new QPushButton(QStringLiteral("⚡ IMPORT"), this);
    import_btn->setStyleSheet(QStringLiteral("background: #0088cc; border: 1px solid #00aaff; color: #ffffff; padding: 2px 10px; font-weight: bold; font-size: 10px; border-radius: 3px;"));
    import_btn->setToolTip(QStringLiteral("Médiafájlok behúzása: a rendszer automatikusan észleli a formátumot, és csak a videókat alakítja 1080p-re az RTX 3060-nal"));
    connect(import_btn, &QPushButton::clicked, this, &TracklistDock::onImportButtonClicked);
    top_row1->addWidget(import_btn);

    main_layout->addLayout(top_row1);

    // ==========================================
    // Top Bar 2: Actions & Conversion Profile
    // ==========================================
    auto *top_row2 = new QHBoxLayout();
    top_row2->setSpacing(6);

    auto *del_btn = new QPushButton(QStringLiteral("🗑 TÖRLÉS"), this);
    del_btn->setStyleSheet(QStringLiteral("background: #3a1c1c; border: 1px solid #703838; color: #ff8888; padding: 2px 6px; font-size: 9px; font-weight: bold; border-radius: 3px;"));
    del_btn->setToolTip(QStringLiteral("Kijelölt szám törlése (listából vagy teljes médiatárból)"));
    connect(del_btn, &QPushButton::clicked, this, &TracklistDock::deleteSelectedTrack);
    top_row2->addWidget(del_btn);

    reconv_btn_ = new QPushButton(QStringLiteral("🔄 KONVERT"), this);
    reconv_btn_->setStyleSheet(QStringLiteral("background: #143522; border: 1px solid #236838; color: #55ff99; padding: 2px 6px; font-size: 9px; font-weight: bold; border-radius: 3px;"));
    reconv_btn_->setToolTip(QStringLiteral("Kijelölt szám kényszerített 1080p All-Intra újrakonvertálása"));
    connect(reconv_btn_, &QPushButton::clicked, this, &TracklistDock::onReconvertButtonClicked);
    top_row2->addWidget(reconv_btn_);

    reanalyze_btn_ = new QPushButton(QStringLiteral("⚡ ANALÍZIS"), this);
    reanalyze_btn_->setStyleSheet(QStringLiteral("background: #2b1f47; border: 1px solid #7c3aed; color: #e9d5ff; padding: 2px 6px; font-size: 9px; font-weight: bold; border-radius: 3px;"));
    reanalyze_btn_->setToolTip(QStringLiteral("Zenei újraanalizálás (BPM, Camelot Key, Auto-CUE, ReplayGain)"));
    connect(reanalyze_btn_, &QPushButton::clicked, this, &TracklistDock::onReanalyzeButtonClicked);
    top_row2->addWidget(reanalyze_btn_);

    auto *new_set_btn = new QPushButton(QStringLiteral("↺ ÚJ SZETT"), this);
    new_set_btn->setStyleSheet(QStringLiteral("background: #1c2635; border: 1px solid #334863; color: #8da0b8; padding: 2px 6px; font-size: 9px; font-weight: bold; border-radius: 3px;"));
    new_set_btn->setToolTip(QStringLiteral("Lejátszott (zöld pipa) jelölések törlése új szett indításához"));
    connect(new_set_btn, &QPushButton::clicked, this, [this]() {
        for (auto &it : tracks_) {
            it.played = false;
        }
        applyFilterAndPopulate();
    });
    top_row2->addWidget(new_set_btn);

    res_combo_ = new QComboBox(this);
    res_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1c2635; border: 1px solid #334863; color: #00d2ff; padding: 2px 6px; font-size: 9px; font-weight: bold; border-radius: 3px; }"
        "QComboBox QAbstractItemView { background: #151a24; color: #ffffff; selection-background-color: #0088cc; }"
    ));
    res_combo_->addItem(QStringLiteral("🎬 1080p FHD"), 1080);
    res_combo_->addItem(QStringLiteral("🎬 4K UHD"), 2160);
    res_combo_->addItem(QStringLiteral("🎬 2K QHD"), 1440);
    res_combo_->addItem(QStringLiteral("🎬 720p HD"), 720);
    res_combo_->addItem(QStringLiteral("🎬 Auto (Eredeti)"), 0);
    res_combo_->setToolTip(QStringLiteral("Konvertálási profil"));
    top_row2->addWidget(res_combo_);

    main_layout->addLayout(top_row2);

    // ==========================================
    // Bar 3: Playlist Quick Slots (4 Buttons) & Search
    // ==========================================
    auto *playlist_bar = new QHBoxLayout();
    playlist_bar->setSpacing(4);

    // Slot 1: MINDEN TRACK (Always fixed to all tracks)
    btn_slot_all_ = new QPushButton(QStringLiteral("🎵 MIND"), this);
    btn_slot_all_->setFixedHeight(24);
    btn_slot_all_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0088cc; border: 1px solid #00d2ff; color: #ffffff; font-weight: bold; font-size: 10px; border-radius: 3px; padding: 2px 8px; }"
        "QPushButton:hover { background: #0099e6; }"
    ));
    btn_slot_all_->setToolTip(QStringLiteral("Teljes médiatár (minden track és videó)"));
    connect(btn_slot_all_, &QPushButton::clicked, this, [this]() {
        onSlotMainClicked(0);
    });
    playlist_bar->addWidget(btn_slot_all_);

    // Slots 2, 3, 4: Split buttons (Main button + mini dropdown menu)
    for (int i = 0; i < 3; ++i) {
        auto *slot_layout = new QHBoxLayout();
        slot_layout->setSpacing(0);

        auto *main_btn = new QPushButton(QStringLiteral("[ Üres ]"), this);
        main_btn->setFixedHeight(24);
        main_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #222834; border: 1px solid #384255; border-right: none; border-top-left-radius: 3px; border-bottom-left-radius: 3px; color: #9aa8ba; font-weight: bold; font-size: 10px; padding: 2px 6px; }"
            "QPushButton:hover { background: #2c3545; color: #ffffff; }"
        ));
        connect(main_btn, &QPushButton::clicked, this, [this, i]() {
            onSlotMainClicked(i + 1);
        });
        slot_layout->addWidget(main_btn, 1);

        auto *menu_btn = new QPushButton(QStringLiteral("▾"), this);
        menu_btn->setFixedSize(16, 24);
        menu_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1c222d; border: 1px solid #384255; border-top-right-radius: 3px; border-bottom-right-radius: 3px; color: #7f8fa4; font-size: 10px; padding: 0px; }"
            "QPushButton:hover { background: #2f384a; color: #00d2ff; }"
        ));
        connect(menu_btn, &QPushButton::clicked, this, [this, i]() {
            onSlotMenuClicked(i);
        });
        slot_layout->addWidget(menu_btn);

        slot_widgets_[i].main_btn = main_btn;
        slot_widgets_[i].menu_btn = menu_btn;

        playlist_bar->addLayout(slot_layout);
    }

    // New Playlist button
    btn_new_playlist_ = new QPushButton(QStringLiteral("➕"), this);
    btn_new_playlist_->setFixedSize(24, 24);
    btn_new_playlist_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #143522; border: 1px solid #236838; color: #55ff99; font-weight: bold; font-size: 11px; border-radius: 3px; padding: 0px; }"
        "QPushButton:hover { background: #1b4b2f; border-color: #00ff88; }"
    ));
    btn_new_playlist_->setToolTip(QStringLiteral("Új lejátszási lista létrehozása"));
    connect(btn_new_playlist_, &QPushButton::clicked, this, &TracklistDock::onNewPlaylistClicked);
    playlist_bar->addWidget(btn_new_playlist_);

    // Quick Live Search field
    search_edit_ = new QLineEdit(this);
    search_edit_->setFixedHeight(24);
    search_edit_->setPlaceholderText(QStringLiteral("🔍 Keresés..."));
    search_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #181d26; color: #ffffff; border: 1px solid #2e3e54; border-radius: 3px; padding: 2px 6px; font-size: 10px; }"
        "QLineEdit:focus { border: 1px solid #00d2ff; }"
    ));
    connect(search_edit_, &QLineEdit::textChanged, this, &TracklistDock::onSearchTextChanged);
    playlist_bar->addWidget(search_edit_, 1);

    main_layout->addLayout(playlist_bar);

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

    cancel_btn_ = new QPushButton(QStringLiteral("⏹ MÉGSE"), this);
    cancel_btn_->setStyleSheet(QStringLiteral("background: #4a1c1c; border: 1px solid #7a3030; color: #ffaaaa; padding: 2px 8px; font-size: 10px; font-weight: bold; border-radius: 3px;"));
    cancel_btn_->setToolTip(QStringLiteral("Aktuális művelet (konvertálás vagy zenei analízis) azonnali megszakítása"));
    cancel_btn_->setVisible(false);
    connect(cancel_btn_, &QPushButton::clicked, this, [this]() {
        if (converter_ && converter_->isConverting()) {
            cancelActiveConversion();
        }
        if (active_analysis_idx_ >= 0 || !analysis_queue_.empty()) {
            cancelActiveAnalysis();
        }
    });
    status_bar->addWidget(cancel_btn_);

    main_layout->addLayout(status_bar);

    // ==========================================
    // Table: Status, Title, BPM, Time, Format, Load
    // ==========================================
    table_ = new QTableWidget(this);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("ST"),
        QStringLiteral("CÍM / FÁJLNÉV"),
        QStringLiteral("BPM"),
        QStringLiteral("KEY"),
        QStringLiteral("IDŐ"),
        QStringLiteral("FORM"),
        QStringLiteral("LOAD")
    });

    table_->setAlternatingRowColors(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(true);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);

    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->setColumnWidth(0, 30);

    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->setColumnWidth(2, 54);

    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->setColumnWidth(3, 38);

    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    table_->setColumnWidth(4, 46);

    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    table_->setColumnWidth(5, 52);

    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    table_->setColumnWidth(6, 68);

    table_->horizontalHeader()->setSectionsClickable(true);
    connect(table_->horizontalHeader(), &QHeaderView::sectionClicked, this, &TracklistDock::onHeaderSectionClicked);

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
    if (row < 0 || row >= static_cast<int>(filtered_track_indices_.size())) {
        return;
    }
    const int real_idx = filtered_track_indices_[row];
    if (real_idx < 0 || real_idx >= static_cast<int>(tracks_.size())) {
        return;
    }
    const auto &item = tracks_[real_idx];

    // Ha egyedi lejátszási listában állunk, csak abból a listából távolítjuk el (nem a teljes médiatárból!)
    if (playlist_mgr_.activeSlot() > 0) {
        const QString cur_pl_id = playlist_mgr_.activePlaylistId();
        playlist_mgr_.removeTrackFromPlaylist(cur_pl_id, item.file_path);
        saveLibraryJson();
        applyFilterAndPopulate();
        return;
    }

    // Ha a MINDEN TRACK (Slot 0) listában vagyunk: teljes törlés a médiatárból
    const auto res = QMessageBox::question(
        this,
        QStringLiteral("Track törlése a médiatárból"),
        QStringLiteral("Biztosan törölni szeretnéd a médiatárból a következőt:\n\n\"%1\"?").arg(item.title),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (res == QMessageBox::Yes) {
        // 1. Delete the .vdj.json sidecar cache file
        const QString orig_cache = item.original_path + QStringLiteral(".vdj.json");
        const QString conv_cache = item.file_path + QStringLiteral(".vdj.json");
        if (QFile::exists(orig_cache)) QFile::remove(orig_cache);
        if (QFile::exists(conv_cache)) QFile::remove(conv_cache);

        // 2. Delete the converted 1080p MP4 file from disk
        if (item.file_path.endsWith(QStringLiteral("_vj1080.mp4")) && QFile::exists(item.file_path)) {
            QFile::remove(item.file_path);
        }
        const QString lib_conv = QDir(library_dir_).filePath(QFileInfo(item.original_path).completeBaseName() + QStringLiteral("_vj1080.mp4"));
        if (QFile::exists(lib_conv)) {
            QFile::remove(lib_conv);
        }

        const QString removed_path = item.file_path;

        // 3. Remove from tracks_
        tracks_.erase(tracks_.begin() + real_idx);

        // 4. Remove from all playlists as well
        for (const auto &pl : playlist_mgr_.playlists()) {
            playlist_mgr_.removeTrackFromPlaylist(pl.id, removed_path);
        }

        saveLibraryJson();
        applyFilterAndPopulate();
    }
}

bool vdj_is_track_in_library(const QString &file_path)
{
    if (!g_tracklist_instance) return false;
    for (const auto &t : g_tracklist_instance->tracks()) {
        if (t.original_path == file_path || t.file_path == file_path)
            return true;
    }
    return false;
}

void TracklistDock::relinkTrackFile(int real_track_index)
{
    if (real_track_index < 0 || real_track_index >= static_cast<int>(tracks_.size())) return;
    auto &item = tracks_[real_track_index];

    const QString old_path = item.file_path;
    QString start_dir = QFileInfo(old_path).absolutePath();
    if (!QDir(start_dir).exists()) {
        start_dir = QFileInfo(item.original_path).absolutePath();
    }
    if (!QDir(start_dir).exists()) {
        start_dir = library_dir_;
    }

    const QString new_file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Hiányzó fájl újrakeresése: %1").arg(item.title),
        start_dir,
        QStringLiteral("Médiafájlok (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav *.flac *.aac *.ogg *.m4a);;Minden fájl (*.*)")
    );

    if (new_file.isEmpty() || !QFile::exists(new_file)) {
        return;
    }

    const QFileInfo fi(new_file);
    const QString ext = fi.suffix().toLower();
    const bool is_audio = isAudioOnlyFile(ext);

    if (is_audio) {
        // Tisztán audió fájl: nincs szükség videokonvertálásra
        item.original_path = new_file;
        item.file_path = new_file;
        item.is_converted = true;
        item.status = QStringLiteral("Ready");
        analyzeTrackAudio(real_track_index, true);
    } else {
        // Videófájl
        item.original_path = new_file;
        const QString out_file = QDir(library_dir_).filePath(
            fi.completeBaseName() + QStringLiteral("_vj1080.mp4")
        );

        if (QFile::exists(out_file) && QFileInfo(out_file).size() > 1024) {
            // Már létezik a konvertált 1080p fájl a VJ könyvtárban
            item.file_path = out_file;
            item.is_converted = true;
            item.status = QStringLiteral("Ready");
            analyzeTrackAudio(real_track_index);
        } else if (new_file.contains(QStringLiteral("_vj")) && new_file.endsWith(QStringLiteral(".mp4"))) {
            // Közvetlenül a konvertált videót tallózta be a felhasználó
            item.file_path = new_file;
            item.is_converted = true;
            item.status = QStringLiteral("Ready");
            analyzeTrackAudio(real_track_index);
        } else {
            // Nyers, nem konvertált videó (pl. 2K/4K vagy Long-GOP):
            // KÖTELEZŐ NVENC 1080p All-Intra formátumra konvertálni, nehogy kifagyassza a dekódert!
            item.file_path = new_file;
            item.is_converted = false;
            item.status = QStringLiteral("Várakozik...");
            conversion_queue_.push_back(real_track_index);
            processNextConversion();
        }
    }

    if (status_lbl_) {
        status_lbl_->setText(QStringLiteral("✅ Fájl elérési útja sikeresen frissítve: %1").arg(fi.fileName()));
        status_lbl_->setStyleSheet(QStringLiteral("color: #00ff88; font-size: 11px;"));
    }

    saveLibraryJson();
    applyFilterAndPopulate();
}

void TracklistDock::onCustomContextMenuRequested(const QPoint &pos)
{
    const int row = table_->rowAt(pos.y());
    if (row < 0 || row >= static_cast<int>(filtered_track_indices_.size())) {
        return;
    }
    const int real_idx = filtered_track_indices_[row];
    if (real_idx < 0 || real_idx >= static_cast<int>(tracks_.size())) {
        return;
    }
    auto &item = tracks_[real_idx];

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: #1a2230; color: #ffffff; border: 1px solid #2e3e54; font-size: 11px; }"
        "QMenu::item:selected { background-color: #0088cc; }"
    ));

    auto *act_a = menu.addAction(QStringLiteral("▶ Betöltés a DECK A-ba"));
    auto *act_b = menu.addAction(QStringLiteral("▶ Betöltés a DECK B-be"));
    menu.addSeparator();

    // ==========================================
    // Playlist Hozzáadás Almenü
    // ==========================================
    auto *playlist_submenu = menu.addMenu(QStringLiteral("📁 Hozzáadás listához..."));
    playlist_submenu->setStyleSheet(menu.styleSheet());

    const auto &playlists = playlist_mgr_.playlists();
    for (const auto &pl : playlists) {
        const bool already_in = playlist_mgr_.isTrackInPlaylist(pl.id, item.file_path, item.title, item.bpm);
        auto *pl_act = playlist_submenu->addAction(already_in ? QStringLiteral("✓ %1 (már benne van)").arg(pl.name) : pl.name);
        pl_act->setEnabled(!already_in);
        connect(pl_act, &QAction::triggered, this, [this, pl_id = pl.id, path = item.file_path]() {
            playlist_mgr_.addTrackToPlaylist(pl_id, path);
            saveLibraryJson();
            applyFilterAndPopulate();
        });
    }

    if (!playlists.empty()) playlist_submenu->addSeparator();

    auto *act_add_new_pl = playlist_submenu->addAction(QStringLiteral("➕ Új listához adás..."));
    connect(act_add_new_pl, &QAction::triggered, this, [this, path = item.file_path]() {
        PlaylistDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString new_id = playlist_mgr_.createPlaylist(dlg.playlistName(), dlg.playlistGenre(), dlg.filterKeyword());
            if (!new_id.isEmpty()) {
                playlist_mgr_.addTrackToPlaylist(new_id, path);
                updateSlotButtons();
                saveLibraryJson();
                applyFilterAndPopulate();
            }
        }
    });

    // Ha egyedi lejátszási listában vagyunk: eltávolítás menüpont ebből a listából
    if (playlist_mgr_.activeSlot() > 0) {
        const QString cur_pl_id = playlist_mgr_.activePlaylistId();
        const Playlist *cur_pl = playlist_mgr_.findPlaylist(cur_pl_id);
        const QString pl_name = cur_pl ? cur_pl->name : QStringLiteral("listából");
        auto *act_remove_from_pl = menu.addAction(QStringLiteral("❌ Eltávolítás a(z) \"%1\" listából").arg(pl_name));
        connect(act_remove_from_pl, &QAction::triggered, this, [this, cur_pl_id, path = item.file_path]() {
            playlist_mgr_.removeTrackFromPlaylist(cur_pl_id, path);
            saveLibraryJson();
            applyFilterAndPopulate();
        });
    }

    menu.addSeparator();

    // Hiányzó fájl újrakeresése (Relink)
    auto *act_relink = menu.addAction(QStringLiteral("📁 Fájl újrakeresése / elérési útvonal megadása..."));
    connect(act_relink, &QAction::triggered, this, [this, real_idx]() {
        relinkTrackFile(real_idx);
    });

    menu.addSeparator();
    auto *act_bpm_double = menu.addAction(QStringLiteral("⚡ BPM Duplázása (x2)"));
    auto *act_bpm_half   = menu.addAction(QStringLiteral("⚡ BPM Felezése (/2)"));
    auto *act_bpm_trip   = menu.addAction(QStringLiteral("⚡ BPM Másfélszerezése (x1.5)"));
    menu.addSeparator();
    auto *act_reconv = menu.addAction(QStringLiteral("🔄 Kényszerített újrakonvertálás (NVENC 1080p)"));
    auto *act_reanalyze = menu.addAction(QStringLiteral("⚡ Zenei újraanalizálás (BPM & Camelot Key, videó nélkül)"));
    auto *act_change_vid = menu.addAction(QStringLiteral("📁 Videófájl hozzárendelése / cseréje..."));
    QAction *act_cancel = nullptr;
    if (converter_ && converter_->isConverting()) {
        act_cancel = menu.addAction(QStringLiteral("⏹ Aktuális konvertálás megszakítása (Mégse)"));
    }
    menu.addSeparator();
    auto *act_del = menu.addAction(QStringLiteral("🗑 Törlés a médiatárból"));
    auto *act_reset_played = menu.addAction(QStringLiteral("↺ Lejátszott jelölések törlése (Új szett)"));

    QAction *selected = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (selected == act_a) {
        loadTrackToDeck(real_idx, 0);
    } else if (selected == act_b) {
        loadTrackToDeck(real_idx, 1);
    } else if (selected == act_bpm_double) {
        setTrackBpmDirect(real_idx, item.bpm * 2.0);
    } else if (selected == act_bpm_half) {
        setTrackBpmDirect(real_idx, item.bpm / 2.0);
    } else if (selected == act_bpm_trip) {
        setTrackBpmDirect(real_idx, item.bpm * 1.5);
    } else if (selected == act_reconv) {
        reconvertTrack(real_idx);
    } else if (selected == act_reanalyze) {
        analyzeTrackAudio(real_idx, true);
    } else if (selected == act_change_vid) {
        const QString chosen = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Videófájl kiválasztása a számhoz"),
            QFileInfo(item.original_path).absolutePath(),
            QStringLiteral("Video Files (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.ts);;All Files (*.*)")
        );
        if (!chosen.isEmpty()) {
            item.original_path = chosen;
            item.title = QFileInfo(chosen).completeBaseName();
            reconvertTrack(real_idx);
        }
    } else if (act_cancel && selected == act_cancel) {
        cancelActiveConversion();
    } else if (selected == act_del) {
        deleteSelectedTrack();
    } else if (selected == act_reset_played) {
        for (auto &it : tracks_) {
            it.played = false;
        }
        applyFilterAndPopulate();
    }
}

void TracklistDock::onReconvertButtonClicked()
{
    reconvertSelectedTrack();
}

void TracklistDock::reconvertSelectedTrack()
{
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(filtered_track_indices_.size())) {
        QMessageBox::information(this, QStringLiteral("Újrakonvertálás"),
            QStringLiteral("Válassz ki egy számot a listában az újrakonvertáláshoz!"));
        return;
    }
    reconvertTrack(filtered_track_indices_[row]);
}

void TracklistDock::reconvertTrack(int real_idx)
{
    if (real_idx < 0 || real_idx >= static_cast<int>(tracks_.size())) return;
    auto &it = tracks_[real_idx];

    const QFileInfo orig_fi(it.original_path);
    const QString ext = orig_fi.suffix().toLower();

    // Ha ez egy tisztán audiófájl, megnézzük, létezik-e azonos nevű videó a mappájában
    if (isAudioOnlyFile(ext)) {
        QString found_video;
        const QString dir = orig_fi.absolutePath();
        const QString base = orig_fi.completeBaseName();
        const QStringList vid_exts = {QStringLiteral(".mp4"), QStringLiteral(".mov"), QStringLiteral(".mkv"), QStringLiteral(".avi")};
        for (const auto &v_ext : vid_exts) {
            const QString candidate = QDir(dir).filePath(base + v_ext);
            if (QFile::exists(candidate)) {
                found_video = candidate;
                break;
            }
        }
        if (!found_video.isEmpty()) {
            it.original_path = found_video;
        } else {
            it.file_path = it.original_path;
            it.is_converted = true;
            it.status = QStringLiteral("Ready");
            analyzeTrackAudio(real_idx, true);
            applyFilterAndPopulate();
            saveLibraryJson();
            return;
        }
    }

    const QString out_file = QDir(library_dir_).filePath(
        QFileInfo(it.original_path).completeBaseName() + QStringLiteral("_vj1080.mp4")
    );

    it.status = QStringLiteral("Várakozik...");
    applyFilterAndPopulate();

    conversion_queue_.push_back(real_idx);
    processNextConversion();
}

void TracklistDock::onReanalyzeButtonClicked()
{
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(filtered_track_indices_.size())) {
        // Ha nincs semmi kijelölve: az összes dal újraanalizálása
        const auto res = QMessageBox::question(
            this,
            QStringLiteral("Teljes Zenei Újraanalizálás"),
            QStringLiteral("Biztosan újra szeretnéd analizálni az összes dalt a médiatárban?\n(BPM, Camelot Key, ReplayGain frissítése a háttérben)"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (res == QMessageBox::Yes) {
            analysis_queue_.clear();
            for (size_t i = 0; i < tracks_.size(); ++i) {
                analysis_queue_.push_back(static_cast<int>(i));
            }
            total_analysis_count_ = static_cast<int>(analysis_queue_.size());
            done_analysis_count_ = 0;
            processNextAnalysis();
        }
        return;
    }

    // Kijelölt dal kényszerített zenei újraanalizálása
    analyzeTrackAudio(filtered_track_indices_[row], true);
}

void TracklistDock::cancelActiveConversion()
{
    conversion_queue_.clear();
    if (converter_ && converter_->isConverting()) {
        converter_->cancel();
    }
    if (active_conversion_idx_ >= 0 && active_conversion_idx_ < static_cast<int>(tracks_.size())) {
        tracks_[active_conversion_idx_].status = QStringLiteral("Megszakítva");
        applyFilterAndPopulate();
    }
    active_conversion_idx_ = -1;
    if (progress_bar_) progress_bar_->setVisible(false);
    if (cancel_btn_) cancel_btn_->setVisible(false);
    if (status_lbl_) {
        status_lbl_->setText(QStringLiteral("Konvertálás megszakítva."));
    }
}

void TracklistDock::applyWindowPosition()
{
    setFixedSize(640, 560);
    const auto &cfg = SettingsDialog::config();
    int x_pos = cfg.win_tracklist_x;
    int y_pos = cfg.win_tracklist_y;

    if (x_pos < 0 || y_pos < 0 || (x_pos == 630 && y_pos == 237)) {
        x_pos = 1255;
        y_pos = 38;
    }
    move(x_pos, y_pos);
}

void TracklistDock::showEvent(QShowEvent *event)
{
    applyWindowPosition();
    QDialog::showEvent(event);
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
    const auto urls = event->mimeData()->urls();
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            addFile(url.toLocalFile(), -1);
        }
    }
    event->acceptProposedAction();
}

void TracklistDock::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedTrack();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void TracklistDock::moveSelection(int delta)
{
    if (!table_ || filtered_track_indices_.empty())
        return;
    const int count = static_cast<int>(filtered_track_indices_.size());
    int cur_row = table_->currentRow();
    if (cur_row < 0) {
        cur_row = (delta > 0) ? 0 : count - 1;
    } else {
        cur_row += delta;
        if (cur_row < 0) cur_row = 0;
        if (cur_row >= count) {
            cur_row = count - 1;
        }
    }
    table_->setCurrentCell(cur_row, 1);
    table_->selectRow(cur_row);
}

void TracklistDock::loadCurrentSelection(int deck_channel)
{
    if (!table_ || filtered_track_indices_.empty())
        return;
    int cur_row = table_->currentRow();
    if (cur_row < 0) {
        cur_row = 0;
        table_->setCurrentCell(0, 1);
        table_->selectRow(0);
    }
    if (cur_row >= 0 && cur_row < static_cast<int>(filtered_track_indices_.size())) {
        loadTrackToDeck(filtered_track_indices_[cur_row], deck_channel);
    }
}

void TracklistDock::addFile(const QString &path, int target_deck)
{
    const QFileInfo fi(path);
    if (!fi.exists()) return;

    const QString clean_path = QDir::cleanPath(path);
    for (size_t i = 0; i < tracks_.size(); i++) {
        if (QDir::cleanPath(tracks_[i].original_path).compare(clean_path, Qt::CaseInsensitive) == 0 ||
            QDir::cleanPath(tracks_[i].file_path).compare(clean_path, Qt::CaseInsensitive) == 0) {
            if (target_deck >= 0) {
                loadTrackToDeck(static_cast<int>(i), target_deck);
            }
            return;
        }
    }

    TrackItem item;
    item.title = fi.completeBaseName();
    item.original_path = clean_path;
    item.target_deck_after_conv = target_deck;

    const QString ext = fi.suffix().toLower();
    const bool is_audio_only = isAudioOnlyFile(ext);

    if (is_audio_only) {
        item.file_path = clean_path;
        item.is_converted = true;
        item.status = QStringLiteral("Ready");
    } else {
        const QString out_file = QDir(library_dir_).filePath(
            fi.completeBaseName() + QStringLiteral("_vj1080.mp4")
        );

        if (QFile::exists(out_file)) {
            item.file_path = out_file;
            item.is_converted = true;
            item.status = QStringLiteral("Ready");
        } else {
            item.file_path = clean_path;
            item.is_converted = false;
            item.status = QStringLiteral("Várakozik...");
        }
    }

    const int new_idx = static_cast<int>(tracks_.size());
    tracks_.push_back(item);

    // Ha éppen egy felhasználói lejátszási listában vagyunk, automatikusan hozzáadjuk ahhoz a listához is!
    if (playlist_mgr_.activeSlot() > 0) {
        playlist_mgr_.addTrackToPlaylist(playlist_mgr_.activePlaylistId(), item.file_path);
    }

    analyzeTrackAudio(new_idx);

    if (!item.is_converted) {
        conversion_queue_.push_back(new_idx);
        processNextConversion();
    } else {
        if (target_deck >= 0) {
            loadTrackToDeck(new_idx, target_deck);
        }
    }

    applyFilterAndPopulate();
    saveLibraryJson();
}

void TracklistDock::processNextConversion()
{
    if (active_conversion_idx_ >= 0) return;
    if (conversion_queue_.empty()) {
        if (progress_bar_) progress_bar_->setVisible(false);
        if (cancel_btn_) cancel_btn_->setVisible(false);
        if (status_lbl_) {
            status_lbl_->setText(QStringLiteral("Minden videó készen áll 1080p All-Intra formátumban."));
            status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-size: 11px;"));
        }
        return;
    }

    active_conversion_idx_ = conversion_queue_.front();
    conversion_queue_.erase(conversion_queue_.begin());

    if (active_conversion_idx_ >= static_cast<int>(tracks_.size())) {
        active_conversion_idx_ = -1;
        processNextConversion();
        return;
    }

    auto &item = tracks_[active_conversion_idx_];
    const QString orig = item.original_path;
    const QString out_path = QDir(library_dir_).filePath(
        QFileInfo(orig).completeBaseName() + QStringLiteral("_vj1080.mp4")
    );

    if (QFile::exists(out_path)) {
        item.file_path = out_path;
        item.is_converted = true;
        item.status = QStringLiteral("Ready");
        analyzeTrackAudio(active_conversion_idx_);

        if (item.target_deck_after_conv >= 0) {
            loadTrackToDeck(active_conversion_idx_, item.target_deck_after_conv);
        }
        applyFilterAndPopulate();
        saveLibraryJson();
        active_conversion_idx_ = -1;
        processNextConversion();
        return;
    }

    item.status = QStringLiteral("Konvertálás...");
    applyFilterAndPopulate();

    if (progress_bar_) {
        progress_bar_->setValue(0);
        progress_bar_->setVisible(true);
    }
    if (cancel_btn_) {
        cancel_btn_->setVisible(true);
    }
    const auto target_res = selectedTargetResolution();
    QString profile_str = QStringLiteral("1080p");
    if (target_res == VideoConverter::TargetResolution::Res4K) profile_str = QStringLiteral("4K");
    else if (target_res == VideoConverter::TargetResolution::Res1440p) profile_str = QStringLiteral("2K");
    else if (target_res == VideoConverter::TargetResolution::Res720p) profile_str = QStringLiteral("720p");
    else if (target_res == VideoConverter::TargetResolution::Auto) profile_str = QStringLiteral("Auto");

    if (status_lbl_) {
        status_lbl_->setText(QStringLiteral("RTX 3060 NVENC %1: %2").arg(profile_str, QFileInfo(orig).fileName()));
        status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-size: 11px; font-weight: bold;"));
    }

    const bool started = converter_->convertVideo(orig, library_dir_, target_res);
    if (!started) {
        item.status = QStringLiteral("Nem indítható");
        active_conversion_idx_ = -1;
        applyFilterAndPopulate();
        processNextConversion();
    }
}

VideoConverter::TargetResolution TracklistDock::selectedTargetResolution() const
{
    if (!res_combo_) return VideoConverter::TargetResolution::Res1080p;
    const int val = res_combo_->currentData().toInt();
    switch (val) {
        case 2160: return VideoConverter::TargetResolution::Res4K;
        case 1440: return VideoConverter::TargetResolution::Res1440p;
        case 720:  return VideoConverter::TargetResolution::Res720p;
        case 0:    return VideoConverter::TargetResolution::Auto;
        default:   return VideoConverter::TargetResolution::Res1080p;
    }
}

void TracklistDock::analyzeTrackAudio(int track_index, bool force)
{
    if (track_index < 0 || track_index >= static_cast<int>(tracks_.size())) return;
    auto &item = tracks_[track_index];

    if (!force) {
        TrackAnalysisResult cached;
        if (AudioAnalyzer::loadCache(item.file_path, cached) || AudioAnalyzer::loadCache(item.original_path, cached)) {
            if (cached.detected_bpm > 20.0) {
                item.bpm = cached.detected_bpm;
            }
            if (!cached.detected_key.isEmpty() && cached.detected_key != QStringLiteral("--")) {
                item.key = cached.detected_key;
            }
            if (!cached.camelot_key.isEmpty() && cached.camelot_key != QStringLiteral("--")) {
                item.camelot = cached.camelot_key;
            }
            if (cached.duration_ms > 0) {
                item.duration_ms = cached.duration_ms;
            }
            applyFilterAndPopulate();
            return;
        }
    }

    analysis_queue_.push_back(track_index);
    total_analysis_count_ = static_cast<int>(analysis_queue_.size());
    done_analysis_count_ = 0;
    processNextAnalysis();
}

void TracklistDock::processNextAnalysis()
{
    if (active_analysis_idx_ >= 0) return;
    if (analysis_queue_.empty()) {
        if (progress_bar_ && (!converter_ || !converter_->isConverting())) {
            progress_bar_->setVisible(false);
        }
        if (cancel_btn_ && (!converter_ || !converter_->isConverting())) {
            cancel_btn_->setVisible(false);
        }
        return;
    }

    active_analysis_idx_ = analysis_queue_.front();
    analysis_queue_.erase(analysis_queue_.begin());

    if (active_analysis_idx_ >= static_cast<int>(tracks_.size())) {
        active_analysis_idx_ = -1;
        processNextAnalysis();
        return;
    }

    const auto &item = tracks_[active_analysis_idx_];
    const QString target_file = QFileInfo::exists(item.file_path) ? item.file_path : item.original_path;

    if (!QFileInfo::exists(target_file)) {
        active_analysis_idx_ = -1;
        processNextAnalysis();
        return;
    }

    if (status_lbl_ && (!converter_ || !converter_->isConverting())) {
        status_lbl_->setText(QStringLiteral("⚡ Zenei analízis (%1/%2): %3")
            .arg(done_analysis_count_ + 1)
            .arg(total_analysis_count_)
            .arg(QFileInfo(target_file).fileName()));
        status_lbl_->setStyleSheet(QStringLiteral("color: #c084fc; font-size: 11px; font-weight: bold;"));
    }

    const int cur_idx = active_analysis_idx_;
    AudioAnalyzer::analyzeFileAsync(target_file, [this, cur_idx](const TrackAnalysisResult &res) {
        QMetaObject::invokeMethod(this, [this, cur_idx, res]() {
            if (cur_idx >= 0 && cur_idx < static_cast<int>(tracks_.size())) {
                auto &track = tracks_[cur_idx];
                if (res.detected_bpm > 20.0) {
                    track.bpm = res.detected_bpm;
                }
                if (!res.detected_key.isEmpty() && res.detected_key != QStringLiteral("--")) {
                    track.key = res.detected_key;
                }
                if (!res.camelot_key.isEmpty() && res.camelot_key != QStringLiteral("--")) {
                    track.camelot = res.camelot_key;
                }
                if (res.duration_ms > 0) {
                    track.duration_ms = res.duration_ms;
                }
                applyFilterAndPopulate();
                saveLibraryJson();
            }

            done_analysis_count_++;
            active_analysis_idx_ = -1;
            processNextAnalysis();
        }, Qt::QueuedConnection);
    });
}

void TracklistDock::cancelActiveAnalysis()
{
    analysis_queue_.clear();
    active_analysis_idx_ = -1;
    if (progress_bar_ && (!converter_ || !converter_->isConverting())) progress_bar_->setVisible(false);
    if (cancel_btn_ && (!converter_ || !converter_->isConverting())) cancel_btn_->setVisible(false);
    if (status_lbl_) {
        status_lbl_->setText(QStringLiteral("Zenei analízis megszakítva."));
    }
}

void TracklistDock::updateTable()
{
    applyFilterAndPopulate();
}

void TracklistDock::applyFilterAndPopulate()
{
    filtered_track_indices_.clear();
    const int active_slot = playlist_mgr_.activeSlot();
    const QString active_pl_id = playlist_mgr_.activePlaylistId();
    const QString filter_text = search_edit_ ? search_edit_->text().trimmed().toLower() : QString();

    for (int i = 0; i < static_cast<int>(tracks_.size()); ++i) {
        const auto &item = tracks_[i];

        // 1. Check playlist membership
        if (active_slot > 0) {
            if (!playlist_mgr_.isTrackInPlaylist(active_pl_id, item.file_path, item.title, item.bpm)) {
                continue;
            }
        }

        // 2. Check live search text
        if (!filter_text.isEmpty()) {
            const QString title = item.title.toLower();
            const QString path = item.file_path.toLower();
            const QString bpm_str = QString::number(item.bpm, 'f', 1);
            const QString key = item.camelot.toLower();
            if (!title.contains(filter_text) && !path.contains(filter_text) &&
                !bpm_str.contains(filter_text) && !key.contains(filter_text)) {
                continue;
            }
        }

        filtered_track_indices_.push_back(i);
    }

    // 3. Opcionális rendezés az oszlopfejlécre kattintás szerint (3-állapotú ciklus)
    if (sort_column_ >= 0) {
        std::stable_sort(filtered_track_indices_.begin(), filtered_track_indices_.end(), [this](int a, int b) {
            const auto &itemA = tracks_[a];
            const auto &itemB = tracks_[b];
            int cmp = 0;

            switch (sort_column_) {
                case 0: { // ST (Státusz / azonos fajták egymás mellé)
                    auto getStatusScore = [](const TrackItem &it) -> int {
                        if (vj_deck_a() && vj_deck_a()->filePath() == it.file_path && vj_deck_a()->isPlaying()) return 0;
                        if (vj_deck_b() && vj_deck_b()->filePath() == it.file_path && vj_deck_b()->isPlaying()) return 1;
                        if (!QFileInfo::exists(it.file_path)) return 2;
                        if (it.played) return 3;
                        if (it.status == QStringLiteral("Ready")) return 4;
                        if (it.status == QStringLiteral("Várakozik...")) return 5;
                        return 6;
                    };
                    const int sA = getStatusScore(itemA);
                    const int sB = getStatusScore(itemB);
                    if (sA != sB) cmp = (sA < sB) ? -1 : 1;
                    break;
                }
                case 1: { // CÍM / NÉV (ABC szerint)
                    cmp = QString::localeAwareCompare(itemA.title, itemB.title);
                    break;
                }
                case 2: { // BPM (Számszerűen növekvő/csökkenő)
                    if (std::abs(itemA.bpm - itemB.bpm) > 0.001) {
                        cmp = (itemA.bpm < itemB.bpm) ? -1 : 1;
                    }
                    break;
                }
                case 3: { // KEY (Camelot kód / Hangnem)
                    auto parseCamelot = [](const QString &c) -> int {
                        if (c.isEmpty() || c == QStringLiteral("--")) return 999;
                        int num = 0;
                        int idx = 0;
                        while (idx < c.length() && c[idx].isDigit()) {
                            num = num * 10 + c[idx].digitValue();
                            idx++;
                        }
                        const int letterVal = (idx < c.length() && (c[idx] == 'B' || c[idx] == 'b')) ? 1 : 0;
                        return num * 10 + letterVal;
                    };
                    const int cA = parseCamelot(itemA.camelot);
                    const int cB = parseCamelot(itemB.camelot);
                    if (cA != cB) {
                        cmp = (cA < cB) ? -1 : 1;
                    } else {
                        cmp = QString::compare(itemA.camelot, itemB.camelot, Qt::CaseInsensitive);
                    }
                    break;
                }
                case 4: { // IDŐ (Időtartam)
                    if (itemA.duration_ms != itemB.duration_ms) {
                        cmp = (itemA.duration_ms < itemB.duration_ms) ? -1 : 1;
                    }
                    break;
                }
                case 5: { // FORM (Formátum: azonos fajták egymás mellé)
                    auto getFormScore = [](const TrackItem &it) -> int {
                        const QString ext = QFileInfo(it.file_path).suffix().toLower();
                        if (isAudioOnlyFile(ext)) return 0; // Audio
                        if (it.is_converted) return 1;     // 1080p VJ
                        return 2;                          // Konvertálatlan / egyéb
                    };
                    const int fA = getFormScore(itemA);
                    const int fB = getFormScore(itemB);
                    if (fA != fB) {
                        cmp = (fA < fB) ? -1 : 1;
                    } else {
                        cmp = QString::localeAwareCompare(itemA.title, itemB.title);
                    }
                    break;
                }
                default:
                    break;
            }

            if (cmp == 0) {
                return a < b; // Stabil fallback az eredeti beillesztési indexre
            }

            return (sort_order_ == Qt::AscendingOrder) ? (cmp < 0) : (cmp > 0);
        });
    }

    if (table_ && table_->horizontalHeader()) {
        if (sort_column_ >= 0) {
            table_->horizontalHeader()->setSortIndicatorShown(true);
            table_->horizontalHeader()->setSortIndicator(sort_column_, sort_order_);
        } else {
            table_->horizontalHeader()->setSortIndicatorShown(false);
        }
    }

    table_->setRowCount(static_cast<int>(filtered_track_indices_.size()));
    for (int r = 0; r < static_cast<int>(filtered_track_indices_.size()); ++r) {
        updateRow(r);
    }

    if (!filtered_track_indices_.empty() && table_->currentRow() < 0) {
        table_->setCurrentCell(0, 1);
        table_->selectRow(0);
    }
}

void TracklistDock::updateSlotButtons()
{
    const int active_slot = playlist_mgr_.activeSlot();

    const QString active_style = QStringLiteral(
        "QPushButton { background: #0088cc; border: 1px solid #00d2ff; color: #ffffff; font-weight: bold; font-size: 10px; padding: 2px 6px; border-radius: 3px; }"
        "QPushButton:hover { background: #0099e6; }"
    );
    const QString inactive_split_style = QStringLiteral(
        "QPushButton { background: #222834; border: 1px solid #384255; border-right: none; border-top-left-radius: 3px; border-bottom-left-radius: 3px; color: #9aa8ba; font-weight: bold; font-size: 10px; padding: 2px 6px; }"
        "QPushButton:hover { background: #2c3545; color: #ffffff; }"
    );
    const QString active_split_style = QStringLiteral(
        "QPushButton { background: #0088cc; border: 1px solid #00d2ff; border-right: none; border-top-left-radius: 3px; border-bottom-left-radius: 3px; color: #ffffff; font-weight: bold; font-size: 10px; padding: 2px 6px; }"
        "QPushButton:hover { background: #0099e6; }"
    );
    const QString inactive_all_style = QStringLiteral(
        "QPushButton { background: #222834; border: 1px solid #384255; color: #9aa8ba; font-weight: bold; font-size: 10px; border-radius: 3px; padding: 2px 8px; }"
        "QPushButton:hover { background: #2c3545; color: #ffffff; }"
    );

    if (btn_slot_all_) {
        btn_slot_all_->setStyleSheet(active_slot == 0 ? active_style : inactive_all_style);
    }

    for (int i = 0; i < 3; ++i) {
        auto *mb = slot_widgets_[i].main_btn;
        if (!mb) continue;

        const QString pl_id = playlist_mgr_.getQuickSlot(i);
        const Playlist *pl = playlist_mgr_.findPlaylist(pl_id);

        if (pl) {
            mb->setText(pl->name);
            mb->setToolTip(pl->genre.isEmpty() ? pl->name : QStringLiteral("%1 (%2)").arg(pl->name, pl->genre));
        } else {
            mb->setText(QStringLiteral("[ Üres ]"));
            mb->setToolTip(QStringLiteral("Nincs lista hozzárendelve. Kattints a ▾ nyílra lista választásához."));
        }

        const bool is_this_active = (active_slot == (i + 1));
        mb->setStyleSheet(is_this_active ? active_split_style : inactive_split_style);
    }
}

void TracklistDock::onSlotMainClicked(int slot_idx)
{
    playlist_mgr_.setActiveSlot(slot_idx);
    updateSlotButtons();
    applyFilterAndPopulate();
}

void TracklistDock::onSlotMenuClicked(int slot_offset)
{
    if (slot_offset < 0 || slot_offset >= 3) return;
    auto *menu_btn = slot_widgets_[slot_offset].menu_btn;
    if (!menu_btn) return;

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background-color: #1a2230; color: #ffffff; border: 1px solid #2e3e54; font-size: 11px; }"
        "QMenu::item:selected { background-color: #0088cc; }"
    ));

    const QString current_id = playlist_mgr_.getQuickSlot(slot_offset);
    const auto &all_pl = playlist_mgr_.playlists();

    if (!all_pl.empty()) {
        auto *header_act = menu.addAction(QStringLiteral("--- Válassz Lejátszási Listát ---"));
        header_act->setEnabled(false);

        for (const auto &pl : all_pl) {
            const bool is_cur = (pl.id == current_id);
            const QString label = is_cur ? QStringLiteral("✓ %1").arg(pl.name) : pl.name;
            auto *act = menu.addAction(label);
            connect(act, &QAction::triggered, this, [this, slot_offset, pl_id = pl.id]() {
                playlist_mgr_.setQuickSlot(slot_offset, pl_id);
                playlist_mgr_.setActiveSlot(slot_offset + 1);
                updateSlotButtons();
                applyFilterAndPopulate();
                saveLibraryJson();
            });
        }
        menu.addSeparator();
    }

    auto *act_new = menu.addAction(QStringLiteral("➕ Új lista létrehozása..."));
    connect(act_new, &QAction::triggered, this, [this, slot_offset]() {
        PlaylistDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString new_id = playlist_mgr_.createPlaylist(dlg.playlistName(), dlg.playlistGenre(), dlg.filterKeyword());
            if (!new_id.isEmpty()) {
                playlist_mgr_.setQuickSlot(slot_offset, new_id);
                playlist_mgr_.setActiveSlot(slot_offset + 1);
                updateSlotButtons();
                applyFilterAndPopulate();
                saveLibraryJson();
            }
        }
    });

    if (!current_id.isEmpty()) {
        auto *act_edit = menu.addAction(QStringLiteral("✏️ Lista szerkesztése (Név / Műfaj)..."));
        connect(act_edit, &QAction::triggered, this, [this, current_id]() {
            onEditPlaylist(current_id);
        });

        auto *act_del = menu.addAction(QStringLiteral("🗑 Lista törlése"));
        connect(act_del, &QAction::triggered, this, [this, current_id]() {
            onDeletePlaylist(current_id);
        });

        auto *act_clear = menu.addAction(QStringLiteral("❌ Slot kiürítése"));
        connect(act_clear, &QAction::triggered, this, [this, slot_offset]() {
            playlist_mgr_.setQuickSlot(slot_offset, QString());
            if (playlist_mgr_.activeSlot() == slot_offset + 1) {
                playlist_mgr_.setActiveSlot(0); // fallback to All
            }
            updateSlotButtons();
            applyFilterAndPopulate();
            saveLibraryJson();
        });
    }

    menu.exec(menu_btn->mapToGlobal(QPoint(0, menu_btn->height())));
}

void TracklistDock::onNewPlaylistClicked()
{
    PlaylistDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString new_id = playlist_mgr_.createPlaylist(dlg.playlistName(), dlg.playlistGenre(), dlg.filterKeyword());
        if (!new_id.isEmpty()) {
            for (int i = 0; i < 3; ++i) {
                if (playlist_mgr_.getQuickSlot(i) == new_id) {
                    playlist_mgr_.setActiveSlot(i + 1);
                    break;
                }
            }
            updateSlotButtons();
            applyFilterAndPopulate();
            saveLibraryJson();
        }
    }
}

void TracklistDock::onEditPlaylist(const QString &playlist_id)
{
    Playlist *pl = playlist_mgr_.findPlaylist(playlist_id);
    if (!pl) return;

    PlaylistDialog dlg(this, pl->name, pl->genre, pl->filter_keyword);
    if (dlg.exec() == QDialog::Accepted) {
        playlist_mgr_.updatePlaylist(playlist_id, dlg.playlistName(), dlg.playlistGenre(), dlg.filterKeyword());
        updateSlotButtons();
        applyFilterAndPopulate();
        saveLibraryJson();
    }
}

void TracklistDock::onDeletePlaylist(const QString &playlist_id)
{
    Playlist *pl = playlist_mgr_.findPlaylist(playlist_id);
    if (!pl) return;

    const auto res = QMessageBox::question(
        this,
        QStringLiteral("Lista Törlése"),
        QStringLiteral("Biztosan törölni szeretnéd a(z) \"%1\" lejátszási listát?\n(A zenék megmaradnak a teljes médiatárban)").arg(pl->name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (res == QMessageBox::Yes) {
        playlist_mgr_.deletePlaylist(playlist_id);
        updateSlotButtons();
        applyFilterAndPopulate();
        saveLibraryJson();
    }
}

void TracklistDock::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    applyFilterAndPopulate();
}

void TracklistDock::onHeaderSectionClicked(int logicalIndex)
{
    if (logicalIndex == 6) return; // LOAD oszlop nem rendezhető

    if (sort_column_ == logicalIndex) {
        if (sort_order_ == Qt::AscendingOrder) {
            sort_order_ = Qt::DescendingOrder;
        } else {
            // Harmadik kattintás: visszaállítás az importálás/beillesztés sorrendjére
            sort_column_ = -1;
            sort_order_ = Qt::AscendingOrder;
        }
    } else {
        sort_column_ = logicalIndex;
        sort_order_ = Qt::AscendingOrder;
    }

    applyFilterAndPopulate();
}

void TracklistDock::updateRow(int row)
{
    if (row < 0 || row >= static_cast<int>(filtered_track_indices_.size())) return;
    const int real_idx = filtered_track_indices_[row];
    if (real_idx < 0 || real_idx >= static_cast<int>(tracks_.size())) return;
    const auto &item = tracks_[real_idx];

    // Col 0: Status & Missing file check
    auto *st_item = table_->item(row, 0);
    if (!st_item) {
        st_item = new QTableWidgetItem();
        st_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 0, st_item);
    }

    const QString fn = item.file_path;
    const bool is_deck_a = (vj_deck_a() && vj_deck_a()->filePath() == fn && vj_deck_a()->isPlaying());
    const bool is_deck_b = (vj_deck_b() && vj_deck_b()->filePath() == fn && vj_deck_b()->isPlaying());
    const bool file_missing = !QFileInfo::exists(fn);

    if (is_deck_a) {
        st_item->setText(QStringLiteral("▶ A"));
        st_item->setForeground(QColor(0, 210, 255));
        st_item->setToolTip(QStringLiteral("Jelenleg a DECK A-ban játszik"));
    } else if (is_deck_b) {
        st_item->setText(QStringLiteral("▶ B"));
        st_item->setForeground(QColor(255, 140, 0));
        st_item->setToolTip(QStringLiteral("Jelenleg a DECK B-ben játszik"));
    } else if (item.played) {
        st_item->setText(QStringLiteral("✓"));
        st_item->setForeground(QColor(0, 255, 120));
        st_item->setToolTip(QStringLiteral("Lejátszva a jelenlegi szettben"));
    } else if (file_missing) {
        st_item->setText(QStringLiteral("⚠️"));
        st_item->setForeground(QColor(255, 180, 0));
        st_item->setToolTip(QStringLiteral("⚠️ A fájl nem található a lemezen!\nDupla kattintás vagy Jobb klikk: Elérési útvonal megadása."));
    } else {
        st_item->setText(item.status == QStringLiteral("Ready") ? QString() : QStringLiteral("⏳"));
        st_item->setForeground(QColor(160, 175, 195));
        st_item->setToolTip(item.status);
    }

    // Col 1: Title
    auto *title_item = table_->item(row, 1);
    if (!title_item) {
        title_item = new QTableWidgetItem();
        table_->setItem(row, 1, title_item);
    }
    title_item->setText(item.title);
    if (file_missing) {
        title_item->setForeground(QColor(255, 140, 140));
        title_item->setToolTip(QStringLiteral("⚠️ Hiányzó fájl: %1").arg(item.file_path));
    } else if (item.played && !is_deck_a && !is_deck_b) {
        title_item->setForeground(QColor(130, 145, 165));
        title_item->setToolTip(item.file_path);
    } else {
        title_item->setForeground(QColor(230, 236, 245));
        title_item->setToolTip(item.file_path);
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

    // Col 3: KEY (Camelot kód & Hangnem)
    auto *key_item = table_->item(row, 3);
    if (!key_item) {
        key_item = new QTableWidgetItem();
        key_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 3, key_item);
    }
    const QString display_key = (item.camelot.isEmpty() || item.camelot == QStringLiteral("--"))
        ? QStringLiteral("--")
        : item.camelot;
    key_item->setText(display_key);
    key_item->setForeground(QColor(255, 204, 0));
    if (!item.key.isEmpty() && item.key != QStringLiteral("--")) {
        key_item->setToolTip(QStringLiteral("%1 (%2)").arg(item.camelot, item.key));
    } else {
        key_item->setToolTip(QString());
    }

    // Col 4: Duration
    auto *dur_item = table_->item(row, 4);
    if (!dur_item) {
        dur_item = new QTableWidgetItem();
        dur_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 4, dur_item);
    }
    dur_item->setText(formatDuration(item.duration_ms));
    dur_item->setForeground(QColor(255, 170, 0));

    // Col 5: Format
    auto *fmt_item = table_->item(row, 5);
    if (!fmt_item) {
        fmt_item = new QTableWidgetItem();
        fmt_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 5, fmt_item);
    }
    if (file_missing) {
        fmt_item->setText(QStringLiteral("Hiányzik"));
        fmt_item->setForeground(QColor(255, 100, 100));
    } else if (item.is_converted) {
        fmt_item->setText(isAudioOnlyFile(QFileInfo(item.file_path).suffix()) ? QStringLiteral("Audio") : QStringLiteral("1080p VJ"));
        fmt_item->setForeground(QColor(0, 255, 136));
    } else {
        fmt_item->setText(item.status);
        fmt_item->setForeground(QColor(160, 175, 195));
    }

    // Col 6: Load Actions Widget (Kompakt mini gombok: [ A ] [ B ])
    auto *action_widget = new QWidget(table_);
    auto *act_layout = new QHBoxLayout(action_widget);
    act_layout->setContentsMargins(2, 1, 2, 1);
    act_layout->setSpacing(3);

    auto *load_a = new QPushButton(QStringLiteral("A"), action_widget);
    load_a->setFixedSize(28, 20);
    load_a->setToolTip(QStringLiteral("Betöltés a Deck A-ba"));
    load_a->setStyleSheet(QStringLiteral(
        "QPushButton { background: #006699; border: 1px solid #00aaff; color: #ffffff; font-weight: bold; font-size: 10px; border-radius: 3px; padding: 0px; }"
        "QPushButton:hover { background: #0088cc; border-color: #38e0ff; }"
    ));
    connect(load_a, &QPushButton::clicked, this, [this, real_idx]() {
        loadTrackToDeck(real_idx, 0);
    });
    act_layout->addWidget(load_a);

    auto *load_b = new QPushButton(QStringLiteral("B"), action_widget);
    load_b->setFixedSize(28, 20);
    load_b->setToolTip(QStringLiteral("Betöltés a Deck B-be"));
    load_b->setStyleSheet(QStringLiteral(
        "QPushButton { background: #a04400; border: 1px solid #ff7700; color: #ffffff; font-weight: bold; font-size: 10px; border-radius: 3px; padding: 0px; }"
        "QPushButton:hover { background: #c05500; border-color: #ffaa33; }"
    ));
    connect(load_b, &QPushButton::clicked, this, [this, real_idx]() {
        loadTrackToDeck(real_idx, 1);
    });
    act_layout->addWidget(load_b);

    table_->setCellWidget(row, 6, action_widget);
}

void TracklistDock::loadTrackToDeck(int track_index, int deck_channel)
{
    if (track_index < 0 || track_index >= static_cast<int>(tracks_.size())) return;
    auto &item = tracks_[track_index];

    // Fájl létezésének ellenőrzése betöltés előtt
    if (!QFileInfo::exists(item.file_path)) {
        if (status_lbl_) {
            status_lbl_->setText(QStringLiteral("⚠️ A fájl nem található a megadott helyen! Kérlek add meg az új útvonalat."));
            status_lbl_->setStyleSheet(QStringLiteral("color: #ffaa00; font-size: 11px; font-weight: bold;"));
        }
        relinkTrackFile(track_index);
        return;
    }

    // Konvertáltság ellenőrzése videófájlok esetén: védelem a nehéz 2K/4K/Long-GOP fagyások ellen!
    const QString ext = QFileInfo(item.file_path).suffix().toLower();
    if (!isAudioOnlyFile(ext) && !item.is_converted) {
        if (status_lbl_) {
            status_lbl_->setText(QStringLiteral("⚠️ Ez a videó még nincs 1080p All-Intra formátumra konvertálva! Konvertálás indítása..."));
            status_lbl_->setStyleSheet(QStringLiteral("color: #ffaa00; font-size: 11px; font-weight: bold;"));
        }
        reconvertTrack(track_index);
        return;
    }

    DeckPlayer *target_deck = (deck_channel == 1) ? vj_deck_b() : vj_deck_a();
    if (!target_deck) {
        target_deck = vj_active_deck();
    }

    if (target_deck) {
        if (target_deck->isPlaying()) {
            blog(LOG_WARNING, "[obs_vj_tracklist] Target deck %s is playing; cannot load new track",
                 target_deck->deckName().toUtf8().constData());
            if (status_lbl_) {
                status_lbl_->setText(QStringLiteral("⚠️ A(z) %1 éppen lejátszás alatt áll! Állítsd meg a betöltéshez.").arg(target_deck->deckName()));
                status_lbl_->setStyleSheet(QStringLiteral("color: #f59e0b; font-size: 11px; font-weight: bold;"));
            }
            return;
        }

        target_deck->loadFile(item.file_path);
        item.played = true;
        applyFilterAndPopulate();
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
                        obs_sceneitem_set_bounds_type(s_item, OBS_BOUNDS_SCALE_INNER);
                        struct vec2 bounds = {1920.0f, 1080.0f};
                        obs_sceneitem_set_bounds(s_item, &bounds);
                        obs_sceneitem_set_bounds_alignment(s_item, OBS_ALIGN_TOP | OBS_ALIGN_LEFT);
                    }
                }
            }
            obs_source_release(scene_source);
        }

        if (status_lbl_) {
            status_lbl_->setText(QStringLiteral("Betöltve: %1 -> %2").arg(item.title, target_deck->deckName()));
            status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-size: 11px; font-weight: bold;"));
        }
    }
}

void TracklistDock::onTableItemDoubleClicked(int row, int column)
{
    if (row < 0 || row >= static_cast<int>(filtered_track_indices_.size())) return;
    const int real_idx = filtered_track_indices_[row];
    if (real_idx < 0 || real_idx >= static_cast<int>(tracks_.size())) return;

    if (column == 0) {
        // ST oszlop: ha hiányzik a fájl, dupla kattintásra azonnali újrakeresés (Relink)
        if (!QFileInfo::exists(tracks_[real_idx].file_path)) {
            relinkTrackFile(real_idx);
            return;
        }
    }

    if (column == 2) {
        // BPM oszlop dupla kattintás: azonnali kézi beírás
        bool ok = false;
        const double cur_b = tracks_[real_idx].bpm;
        const double new_b = QInputDialog::getDouble(
            this,
            QStringLiteral("BPM Módosítása"),
            QStringLiteral("Szám: %1\n\nAdd meg a szám pontos BPM értékét:").arg(tracks_[real_idx].title),
            cur_b, 40.0, 300.0, 2, &ok
        );
        if (ok && new_b >= 40.0 && new_b <= 300.0) {
            setTrackBpmDirect(real_idx, new_b);
        }
        return;
    }

    // Bármely más oszlop dupla kattintás: betöltés az aktív deckbe
    loadTrackToDeck(real_idx, -1);
}

void TracklistDock::setTrackBpmDirect(int row, double new_bpm)
{
    if (row < 0 || row >= static_cast<int>(tracks_.size())) return;
    tracks_[row].bpm = std::round(new_bpm * 100.0) / 100.0;

    TrackAnalysisResult res;
    res.detected_bpm = tracks_[row].bpm;
    res.detected_key = tracks_[row].key;
    res.camelot_key  = tracks_[row].camelot;
    res.duration_ms  = tracks_[row].duration_ms;
    AudioAnalyzer::saveCache(tracks_[row].file_path, res);
    if (!tracks_[row].original_path.isEmpty() && tracks_[row].original_path != tracks_[row].file_path) {
        AudioAnalyzer::saveCache(tracks_[row].original_path, res);
    }

    applyFilterAndPopulate();
    saveLibraryJson();

    if (vj_deck_a() && vj_deck_a()->filePath() == tracks_[row].file_path) {
        vj_deck_a()->setBpm(tracks_[row].bpm);
    }
    if (vj_deck_b() && vj_deck_b()->filePath() == tracks_[row].file_path) {
        vj_deck_b()->setBpm(tracks_[row].bpm);
    }
}

void TracklistDock::onRefreshStatus()
{
    for (int r = 0; r < static_cast<int>(filtered_track_indices_.size()); ++r) {
        updateRow(r);
    }
}

void TracklistDock::saveLibraryJson()
{
    if (library_dir_.isEmpty()) return;
    const QString json_path = QDir(library_dir_).filePath(QStringLiteral("vj_tracklist.json"));

    QJsonObject root;

    QJsonArray arr;
    for (const auto &item : tracks_) {
        QJsonObject obj;
        obj[QStringLiteral("title")] = item.title;
        obj[QStringLiteral("file_path")] = item.file_path;
        obj[QStringLiteral("original_path")] = item.original_path;
        obj[QStringLiteral("bpm")] = item.bpm;
        obj[QStringLiteral("key")] = item.key;
        obj[QStringLiteral("camelot")] = item.camelot;
        obj[QStringLiteral("duration_ms")] = static_cast<qint64>(item.duration_ms);
        obj[QStringLiteral("is_converted")] = item.is_converted;
        obj[QStringLiteral("played")] = false;
        obj[QStringLiteral("status")] = item.status;
        arr.append(obj);
    }
    root[QStringLiteral("tracks")] = arr;

    // Save playlists and quick slots
    const QJsonObject pm_obj = playlist_mgr_.toJson();
    root[QStringLiteral("playlists")] = pm_obj[QStringLiteral("playlists")];
    root[QStringLiteral("quick_slots")] = pm_obj[QStringLiteral("quick_slots")];
    root[QStringLiteral("active_slot")] = pm_obj[QStringLiteral("active_slot")];

    QJsonDocument doc(root);
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

        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            const QJsonObject root = doc.object();
            arr = root[QStringLiteral("tracks")].toArray();
            playlist_mgr_.fromJson(root);
        }

        for (const auto &val : arr) {
            const QJsonObject obj = val.toObject();
            TrackItem item;
            item.title = obj[QStringLiteral("title")].toString();
            item.file_path = obj[QStringLiteral("file_path")].toString();
            item.original_path = obj[QStringLiteral("original_path")].toString();
            item.bpm = obj[QStringLiteral("bpm")].toDouble(128.0);
            item.key = obj[QStringLiteral("key")].toString(QStringLiteral("--"));
            item.camelot = obj[QStringLiteral("camelot")].toString(QStringLiteral("--"));
            item.duration_ms = obj[QStringLiteral("duration_ms")].toInteger();
            item.is_converted = obj[QStringLiteral("is_converted")].toBool();
            item.played = false;
            QString st = obj[QStringLiteral("status")].toString(QStringLiteral("Ready"));
            if (st.startsWith(QStringLiteral("Conv")) || st == QStringLiteral("Várakozik...")) {
                st = QStringLiteral("Ready");
            }
            item.status = st;

            TrackAnalysisResult c_res;
            if (AudioAnalyzer::loadCache(item.file_path, c_res) || AudioAnalyzer::loadCache(item.original_path, c_res)) {
                if (c_res.detected_bpm > 20.0) {
                    item.bpm = c_res.detected_bpm;
                }
                if (!c_res.detected_key.isEmpty() && c_res.detected_key != QStringLiteral("--")) {
                    item.key = c_res.detected_key;
                }
                if (!c_res.camelot_key.isEmpty() && c_res.camelot_key != QStringLiteral("--")) {
                    item.camelot = c_res.camelot_key;
                }
            }

            // Önjavítás: ha egy videó korábban is_converted = true-nak lett beállítva,
            // de valójában nyers videófájlra mutat és a cél _vj1080.mp4 nem létezik:
            const QFileInfo fi(item.file_path);
            const QString ext = fi.suffix().toLower();
            if (!isAudioOnlyFile(ext)) {
                const QString target_conv = QDir(library_dir_).filePath(
                    fi.completeBaseName() + QStringLiteral("_vj1080.mp4")
                );
                if (QFile::exists(target_conv) && QFileInfo(target_conv).size() > 1024) {
                    item.file_path = target_conv;
                    item.is_converted = true;
                    item.status = QStringLiteral("Ready");
                } else if (!item.file_path.contains(QStringLiteral("_vj")) || !QFile::exists(item.file_path)) {
                    item.is_converted = false;
                    item.status = QStringLiteral("Konvertálatlan");
                }
            }

            tracks_.push_back(item);
        }
    }

    updateSlotButtons();
    applyFilterAndPopulate();

    // Automatikus háttér-analízis a hiányos zenei kulcsú dalokra (ha léteznek a lemezen)
    for (size_t i = 0; i < tracks_.size(); ++i) {
        if ((tracks_[i].camelot.isEmpty() || tracks_[i].camelot == QStringLiteral("--")) &&
            QFileInfo::exists(tracks_[i].file_path)) {
            analysis_queue_.push_back(static_cast<int>(i));
        }
    }
    if (!analysis_queue_.empty()) {
        total_analysis_count_ = static_cast<int>(analysis_queue_.size());
        done_analysis_count_ = 0;
        if (status_lbl_) {
            status_lbl_->setText(QStringLiteral("⚡ Zenei háttér-analízis (%1 korábbi dal kulcsának frissítése)...").arg(total_analysis_count_));
        }
        processNextAnalysis();
    }
}

void TracklistDock::updateTrackBpm(const QString &file_path, double bpm)
{
    if (file_path.isEmpty() || bpm < 20.0) return;
    const QString clean_target = QDir::cleanPath(file_path);
    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (QDir::cleanPath(tracks_[i].file_path).compare(clean_target, Qt::CaseInsensitive) == 0 ||
            QDir::cleanPath(tracks_[i].original_path).compare(clean_target, Qt::CaseInsensitive) == 0) {
            tracks_[i].bpm = std::round(bpm * 100.0) / 100.0;
            saveLibraryJson();
            applyFilterAndPopulate();
            break;
        }
    }
}

void vdj_notify_track_bpm_changed(const QString &file_path, double new_bpm)
{
    if (g_tracklist_instance) {
        g_tracklist_instance->updateTrackBpm(file_path, new_bpm);
    }
}
