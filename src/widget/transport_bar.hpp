#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>

class DeckPlayer;

class TransportBar : public QWidget {
    Q_OBJECT
public:
    explicit TransportBar(DeckPlayer *deck = nullptr, QWidget *parent = nullptr);

    void bindDeck(DeckPlayer *deck);
    void refresh();

private:
    void updateHotCueButtons();

    DeckPlayer *deck_ = nullptr;
    QPushButton *cue_btn_ = nullptr;
    QPushButton *play_btn_ = nullptr;
    QPushButton *sync_btn_ = nullptr;
    QPushButton *master_btn_ = nullptr;
    QLabel *hc_hdr_ = nullptr;
    QPushButton *hot_cue_btns_[4] = {nullptr, nullptr, nullptr, nullptr};
};
