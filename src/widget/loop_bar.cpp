#include "widget/loop_bar.hpp"
#include "engine/deck_engine.hpp"

#include <QStringList>

LoopBar::LoopBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    loop_hdr_ = new QLabel(QStringLiteral("LOOP:"), this);
    loop_hdr_->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px;"));
    layout->addWidget(loop_hdr_);

    const QStringList loop_labels = {
        "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16", "32", "64", "128"
    };
    for (int i = 0; i < loop_labels.size(); i++) {
        auto *btn = new QPushButton(loop_labels[i], this);
        btn->setFixedSize(30, 22);
        btn->setAutoDefault(false);
        btn->setDefault(false);
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
        layout->addWidget(btn);
    }

    layout->addSpacing(4);

    jog_hdr_ = new QLabel(QStringLiteral("JOG:"), this);
    jog_hdr_->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px;"));
    layout->addWidget(jog_hdr_);

    jog_minus_btn_ = new QPushButton(QStringLiteral("◀ NUDGE"), this);
    jog_minus_btn_->setFixedSize(48, 22);
    jog_minus_btn_->setAutoDefault(false);
    jog_minus_btn_->setDefault(false);
    jog_minus_btn_->setAutoRepeat(true);
    jog_minus_btn_->setAutoRepeatDelay(250);
    jog_minus_btn_->setAutoRepeatInterval(80);
    jog_minus_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2634; border: 1px solid #385070; font-size: 8px; font-weight: bold; color: #78a0d0; border-radius: 3px; padding: 0px; } "
        "QPushButton:hover { background: #26384e; color: #fff; } "
        "QPushButton:pressed { background: #0088cc; color: #fff; }"));
    jog_minus_btn_->setToolTip(QStringLiteral("Jog Nudge: lemezt lassítja / visszatartja 5ms-mal"));
    connect(jog_minus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        deck_->nudgePlayback(-5);
    });
    layout->addWidget(jog_minus_btn_);

    jog_plus_btn_ = new QPushButton(QStringLiteral("NUDGE ▶"), this);
    jog_plus_btn_->setFixedSize(48, 22);
    jog_plus_btn_->setAutoDefault(false);
    jog_plus_btn_->setDefault(false);
    jog_plus_btn_->setAutoRepeat(true);
    jog_plus_btn_->setAutoRepeatDelay(250);
    jog_plus_btn_->setAutoRepeatInterval(80);
    jog_plus_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2634; border: 1px solid #385070; font-size: 8px; font-weight: bold; color: #78a0d0; border-radius: 3px; padding: 0px; } "
        "QPushButton:hover { background: #26384e; color: #fff; } "
        "QPushButton:pressed { background: #0088cc; color: #fff; }"));
    jog_plus_btn_->setToolTip(QStringLiteral("Jog Nudge: lemezt gyorsítja / megtolja 5ms-mal"));
    connect(jog_plus_btn_, &QPushButton::clicked, this, [this]() {
        if (!deck_) return;
        deck_->nudgePlayback(+5);
    });
    layout->addWidget(jog_plus_btn_);

    layout->addStretch(1);
}

void LoopBar::bindDeck(DeckPlayer *deck)
{
    deck_ = deck;
    last_looping_ = false;
    last_loop_idx_ = -1;
}

void LoopBar::refresh()
{
    if (!deck_) return;

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
}
