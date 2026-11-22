#include "widget/loop_bar.hpp"
#include "engine/deck_engine.hpp"

#include <QStringList>

namespace {
static inline bool isTripletLoop(int idx) noexcept
{
    // 6, 12, 24, 48, 96 beat harmadolos ciklusok
    return (idx == 7 || idx == 9 || idx == 11 || idx == 13 || idx == 15);
}

static inline QString loopButtonStyle(bool is_active, int idx)
{
    if (is_active) {
        return QStringLiteral("background: #ff8800; border: 1px solid #ffaa00; color: #000000; font-weight: bold; border-radius: 3px; padding: 0px; font-size: 8px;");
    }
    if (isTripletLoop(idx)) {
        return QStringLiteral("background: #1c2635; border: 1px solid #3b5375; color: #88b0e8; font-weight: bold; border-radius: 3px; padding: 0px; font-size: 8px;");
    }
    return QStringLiteral("background: #222834; border: 1px solid #384255; color: #ffffff; font-weight: bold; border-radius: 3px; padding: 0px; font-size: 8px;");
}
} // namespace

LoopBar::LoopBar(DeckPlayer *deck, QWidget *parent)
    : QWidget(parent), deck_(deck)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    loop_hdr_ = new QLabel(QStringLiteral("LOOP:"), this);
    loop_hdr_->setStyleSheet(QStringLiteral("font-weight: bold; color: #7f8fa4; font-size: 9px;"));
    layout->addWidget(loop_hdr_);

    const QStringList loop_labels = {
        "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "6", "8", "12", "16", "24", "32", "48", "64", "96", "128"
    };
    for (int i = 0; i < loop_labels.size(); i++) {
        auto *btn = new QPushButton(loop_labels[i], this);
        btn->setFixedSize(30, 22);
        btn->setAutoDefault(false);
        btn->setDefault(false);
        btn->setStyleSheet(loopButtonStyle(false, i));
        if (isTripletLoop(i)) {
            btn->setToolTip(QString::asprintf("Harmadolós (Triola) loop: %s ütem", loop_labels[i].toUtf8().constData()));
        } else {
            btn->setToolTip(QString::asprintf("Loop méret: %s ütem", loop_labels[i].toUtf8().constData()));
        }
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
            const bool is_active = (looping && static_cast<int>(i) == loop_idx);
            loop_btns_[i]->setStyleSheet(loopButtonStyle(is_active, static_cast<int>(i)));
        }
    }
}
