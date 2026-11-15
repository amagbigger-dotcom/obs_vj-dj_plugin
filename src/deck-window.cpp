#include "deck-window.hpp"
#include "deck-engine.hpp"
#include "waveform-widget.hpp"

#include <obs-frontend-api.h>
#include <obs.h>

#include <QFileDialog>
#include <QDoubleValidator>
#include <QDateTime>
#include <QSignalBlocker>
#include <QGroupBox>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

static QString formatTime(int64_t ms)
{
    if (ms < 0) ms = 0;
    const int total_sec = static_cast<int>(ms / 1000);
    const int hours = total_sec / 3600;
    const int mins = (total_sec % 3600) / 60;
    const int secs = total_sec % 60;
    const int millis = static_cast<int>((ms % 1000) / 10);
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hours)
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'))
            .arg(millis, 2, 10, QChar('0'));
    }
    return QStringLiteral("%1:%2.%3")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(millis, 2, 10, QChar('0'));
}

DeckWindow::DeckWindow(DeckPlayer *deck, QWidget *parent)
    : QDialog(parent), deck_(deck)
{
    setWindowTitle(QStringLiteral("OBS VJ Deck"));
    // Fix kompakt méret a monitor alsó egyharmadához igazítva (1/3 Screen Height, 640x340 px)
    setFixedSize(640, 340);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setAcceptDrops(true);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #12151c; color: #e0e4ec; font-family: 'Segoe UI', Arial, sans-serif; }"
        "QLabel { color: #cfd6e4; font-size: 11px; }"
        "QPushButton { background: #222834; border: 1px solid #384255; border-radius: 4px; color: #ffffff; padding: 4px 10px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background: #2f384a; border-color: #00d2ff; }"
        "QPushButton:pressed { background: #181d26; }"
        "QSlider::groove:horizontal { height: 6px; background: #1c222c; border: 1px solid #2e3746; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #00d2ff; width: 18px; margin: -5px 0; border-radius: 3px; border: 1px solid #ffffff; }"
        "QSlider::handle:horizontal:hover { background: #38e0ff; }"
    ));

    setupUi();
    updateDeckButtonStates();

    pitch_throttle_timer_ = new QTimer(this);
    pitch_throttle_timer_->setSingleShot(true);
    connect(pitch_throttle_timer_, &QTimer::timeout, this, [this]() {
        if (deck_) {
            deck_->applySpeedSettings(pending_throttle_speed_);
        }
    });

    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, &DeckWindow::onRefresh);
    refresh_timer_->start(25);
}

DeckWindow::~DeckWindow()
{
    if (pitch_throttle_timer_) {
        pitch_throttle_timer_->stop();
        delete pitch_throttle_timer_;
    }
    if (refresh_timer_) {
        refresh_timer_->stop();
        delete refresh_timer_;
    }
}

