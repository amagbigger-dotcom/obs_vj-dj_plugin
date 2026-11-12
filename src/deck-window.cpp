#include "deck-window.hpp"
#include "deck-engine.hpp"
#include "waveform-widget.hpp"

#include <QFileDialog>
#include <QInputDialog>
#include <QDateTime>
#include <QSignalBlocker>
#include <QGroupBox>

static QString formatTime(int64_t ms)
{
    if (ms < 0) ms = 0;
    const int total_sec = static_cast<int>(ms / 1000);
    const int mins = total_sec / 60;
    const int secs = total_sec % 60;
    const int millis = static_cast<int>((ms % 1000) / 10);
    return QStringLiteral("%1:%2.%3")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(millis, 2, 10, QChar('0'));
}

DeckWindow::DeckWindow(DeckPlayer *deck, QWidget *parent)
    : QDialog(parent), deck_(deck)
{
    setWindowTitle(QStringLiteral("OBS VJ Deck"));
    setMinimumSize(820, 480);
    resize(860, 520);
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

    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, &DeckWindow::onRefresh);
    refresh_timer_->start(50);
}

DeckWindow::~DeckWindow()
{
    if (refresh_timer_) {
        refresh_timer_->stop();
        delete refresh_timer_;
    }
}

void DeckWindow::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 14, 16, 14);
    main_layout->setSpacing(12);

    // ==========================================
    // 1. TOP BAR: File, Time, BPM
    // ==========================================
    auto *top_bar = new QHBoxLayout();
    top_bar->setSpacing(10);

    auto *open_btn = new QPushButton(QStringLiteral("📂 OPEN MEDIA"), this);
    open_btn->setStyleSheet(QStringLiteral("background: #0088cc; border: 1px solid #00aaff; color: #ffffff; font-size: 12px; padding: 6px 14px;"));
    connect(open_btn, &QPushButton::clicked, this, &DeckWindow::openFileDialog);
    top_bar->addWidget(open_btn);

    file_lbl_ = new QLabel(QStringLiteral("No Track Loaded"), this);
    file_lbl_->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #00d2ff; padding-left: 6px;"));
    top_bar->addWidget(file_lbl_, 1);

    elapsed_lbl_ = new QLabel(QStringLiteral("00:00.00"), this);
    elapsed_lbl_->setStyleSheet(QStringLiteral("font-family: 'Consolas', monospace; font-size: 20px; font-weight: bold; color: #00ffaa; background: #0a0e14; padding: 4px 10px; border: 1px solid #1c2a38; border-radius: 4px;"));
    top_bar->addWidget(elapsed_lbl_);

    remaining_lbl_ = new QLabel(QStringLiteral("-00:00.00"), this);
    remaining_lbl_->setStyleSheet(QStringLiteral("font-family: 'Consolas', monospace; font-size: 20px; font-weight: bold; color: #ffaa00; background: #0a0e14; padding: 4px 10px; border: 1px solid #1c2a38; border-radius: 4px;"));
    top_bar->addWidget(remaining_lbl_);

    bpm_lbl_ = new QLabel(QStringLiteral("128.00 BPM"), this);
    bpm_lbl_->setStyleSheet(QStringLiteral("font-family: 'Consolas', monospace; font-size: 16px; font-weight: bold; color: #ffffff; background: #1a222e; padding: 4px 8px; border: 1px solid #2e3e54; border-radius: 4px;"));
    top_bar->addWidget(bpm_lbl_);

    edit_bpm_btn_ = new QPushButton(QStringLiteral("EDIT"), this);
    edit_bpm_btn_->setFixedWidth(45);
    connect(edit_bpm_btn_, &QPushButton::clicked, this, &DeckWindow::editBpmDialog);
    top_bar->addWidget(edit_bpm_btn_);

    main_layout->addLayout(top_bar);

    // ==========================================
    // 2. WAVEFORM DISPLAY
    // ==========================================
    waveform_widget_ = new WaveformWidget(deck_, true, this);
    waveform_widget_->setFixedHeight(140);
    main_layout->addWidget(waveform_widget_);

    // ==========================================
    // 3. BEATGRID & MOVE CONTROLS
    // ==========================================
    auto *move_bar = new QHBoxLayout();
    move_bar->setSpacing(8);

    auto *grid_lbl = new QLabel(QStringLiteral("BEATGRID:"), this);
    grid_lbl->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4;"));
    move_bar->addWidget(grid_lbl);

    grid_nudge_minus_ = new QPushButton(QStringLiteral("<< 10ms"), this);
    connect(grid_nudge_minus_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->nudgeGrid(-10);
    });
    move_bar->addWidget(grid_nudge_minus_);

    grid_set1_btn_ = new QPushButton(QStringLiteral("SET 1 (DOWNBEAT)"), this);
    grid_set1_btn_->setStyleSheet(QStringLiteral("background: #1c3248; border-color: #00aaff; color: #00d2ff;"));
    connect(grid_set1_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->setGridAnchorAtCurrent();
    });
    move_bar->addWidget(grid_set1_btn_);

    grid_nudge_plus_ = new QPushButton(QStringLiteral("10ms >>"), this);
    connect(grid_nudge_plus_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->nudgeGrid(10);
    });
    move_bar->addWidget(grid_nudge_plus_);

    grid_offset_lbl_ = new QLabel(QStringLiteral("GRID: 0ms"), this);
    grid_offset_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-family: monospace; font-weight: bold; margin-left: 6px;"));
    move_bar->addWidget(grid_offset_lbl_);

    move_bar->addStretch(1);

    tap_bpm_btn_ = new QPushButton(QStringLiteral("TAP BPM"), this);
    connect(tap_bpm_btn_, &QPushButton::clicked, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        tap_times_.push_back(now);
        while (tap_times_.size() > 8) tap_times_.erase(tap_times_.begin());
        if (tap_times_.size() >= 3) {
            double total_diff = 0;
            for (size_t i = 1; i < tap_times_.size(); i++) {
                total_diff += (tap_times_[i] - tap_times_[i - 1]);
            }
            const double avg_ms = total_diff / (tap_times_.size() - 1);
            if (avg_ms > 100.0) {
                const double bpm = 60000.0 / avg_ms;
                if (deck_) deck_->setBpm(bpm);
            }
        }
    });
    move_bar->addWidget(tap_bpm_btn_);

    main_layout->addLayout(move_bar);

    // ==========================================
    // 4. AUTO-LOOP MATRIX
    // ==========================================
    auto *loop_bar = new QHBoxLayout();
    loop_bar->setSpacing(6);

    auto *loop_hdr = new QLabel(QStringLiteral("AUTO-LOOP:"), this);
    loop_hdr->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4;"));
    loop_bar->addWidget(loop_hdr);

    const QStringList loop_labels = {"1/2", "1", "2", "4", "8", "16", "32"};
    for (int i = 0; i < loop_labels.size(); i++) {
        auto *btn = new QPushButton(loop_labels[i], this);
        btn->setFixedWidth(46);
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
    loop_bar->addStretch(1);
    main_layout->addLayout(loop_bar);

    // ==========================================
    // 5. TRANSPORT (CUE, PLAY, SYNC)
    // ==========================================
    auto *transport_bar = new QHBoxLayout();
    transport_bar->setSpacing(10);

    cue_btn_ = new QPushButton(QStringLiteral("CUE"), this);
    cue_btn_->setMinimumSize(90, 48);
    cue_btn_->setStyleSheet(QStringLiteral("background: #2a3444; border: 2px solid #586884; font-size: 14px; font-weight: bold; color: #ffffff; border-radius: 6px;"));
    connect(cue_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->cue();
    });
    transport_bar->addWidget(cue_btn_);

    play_btn_ = new QPushButton(QStringLiteral("▶ PLAY"), this);
    play_btn_->setMinimumSize(120, 48);
    play_btn_->setStyleSheet(QStringLiteral("background: #1e3a24; border: 2px solid #00cc44; font-size: 15px; font-weight: bold; color: #ffffff; border-radius: 6px;"));
    connect(play_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->togglePlayback();
    });
    transport_bar->addWidget(play_btn_);

    sync_btn_ = new QPushButton(QStringLiteral("SYNC"), this);
    sync_btn_->setMinimumSize(80, 48);
    sync_btn_->setCheckable(true);
    sync_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2634; border: 2px solid #385070; font-size: 13px; font-weight: bold; color: #78a0d0; border-radius: 6px; }"
        "QPushButton:checked { background: #005588; border-color: #00d2ff; color: #ffffff; }"
    ));
    connect(sync_btn_, &QPushButton::toggled, this, [this](bool checked) {
        if (deck_) deck_->setSync(checked);
    });
    transport_bar->addWidget(sync_btn_);

    transport_bar->addSpacing(20);

    // Volume
    auto *vol_lbl = new QLabel(QStringLiteral("VOL:"), this);
    vol_lbl->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4;"));
    transport_bar->addWidget(vol_lbl);

    volume_slider_ = new QSlider(Qt::Horizontal, this);
    volume_slider_->setRange(0, 100);
    volume_slider_->setValue(100);
    volume_slider_->setFixedWidth(140);
    connect(volume_slider_, &QSlider::valueChanged, this, [this](int val) {
        if (deck_) deck_->setVolume(static_cast<float>(val) / 100.0f);
    });
    transport_bar->addWidget(volume_slider_);

    transport_bar->addStretch(1);
    main_layout->addLayout(transport_bar);

    // ==========================================
    // 6. TEMPO / PITCH FADER
    // ==========================================
    auto *tempo_bar = new QHBoxLayout();
    tempo_bar->setSpacing(8);

    auto *tempo_hdr = new QLabel(QStringLiteral("TEMPO:"), this);
    tempo_hdr->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4;"));
    tempo_bar->addWidget(tempo_hdr);

    bend_minus_btn_ = new QPushButton(QStringLiteral("−"), this);
    bend_minus_btn_->setFixedSize(30, 28);
    connect(bend_minus_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->applySpeedSettings(deck_->speed() - 1);
    });
    tempo_bar->addWidget(bend_minus_btn_);

    pitch_slider_ = new QSlider(Qt::Horizontal, this);
    pitch_slider_->setRange(50, 150);
    pitch_slider_->setValue(100);
    pitch_slider_->setTracking(true);
    connect(pitch_slider_, &QSlider::valueChanged, this, [this](int val) {
        if (deck_) {
            if (deck_->isSync()) deck_->setSync(false);
            deck_->setSpeed(val);
        }
        const double diff = val - 100;
        pitch_val_lbl_->setText(QStringLiteral("%1% (%2%3%)")
            .arg(val)
            .arg(diff >= 0 ? "+" : "")
            .arg(diff, 0, 'f', 1));
    });
    connect(pitch_slider_, &QSlider::sliderReleased, this, [this]() {
        if (deck_ && pitch_slider_) {
            deck_->applySpeedSettings(pitch_slider_->value());
        }
    });
    tempo_bar->addWidget(pitch_slider_, 1);

    bend_plus_btn_ = new QPushButton(QStringLiteral("+"), this);
    bend_plus_btn_->setFixedSize(30, 28);
    connect(bend_plus_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->applySpeedSettings(deck_->speed() + 1);
    });
    tempo_bar->addWidget(bend_plus_btn_);

    pitch_val_lbl_ = new QLabel(QStringLiteral("100% (+0.0%)"), this);
    pitch_val_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-family: monospace; font-weight: bold; width: 110px;"));
    pitch_val_lbl_->setFixedWidth(110);
    tempo_bar->addWidget(pitch_val_lbl_);

    pitch_reset_btn_ = new QPushButton(QStringLiteral("0% RESET"), this);
    pitch_reset_btn_->setStyleSheet(QStringLiteral("background: #2a3444; border-color: #48566e; color: #cfd6e4; font-size: 10px;"));
    connect(pitch_reset_btn_, &QPushButton::clicked, this, [this]() {
        if (pitch_slider_) pitch_slider_->setValue(100);
        if (deck_) deck_->applySpeedSettings(100);
    });
    tempo_bar->addWidget(pitch_reset_btn_);

    main_layout->addLayout(tempo_bar);
}

