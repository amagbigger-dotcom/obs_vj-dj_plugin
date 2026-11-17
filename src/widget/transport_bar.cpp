#include "widget/transport_bar.hpp"
#include "engine/deck_engine.hpp"

#include <QSignalBlocker>

TransportBar::TransportBar(DeckPlayer *deck, QWidget *parent)
    : QWidget(parent), deck_(deck)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    cue_btn_ = new QPushButton(QStringLiteral("CUE"), this);
    cue_btn_->setFixedSize(65, 34);
    cue_btn_->setAutoDefault(false);
    cue_btn_->setDefault(false);
    cue_btn_->setStyleSheet(QStringLiteral("background: #2a3444; border: 2px solid #586884; font-size: 12px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    connect(cue_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->cue();
    });
    layout->addWidget(cue_btn_);

    play_btn_ = new QPushButton(QStringLiteral("▶ PLAY"), this);
    play_btn_->setFixedSize(90, 34);
    play_btn_->setAutoDefault(false);
    play_btn_->setDefault(false);
    play_btn_->setStyleSheet(QStringLiteral("background: #1e3a24; border: 2px solid #00cc44; font-size: 13px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    connect(play_btn_, &QPushButton::clicked, this, [this]() {
        if (deck_) deck_->togglePlayback();
    });
    layout->addWidget(play_btn_);

    sync_btn_ = new QPushButton(QStringLiteral("SYNC"), this);
    sync_btn_->setFixedSize(65, 34);
    sync_btn_->setAutoDefault(false);
    sync_btn_->setDefault(false);
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
    layout->addWidget(sync_btn_);

    master_btn_ = new QPushButton(QStringLiteral("MASTER"), this);
    master_btn_->setFixedSize(65, 34);
    master_btn_->setAutoDefault(false);
    master_btn_->setDefault(false);
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
    layout->addWidget(master_btn_);

    hc_hdr_ = new QLabel(QStringLiteral("HOT CUE:"), this);
    hc_hdr_->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px; margin-left: 6px;"));
    layout->addWidget(hc_hdr_);

    for (int i = 0; i < 4; ++i) {
        auto *btn = new QPushButton(QString::asprintf("%d", i + 1), this);
        btn->setFixedSize(30, 34);
        btn->setAutoDefault(false);
        btn->setDefault(false);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        hot_cue_btns_[i] = btn;
        layout->addWidget(btn);

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
    layout->addStretch(1);
}

void TransportBar::bindDeck(DeckPlayer *deck)
{
    deck_ = deck;
    updateHotCueButtons();
}

void TransportBar::refresh()
{
    if (!deck_) return;

    // Play button state
    const bool is_playing = deck_->isPlaying();
    if (is_playing) {
        play_btn_->setText(QStringLiteral("⏸ PAUSE"));
        play_btn_->setStyleSheet(QStringLiteral("background: #00aa44; border: 2px solid #00ff55; font-size: 13px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    } else {
        play_btn_->setText(QStringLiteral("▶ PLAY"));
        play_btn_->setStyleSheet(QStringLiteral("background: #1e3a24; border: 2px solid #00cc44; font-size: 13px; font-weight: bold; color: #ffffff; border-radius: 4px; padding: 0px;"));
    }

    // Master & Sync button states
    const bool is_m = deck_->isMaster();
    if (master_btn_->isChecked() != is_m) {
        QSignalBlocker b(master_btn_);
        master_btn_->setChecked(is_m);
    }
    if (sync_btn_->isChecked() != deck_->isSync()) {
        QSignalBlocker b(sync_btn_);
        sync_btn_->setChecked(deck_->isSync());
    }

    // Hot cues
    updateHotCueButtons();
}

void TransportBar::updateHotCueButtons()
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