void DeckWindow::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 6, 8, 6);
    main_layout->setSpacing(4);
    main_layout->setSizeConstraint(QLayout::SetNoConstraint);

    // =========================================================================
    // 1. FEJLÉC SÁV: Média megnyitás, Fájlnév, Időkódok, Közvetlen BPM (Top Bar)
    // =========================================================================
    auto *top_bar = new QHBoxLayout();
    top_bar->setSpacing(6);

    open_btn_ = new QPushButton(QStringLiteral("📂"), this);
    open_btn_->setAutoDefault(false);
    open_btn_->setDefault(false);
    open_btn_->setFixedSize(26, 24);
    open_btn_->setStyleSheet(QStringLiteral("background: #0088cc; border: 1px solid #00aaff; color: #ffffff; font-size: 12px; padding: 0px; font-weight: bold; border-radius: 3px;"));
    open_btn_->setToolTip(QStringLiteral("Médiafájl megnyitása ebbe a deckbe (Open)"));
    connect(open_btn_, &QPushButton::clicked, this, &DeckWindow::openFileDialog);
    top_bar->addWidget(open_btn_);

    file_lbl_ = new QLabel(QStringLiteral("No Track Loaded"), this);
    file_lbl_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    file_lbl_->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #00d2ff; padding-left: 2px;"));
    top_bar->addWidget(file_lbl_, 1);

    // Kompakt és precíz időkód kijelző (Elapsed and Remaining Timecode Display)
    auto *time_container = new QFrame(this);
    time_container->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    time_container->setStyleSheet(QStringLiteral(
        "QFrame { background: #0c1017; border: 1px solid #1e2838; border-radius: 4px; }"
    ));
    auto *time_layout = new QHBoxLayout(time_container);
    time_layout->setContentsMargins(5, 2, 5, 2);
    time_layout->setSpacing(4);

    auto *el_title = new QLabel(QStringLiteral("TIME"), time_container);
    el_title->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #586884;"));
    time_layout->addWidget(el_title);

    elapsed_lbl_ = new QLabel(QStringLiteral("00:00.00"), time_container);
    elapsed_lbl_->setMinimumWidth(58);
    elapsed_lbl_->setStyleSheet(QStringLiteral("font-family: 'Consolas', monospace; font-size: 12px; font-weight: bold; color: #00ffaa;"));
    time_layout->addWidget(elapsed_lbl_);

    auto *rem_title = new QLabel(QStringLiteral("REMAIN"), time_container);
    rem_title->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #586884; margin-left: 2px;"));
    time_layout->addWidget(rem_title);

    remaining_lbl_ = new QLabel(QStringLiteral("-00:00.00"), time_container);
    remaining_lbl_->setMinimumWidth(64);
    remaining_lbl_->setStyleSheet(QStringLiteral("font-family: 'Consolas', monospace; font-size: 12px; font-weight: bold; color: #ffaa00;"));
    time_layout->addWidget(remaining_lbl_);

    top_bar->addWidget(time_container, 0);

    // Közvetlen kézi BPM beviteli mező (In-Place Direct BPM Input, Enter commits)
    auto *bpm_box = new QHBoxLayout();
    bpm_box->setSpacing(3);

    bpm_edit_ = new QLineEdit(QStringLiteral("128.00"), this);
    bpm_edit_->setFixedWidth(56);
    bpm_edit_->setFixedHeight(24);
    bpm_edit_->setAlignment(Qt::AlignCenter);
    bpm_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #141b24; color: #00d2ff; font-family: 'Consolas', monospace; font-size: 12px; font-weight: bold; border: 1px solid #2e3e54; border-radius: 3px; padding: 2px 2px; }"
        "QLineEdit:focus { border: 1px solid #00d2ff; background: #1c2635; color: #ffffff; }"
    ));
    bpm_edit_->setToolTip(QStringLiteral("BPM kézi beírása (Enter lenyomására azonnal menti és igazítja a rácsot)"));
    auto *bpm_val = new QDoubleValidator(40.0, 300.0, 2, bpm_edit_);
    bpm_val->setNotation(QDoubleValidator::StandardNotation);
    bpm_edit_->setValidator(bpm_val);

    connect(bpm_edit_, &QLineEdit::returnPressed, this, [this]() {
        bool ok = false;
        const double val = bpm_edit_->text().toDouble(&ok);
        if (ok && deck_) {
            deck_->setBpm(val);
        }
        bpm_edit_->clearFocus();
    });

    connect(bpm_edit_, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        const double val = bpm_edit_->text().toDouble(&ok);
        if (ok && deck_) {
            deck_->setBpm(val);
        }
    });

    bpm_box->addWidget(bpm_edit_);

    auto *bpm_unit = new QLabel(QStringLiteral("BPM"), this);
    bpm_unit->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px; margin-left: 1px;"));
    bpm_box->addWidget(bpm_unit);
    top_bar->addLayout(bpm_box);

    tracklist_btn_ = new QPushButton(QStringLiteral("📋"), this);
    tracklist_btn_->setAutoDefault(false);
    tracklist_btn_->setDefault(false);
    tracklist_btn_->setFixedSize(26, 24);
    tracklist_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f2b3c; border: 1px solid #385070; color: #00d2ff; font-size: 12px; padding: 0px; font-weight: bold; border-radius: 3px; }"
        "QPushButton:hover { background: #29384e; border-color: #00d2ff; }"
    ));
    tracklist_btn_->setToolTip(QStringLiteral("Médiatár és 1080p videokonvertáló megnyitása (Tracklist)"));
    connect(tracklist_btn_, &QPushButton::clicked, this, []() {
        extern void vdj_open_tracklist_window();
        vdj_open_tracklist_window();
    });
    top_bar->addWidget(tracklist_btn_, 0);

    main_layout->addLayout(top_bar);

    // ==========================================
    // 2. CLEAN WAVEFORM CANVAS (No duplicate header!)
    // ==========================================
    waveform_canvas_ = new WaveformCanvas(deck_, true, this);
    waveform_canvas_->setFixedHeight(85);
    main_layout->addWidget(waveform_canvas_);

    // ==========================================
    // 3. BEATGRID & MOVE CONTROLS
    // ==========================================
    auto *move_bar = new QHBoxLayout();
    move_bar->setSpacing(4);

    auto *grid_lbl = new QLabel(QStringLiteral("GRID:"), this);
    grid_lbl->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 10px;"));
    move_bar->addWidget(grid_lbl);

    grid_nudge_minus_ = new QPushButton(QStringLiteral("−10"), this);
    grid_nudge_minus_->setFixedSize(36, 22);
    grid_nudge_minus_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 10px;"));
    grid_nudge_minus_->setToolTip(QStringLiteral("Ütemrács tolás: −10ms"));
    connect(grid_nudge_minus_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->nudgeGrid(-10);
    });
    move_bar->addWidget(grid_nudge_minus_);

    grid_nudge_minus_1ms_ = new QPushButton(QStringLiteral("−1"), this);
    grid_nudge_minus_1ms_->setFixedSize(28, 22);
    grid_nudge_minus_1ms_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 10px;"));
    grid_nudge_minus_1ms_->setToolTip(QStringLiteral("Ütemrács finomhangolás: −1ms"));
    connect(grid_nudge_minus_1ms_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->nudgeGrid(-1);
    });
    move_bar->addWidget(grid_nudge_minus_1ms_);

    grid_set1_btn_ = new QPushButton(QStringLiteral("SET 1"), this);
    grid_set1_btn_->setFixedSize(48, 22);
    grid_set1_btn_->setStyleSheet(QStringLiteral("background: #1c3248; border-color: #00aaff; color: #00d2ff; padding: 0px; font-size: 10px; font-weight: bold;"));
    grid_set1_btn_->setToolTip(QStringLiteral("1. ütem leütés (Downbeat) rögzítése a lejátszófej pozíciójához"));
    connect(grid_set1_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->setGridAnchorAtCurrent();
    });
    move_bar->addWidget(grid_set1_btn_);

    grid_nudge_plus_1ms_ = new QPushButton(QStringLiteral("+1"), this);
    grid_nudge_plus_1ms_->setFixedSize(28, 22);
    grid_nudge_plus_1ms_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 10px;"));
    grid_nudge_plus_1ms_->setToolTip(QStringLiteral("Ütemrács finomhangolás: +1ms"));
    connect(grid_nudge_plus_1ms_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->nudgeGrid(1);
    });
    move_bar->addWidget(grid_nudge_plus_1ms_);

    grid_nudge_plus_ = new QPushButton(QStringLiteral("+10"), this);
    grid_nudge_plus_->setFixedSize(36, 22);
    grid_nudge_plus_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 10px;"));
    grid_nudge_plus_->setToolTip(QStringLiteral("Ütemrács tolás: +10ms"));
    connect(grid_nudge_plus_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->nudgeGrid(10);
    });
    move_bar->addWidget(grid_nudge_plus_);

    grid_offset_lbl_ = new QLabel(QStringLiteral("0ms"), this);
    grid_offset_lbl_->setFixedWidth(52);
    grid_offset_lbl_->setAlignment(Qt::AlignCenter);
    grid_offset_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-family: monospace; font-weight: bold; font-size: 10px;"));
    move_bar->addWidget(grid_offset_lbl_);

    move_bar->addSpacing(8);

    auto *zoom_hdr = new QLabel(QStringLiteral("ZOOM:"), this);
    zoom_hdr->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 10px;"));
    move_bar->addWidget(zoom_hdr);

    zoom_out_btn_ = new QPushButton(QStringLiteral("−"), this);
    zoom_out_btn_->setFixedSize(22, 22);
    zoom_out_btn_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 11px; font-weight: bold;"));
    zoom_out_btn_->setToolTip(QStringLiteral("Waveform kicsinyítés (szélesebb szakasz, lassabb gördülés)"));
    connect(zoom_out_btn_, &QPushButton::clicked, this, [this]() {
        if (waveform_canvas_) waveform_canvas_->zoomOut();
    });
    move_bar->addWidget(zoom_out_btn_);

    zoom_lbl_ = new QLabel(QStringLiteral("6s"), this);
    zoom_lbl_->setFixedWidth(24);
    zoom_lbl_->setAlignment(Qt::AlignCenter);
    zoom_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-family: monospace; font-weight: bold; font-size: 10px;"));
    move_bar->addWidget(zoom_lbl_);

    zoom_in_btn_ = new QPushButton(QStringLiteral("+"), this);
    zoom_in_btn_->setFixedSize(22, 22);
    zoom_in_btn_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 11px; font-weight: bold;"));
    zoom_in_btn_->setToolTip(QStringLiteral("Waveform nagyítás (részletesebb nézet)"));
    connect(zoom_in_btn_, &QPushButton::clicked, this, [this]() {
        if (waveform_canvas_) waveform_canvas_->zoomIn();
    });
    move_bar->addWidget(zoom_in_btn_);

    connect(waveform_canvas_, &WaveformCanvas::zoomChanged, this, [this](double sec) {
        if (zoom_lbl_) zoom_lbl_->setText(QStringLiteral("%1s").arg(sec, 0, 'f', 0));
    });

    move_bar->addStretch(1);

    // =========================================================================
    // TAP BPM gomb valós idejű intervallum-számítással és automatikus újraindítással
    // =========================================================================
    tap_bpm_btn_ = new QPushButton(QStringLiteral("TAP"), this);
    tap_bpm_btn_->setFixedSize(38, 22);
    tap_bpm_btn_->setStyleSheet(QStringLiteral("background: #1c2838; border: 1px solid #2e4460; color: #00d2ff; font-size: 10px; font-weight: bold; padding: 0px; border-radius: 3px;"));
    tap_bpm_btn_->setToolTip(QStringLiteral("BPM kileütése ritmusra (Tap Tempo)"));
    connect(tap_bpm_btn_, &QPushButton::clicked, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (!tap_times_.empty()) {
            const qint64 diff = now - tap_times_.back();
            if (diff > 2000) {
                tap_times_.clear();
            }
        }
        tap_times_.push_back(now);
        while (tap_times_.size() > 8) {
            tap_times_.erase(tap_times_.begin());
        }
        if (tap_times_.size() >= 3) {
            double total_diff = 0;
            for (size_t i = 1; i < tap_times_.size(); i++) {
                total_diff += (tap_times_[i] - tap_times_[i - 1]);
            }
            const double avg_ms = total_diff / (tap_times_.size() - 1);
            if (avg_ms > 100.0) {
                const double bpm = std::clamp(60000.0 / avg_ms, 40.0, 300.0);
                if (deck_) {
                    deck_->setBpm(bpm);
                    if (bpm_edit_) {
                        bpm_edit_->setText(QString::number(bpm, 'f', 2));
                    }
                }
            }
        }
    });
    move_bar->addWidget(tap_bpm_btn_);

    main_layout->addLayout(move_bar);

    // ==========================================
    // 4. AUTO-LOOP MATRIX & JOG NUDGE
    // ==========================================
    auto *loop_bar = new QHBoxLayout();
    loop_bar->setSpacing(3);

    auto *loop_hdr = new QLabel(QStringLiteral("LOOP:"), this);
    loop_hdr->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px;"));
    loop_bar->addWidget(loop_hdr);

    const QStringList loop_labels = {
        "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16", "32", "64", "128"
    };
    for (int i = 0; i < loop_labels.size(); i++) {
        auto *btn = new QPushButton(loop_labels[i], this);
        btn->setFixedSize(30, 22);
        btn->setStyleSheet(QStringLiteral("background: #222834; border: 1px solid #384255; color: #ffffff; font-weight: bold; border-radius: 3px; padding: 0px; font-size: 8px;"));
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            if (!deck_) return;
            if (deck_->isLooping() && deck_->loopBeatIndex() == i) {
                deck_->setLooping(false);
            } else {
                deck_->setLoopBeatIndex(i);
                deck_->setLooping(true);
            }
        });
        loop_btns_.push_back(btn);
        loop_bar->addWidget(btn);
    }

    loop_bar->addSpacing(4);

    auto *jog_hdr = new QLabel(QStringLiteral("JOG:"), this);
    jog_hdr->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px;"));
    loop_bar->addWidget(jog_hdr);

    jog_minus_btn_ = new QPushButton(QStringLiteral("◀ NUDGE"), this);
    jog_minus_btn_->setFixedSize(48, 22);
    jog_minus_btn_->setAutoRepeat(true);
    jog_minus_btn_->setAutoRepeatDelay(250);
    jog_minus_btn_->setAutoRepeatInterval(80);
    jog_minus_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2634; border: 1px solid #385070; font-size: 8px; font-weight: bold; color: #78a0d0; border-radius: 3px; padding: 0px; } "
        "QPushButton:hover { background: #26384e; color: #fff; } "
        "QPushButton:pressed { background: #0088cc; color: #fff; }"));
    jog_minus_btn_->setToolTip(QStringLiteral("Jog Nudge: lemezt lassítja / visszatartja 20ms-mal"));
    connect(jog_minus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        deck_->nudgePlayback(-20);
    });
    loop_bar->addWidget(jog_minus_btn_);

    jog_plus_btn_ = new QPushButton(QStringLiteral("NUDGE ▶"), this);
    jog_plus_btn_->setFixedSize(48, 22);
    jog_plus_btn_->setAutoRepeat(true);
    jog_plus_btn_->setAutoRepeatDelay(250);
    jog_plus_btn_->setAutoRepeatInterval(80);
    jog_plus_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2634; border: 1px solid #385070; font-size: 8px; font-weight: bold; color: #78a0d0; border-radius: 3px; padding: 0px; } "
        "QPushButton:hover { background: #26384e; color: #fff; } "
        "QPushButton:pressed { background: #0088cc; color: #fff; }"));
    jog_plus_btn_->setToolTip(QStringLiteral("Jog Nudge: lemezt gyorsítja / megtolja 20ms-mal"));
    connect(jog_plus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        deck_->nudgePlayback(+20);
    });
    loop_bar->addWidget(jog_plus_btn_);

    loop_bar->addStretch(1);
    main_layout->addLayout(loop_bar);

    // ==========================================
    // 5. TRANSPORT (CUE, PLAY, SYNC, VOLUME)
    // ==========================================
    auto *transport_bar = new QHBoxLayout();
    transport_bar->setSpacing(8);

    cue_btn_ = new QPushButton(QStringLiteral("CUE"), this);
    cue_btn_->setFixedSize(65, 34);
    cue_btn_->setStyleSheet(QStringLiteral("background: #2a3444; border: 2px solid #586884; font-size: 12px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    connect(cue_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->cue();
    });
    transport_bar->addWidget(cue_btn_);

    play_btn_ = new QPushButton(QStringLiteral("▶ PLAY"), this);
    play_btn_->setFixedSize(90, 34);
    play_btn_->setStyleSheet(QStringLiteral("background: #1e3a24; border: 2px solid #00cc44; font-size: 13px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    connect(play_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->togglePlayback();
    });
    transport_bar->addWidget(play_btn_);

    sync_btn_ = new QPushButton(QStringLiteral("SYNC"), this);
    sync_btn_->setFixedSize(65, 34);
    sync_btn_->setCheckable(true);
    sync_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2634; border: 2px solid #385070; font-size: 11px; font-weight: bold; color: #78a0d0; border-radius: 4px; padding: 0px; }"
        "QPushButton:checked { background: #005588; border-color: #00d2ff; color: #ffffff; }"
    ));
    connect(sync_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        DeckPair *decks = vdj_decks();
        if (decks) {
            const int ch = (deck_->side() == DeckPlayer::DeckSide::B) ? 1 : 0;
            const bool new_sync = !deck_->isSync();
            decks->setSync(ch, new_sync);
            sync_btn_->setChecked(new_sync);
        }
    });
    transport_bar->addWidget(sync_btn_);

    master_btn_ = new QPushButton(QStringLiteral("MASTER"), this);
    master_btn_->setFixedSize(65, 34);
    master_btn_->setCheckable(true);
    master_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2634; border: 2px solid #385070; font-size: 11px; font-weight: bold; color: #78a0d0; border-radius: 4px; padding: 0px; }"
        "QPushButton:checked { background: #d97706; border-color: #f59e0b; color: #ffffff; }"
    ));
    connect(master_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        DeckPair *decks = vdj_decks();
        if (decks) {
            const int ch = (deck_->side() == DeckPlayer::DeckSide::B) ? 1 : 0;
            decks->setDeckMaster(ch);
        }
    });
    transport_bar->addWidget(master_btn_);

    auto *hc_hdr = new QLabel(QStringLiteral("HOT CUE:"), this);
    hc_hdr->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px; margin-left: 6px;"));
    transport_bar->addWidget(hc_hdr);

    for (int i = 0; i < 4; ++i) {
        auto *btn = new QPushButton(QString::asprintf("%d", i + 1), this);
        btn->setFixedSize(30, 34);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        hot_cue_btns_[i] = btn;
        transport_bar->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            if (!deck_) return;
            deck_->triggerHotCue(i);
            updateHotCueButtons();
        });

        connect(btn, &QPushButton::customContextMenuRequested, this, [this, i](const QPoint &) {
            if (!deck_) return;
            deck_->clearHotCue(i);
            updateHotCueButtons();
        });
    }

    updateHotCueButtons();

    transport_bar->addStretch(1);
    main_layout->addLayout(transport_bar);

    // ==========================================
    // 6. CONTINUOUS REAL-TIME TEMPO / PITCH FADER
    // ==========================================
    auto *tempo_bar = new QHBoxLayout();
    tempo_bar->setSpacing(6);

    auto *tempo_hdr = new QLabel(QStringLiteral("TEMPO:"), this);
    tempo_hdr->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 10px;"));
    tempo_bar->addWidget(tempo_hdr);

    bend_minus_btn_ = new QPushButton(QStringLiteral("−"), this);
    bend_minus_btn_->setFixedSize(26, 24);
    bend_minus_btn_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 11px; font-weight: bold;"));
    bend_minus_btn_->setToolTip(QStringLiteral("Tempo léptetés: első klikk kerekít tizedre, utána −0.10 BPM"));
    connect(bend_minus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        const double cur_eff = deck_->effectiveBpm();
        double target_bpm = 0.0;
        if (std::abs(cur_eff - (std::round(cur_eff * 10.0) / 10.0)) < 0.005) {
            target_bpm = (std::round(cur_eff * 10.0) - 1.0) / 10.0;
        } else {
            target_bpm = std::floor(cur_eff * 10.0) / 10.0;
        }
        deck_->setPlaybackBpm(target_bpm);
    });
    tempo_bar->addWidget(bend_minus_btn_);

    pitch_slider_ = new QSlider(Qt::Horizontal, this);
    pitch_slider_->setRange(50, 150);
    pitch_slider_->setValue(100);
    pitch_slider_->setTracking(true);

    connect(pitch_slider_, &QSlider::valueChanged, this, [this](int val) {
        if (deck_) {
            if (deck_->isSync()) {
                deck_->setSync(false);
                DeckPair *dp = vdj_decks();
                if (dp) {
                    const int ch = (deck_->side() == DeckPlayer::DeckSide::B) ? 1 : 0;
                    dp->setSync(ch, false);
                }
            }
            deck_->setSpeed(val);
        }
        const double eff_bpm = deck_ ? deck_->effectiveBpm() : 128.0;
        const double base_bpm = (deck_ && deck_->bpm() > 1.0) ? deck_->bpm() : 128.0;
        const double diff_pct = (eff_bpm / base_bpm - 1.0) * 100.0;
        pitch_val_lbl_->setText(QString::asprintf("%.2f BPM (%+.1f%%)", eff_bpm, diff_pct));

        // Debounced speed update: reset timer on every move so FFmpeg only reloads once user pauses/settles
        pending_throttle_speed_ = val;
        pitch_throttle_timer_->start(350);
    });

    connect(pitch_slider_, &QSlider::sliderReleased, this, [this]() {
        pitch_throttle_timer_->stop();
        if (deck_ && pitch_slider_) {
            deck_->applySpeedSettings(pitch_slider_->value());
        }
    });
    tempo_bar->addWidget(pitch_slider_, 1);

    bend_plus_btn_ = new QPushButton(QStringLiteral("+"), this);
    bend_plus_btn_->setFixedSize(26, 24);
    bend_plus_btn_->setStyleSheet(QStringLiteral("padding: 0px; font-size: 11px; font-weight: bold;"));
    bend_plus_btn_->setToolTip(QStringLiteral("Tempo léptetés: első klikk kerekít tizedre, utána +0.10 BPM"));
    connect(bend_plus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        const double cur_eff = deck_->effectiveBpm();
        double target_bpm = 0.0;
        if (std::abs(cur_eff - (std::round(cur_eff * 10.0) / 10.0)) < 0.005) {
            target_bpm = (std::round(cur_eff * 10.0) + 1.0) / 10.0;
        } else {
            target_bpm = std::ceil(cur_eff * 10.0) / 10.0;
        }
        deck_->setPlaybackBpm(target_bpm);
    });
    tempo_bar->addWidget(bend_plus_btn_);

    pitch_val_lbl_ = new QLabel(QStringLiteral("128.00 BPM (+0.0%)"), this);
    pitch_val_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-family: monospace; font-weight: bold; font-size: 11px;"));
    pitch_val_lbl_->setFixedWidth(135);
    tempo_bar->addWidget(pitch_val_lbl_);

    pitch_reset_btn_ = new QPushButton(QStringLiteral("0% RESET"), this);
    pitch_reset_btn_->setFixedSize(60, 24);
    pitch_reset_btn_->setStyleSheet(QStringLiteral("background: #2a3444; border-color: #48566e; color: #cfd6e4; font-size: 10px; padding: 0px;"));
    connect(pitch_reset_btn_, &QPushButton::clicked, this, [this]() {
        if (pitch_slider_) pitch_slider_->setValue(100);
        if (deck_) {
            deck_->setPlaybackBpm(deck_->bpm());
            deck_->applySpeedSettings(100);
        }
    });
    tempo_bar->addWidget(pitch_reset_btn_);

    main_layout->addLayout(tempo_bar);

    for (auto *btn : findChildren<QPushButton *>()) {
        btn->setAutoDefault(false);
        btn->setDefault(false);
    }
}