void DeckWindow::openFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Video or Audio Media"),
        QString(),
        QStringLiteral("Media Files (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav *.flac *.aac);;All Files (*.*)")
    );

    if (!path.isEmpty() && deck_) {
        deck_->loadFile(path);
    }
}

void DeckWindow::editBpmDialog()
{
    if (!deck_) return;
    bool ok = false;
    const double cur_bpm = deck_->bpm();
    const double new_bpm = QInputDialog::getDouble(
        this,
        QStringLiteral("Edit Deck BPM"),
        QStringLiteral("BPM:"),
        cur_bpm,
        40.0,
        300.0,
        2,
        &ok
    );

    if (ok) {
        deck_->setBpm(new_bpm);
    }
}

void DeckWindow::onRefresh()
{
    if (!deck_) return;

    // Track filename
    const QString fn = deck_->fileName();
    if (!fn.isEmpty()) {
        if (file_lbl_->text() != fn) {
            file_lbl_->setText(fn);
        }
    } else {
        file_lbl_->setText(QStringLiteral("No Track Loaded"));
    }

    // Timecodes
    const int64_t cur_ms = deck_->currentTimeMs();
    const int64_t dur_ms = deck_->durationMs();
    const int64_t rem_ms = std::max<int64_t>(0, dur_ms - cur_ms);

    elapsed_lbl_->setText(formatTime(cur_ms));
    remaining_lbl_->setText(QStringLiteral("-%1").arg(formatTime(rem_ms)));

    // BPM
    const double eff_bpm = deck_->effectiveBpm();
    bpm_lbl_->setText(QStringLiteral("%1 BPM").arg(eff_bpm, 0, 'f', 2));

    // Beatgrid offset
    const int64_t offset = deck_->gridOffsetMs();
    grid_offset_lbl_->setText(QStringLiteral("GRID: %1%2ms")
        .arg(offset >= 0 ? "+" : "")
        .arg(offset));

    // Play button state
    const bool is_playing = deck_->isPlaying();
    if (is_playing) {
        play_btn_->setText(QStringLiteral("⏸ PAUSE"));
        play_btn_->setStyleSheet(QStringLiteral("background: #00aa44; border: 2px solid #00ff55; font-size: 15px; font-weight: bold; color: #ffffff; border-radius: 6px;"));
    } else {
        play_btn_->setText(QStringLiteral("▶ PLAY"));
        play_btn_->setStyleSheet(QStringLiteral("background: #1e3a24; border: 2px solid #00cc44; font-size: 15px; font-weight: bold; color: #ffffff; border-radius: 6px;"));
    }

    // Sync button state
    if (sync_btn_->isChecked() != deck_->isSync()) {
        QSignalBlocker b(sync_btn_);
        sync_btn_->setChecked(deck_->isSync());
    }

    // Loop buttons highlight
    const bool looping = deck_->isLooping();
    const int loop_idx = deck_->loopBeatIndex();
    for (size_t i = 0; i < loop_btns_.size(); i++) {
        if (looping && static_cast<int>(i) == loop_idx) {
            loop_btns_[i]->setStyleSheet(QStringLiteral("background: #ff8800; border: 2px solid #ffaa00; color: #000000; font-weight: bold; border-radius: 4px;"));
        } else {
            loop_btns_[i]->setStyleSheet(QStringLiteral("background: #222834; border: 1px solid #384255; color: #ffffff; font-weight: bold; border-radius: 4px;"));
        }
    }

    // Pitch slider synchronization (if user isn't dragging it)
    if (pitch_slider_ && !pitch_slider_->isSliderDown()) {
        const int spd = deck_->speed();
        if (pitch_slider_->value() != spd) {
            QSignalBlocker b(pitch_slider_);
            pitch_slider_->setValue(spd);
            const double diff = spd - 100;
            pitch_val_lbl_->setText(QStringLiteral("%1% (%2%3%)")
                .arg(spd)
                .arg(diff >= 0 ? "+" : "")
                .arg(diff, 0, 'f', 1));
        }
    }
}