void DeckWindow::bindDeck(DeckPlayer *deck)
{
    deck_ = deck;
    if (waveform_canvas_) {
        waveform_canvas_->setDeck(deck_, !deck_ || deck_->side() == DeckPlayer::DeckSide::A);
    }
    updateDeckButtonStates();
    fitTo1080p();
}

void DeckWindow::updateDeckButtonStates()
{
    const bool is_a = (!deck_ || deck_->side() == DeckPlayer::DeckSide::A);
    setWindowTitle(is_a ? QStringLiteral("OBS VJ Deck A") : QStringLiteral("OBS VJ Deck B"));
}

void DeckWindow::fitTo1080p()
{
    obs_source_t *scene_source = obs_frontend_get_current_scene();
    if (!scene_source) return;
    obs_scene_t *scene = obs_scene_from_source(scene_source);
    if (scene) {
        const char *src_names[] = {"OBS VJ Deck", "OBS VJ Deck A", "OBS VJ Deck B"};
        for (const char *name : src_names) {
            obs_sceneitem_t *item = obs_scene_find_source(scene, name);
            if (item) {
                struct vec2 pos = {0.0f, 0.0f};
                struct vec2 scale = {1.0f, 1.0f};
                obs_sceneitem_set_pos(item, &pos);
                obs_sceneitem_set_scale(item, &scale);
                obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
            }
        }
    }
    obs_source_release(scene_source);
}

void DeckWindow::openFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Média megnyitása"),
        QString(),
        QStringLiteral("Media Files (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav *.flac *.aac);;All Files (*.*)")
    );

    if (!path.isEmpty()) {
        extern void vdj_import_and_load(const QString &file_path, int deck_channel);
        const int channel = (deck_ && deck_->side() == DeckPlayer::DeckSide::B) ? 1 : 0;
        vdj_import_and_load(path, channel);
    }
}

void DeckWindow::onRefresh()
{
    if (!deck_) return;

    // 1. Refresh Waveform Canvas continuously in real-time!
    if (waveform_canvas_) {
        waveform_canvas_->refreshCanvas();
    }

    // 2. Track filename
    const QString fn = deck_->fileName();
    if (!fn.isEmpty()) {
        QFontMetrics fm(file_lbl_->font());
        const int avail = file_lbl_->width() > 30 ? file_lbl_->width() - 4 : 260;
        const QString elided = fm.elidedText(fn, Qt::ElideMiddle, avail);
        if (file_lbl_->text() != elided) {
            file_lbl_->setText(elided);
            file_lbl_->setToolTip(fn);
        }
    } else {
        file_lbl_->setText(QStringLiteral("No Track Loaded"));
        file_lbl_->setToolTip(QString());
    }

    // 3. Timecodes
    const int64_t cur_ms = deck_->currentTimeMs();
    const int64_t dur_ms = deck_->durationMs();
    const int64_t rem_ms = std::max<int64_t>(0, dur_ms - cur_ms);

    elapsed_lbl_->setText(formatTime(cur_ms));
    remaining_lbl_->setText(QStringLiteral("-%1").arg(formatTime(rem_ms)));

    // 4. In-Place BPM mező: a zeneszám rács/alap BPM-ét mutatja (Track Grid Base BPM)
    if (bpm_edit_ && !bpm_edit_->hasFocus()) {
        const double base_b = deck_->bpm();
        bpm_edit_->setText(QString::number(base_b, 'f', 2));
    }

    // 5. Beatgrid offset
    const int64_t offset = deck_->gridOffsetMs();
    grid_offset_lbl_->setText(QStringLiteral("%1%2ms")
        .arg(offset >= 0 ? "+" : "")
        .arg(offset));

    // 6. Play button state
    const bool is_playing = deck_->isPlaying();
    if (is_playing) {
        play_btn_->setText(QStringLiteral("⏸ PAUSE"));
        play_btn_->setStyleSheet(QStringLiteral("background: #00aa44; border: 2px solid #00ff55; font-size: 13px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    } else {
        play_btn_->setText(QStringLiteral("▶ PLAY"));
        play_btn_->setStyleSheet(QStringLiteral("background: #1e3a24; border: 2px solid #00cc44; font-size: 13px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    }

    // 7. Master & Sync button states
    const bool is_m = deck_->isMaster();
    if (master_btn_->isChecked() != is_m) {
        QSignalBlocker b(master_btn_);
        master_btn_->setChecked(is_m);
    }
    if (sync_btn_->isChecked() != deck_->isSync()) {
        QSignalBlocker b(sync_btn_);
        sync_btn_->setChecked(deck_->isSync());
    }

    // 8. Loop buttons highlight (állapotváltáskor frissül, 0px paddinggal, hogy a méret soha ne mozduljon)
    const bool looping = deck_->isLooping();
    const int loop_idx = deck_->loopBeatIndex();
    if (looping != last_looping_ || loop_idx != last_loop_idx_) {
        last_looping_ = looping;
        last_loop_idx_ = loop_idx;
        for (size_t i = 0; i < loop_btns_.size(); i++) {
            if (looping && static_cast<int>(i) == loop_idx) {
                loop_btns_[i]->setStyleSheet(QStringLiteral("background: #ff8800; border: 1px solid #ffaa00; color: #000000; font-weight: bold; border-radius: 3px; padding: 0px; font-size: 8px;"));
            } else {
                loop_btns_[i]->setStyleSheet(QStringLiteral("background: #222834; border: 1px solid #384255; color: #ffffff; font-weight: bold; border-radius: 3px; padding: 0px; font-size: 8px;"));
            }
        }
    }

    // 9. Pitch slider synchronization (if user isn't dragging it)
    if (pitch_slider_ && !pitch_slider_->isSliderDown()) {
        const int spd = deck_->speed();
        if (pitch_slider_->value() != spd) {
            QSignalBlocker b(pitch_slider_);
            pitch_slider_->setValue(spd);
        }
        const double eff_bpm = deck_->effectiveBpm();
        const double base_bpm = (deck_->bpm() > 1.0) ? deck_->bpm() : 128.0;
        const double diff_pct = (eff_bpm / base_bpm - 1.0) * 100.0;
        pitch_val_lbl_->setText(QString::asprintf("%.2f BPM (%+.1f%%)", eff_bpm, diff_pct));
    }

    // 10. Hot Cue buttons state update
    updateHotCueButtons();
}

void DeckWindow::updateHotCueButtons()
{
    if (!deck_) return;
    const char *hc_colors[4] = {"#ff8800", "#00d2ff", "#00ff66", "#ff00bb"};
    for (int i = 0; i < 4; ++i) {
        auto *btn = hot_cue_btns_[i];
        if (!btn) continue;
        const int64_t ms = deck_->hotCueMs(i);
        if (ms >= 0) {
            const int total_sec = static_cast<int>(ms / 1000);
            const int m = total_sec / 60;
            const int s = total_sec % 60;
            const int cs = static_cast<int>((ms % 1000) / 10);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; border: 2px solid #ffffff; font-size: 13px; font-weight: bold; color: #000000; border-radius: 4px; padding: 0px; } "
                "QPushButton:hover { background: #ffffff; color: %1; }")
                .arg(hc_colors[i]));
            btn->setToolTip(QString::asprintf("Hot Cue %d: %02d:%02d.%02d (Kattintás: Ugrás, Jobb-klikk: Törlés)", i + 1, m, s, cs));
        } else {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #181f2a; border: 1px solid #324155; font-size: 11px; font-weight: bold; color: #5f748f; border-radius: 4px; padding: 0px; } "
                "QPushButton:hover { background: #263447; color: #ffffff; border-color: #4a6280; }"));
            btn->setToolTip(QString::asprintf("Hot Cue %d: Üres (Kattintás: Rögzítés az aktuális pozíciónál)", i + 1));
        }
    }
}

// =============================================================================
// Billentyűzetkezelés (Enter lenyomásakor BPM mentés, dialógus megnyitás blokkolása)
// =============================================================================
void DeckWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if (bpm_edit_ && bpm_edit_->hasFocus()) {
            bool ok = false;
            const double val = bpm_edit_->text().toDouble(&ok);
            if (ok && deck_) {
                deck_->setBpm(val);
            }
            bpm_edit_->clearFocus();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (bpm_edit_ && bpm_edit_->hasFocus()) {
            bpm_edit_->clearFocus();
            event->accept();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

// =============================================================================
// Ablak megjelenítése: Monitor alsó felébe pozicionálás (Dock to Bottom Half)
// =============================================================================
void DeckWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect geom = screen->availableGeometry();
        const int target_w = 640;
        const int target_h = 340;
        setFixedSize(target_w, target_h);

        const int y_pos = std::max(0, geom.bottom() - target_h - 4);
        const bool is_b = (deck_ && deck_->side() == DeckPlayer::DeckSide::B);
        const int x_pos = is_b ? (geom.left() + 1268) : (geom.left() + 12);
        move(x_pos, y_pos);
    }
}

void DeckWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void DeckWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void DeckWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        const QString path = urls.first().toLocalFile();
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            extern void vdj_import_and_load(const QString &file_path, int deck_channel);
            const int channel = (deck_ && deck_->side() == DeckPlayer::DeckSide::B) ? 1 : 0;
            vdj_import_and_load(path, channel);
        }
    }
    event->acceptProposedAction();
}
