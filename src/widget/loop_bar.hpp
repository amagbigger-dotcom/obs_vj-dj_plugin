#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <vector>

class DeckPlayer;

class LoopBar : public QWidget {
    Q_OBJECT
public:
    explicit LoopBar(DeckPlayer *deck = nullptr, QWidget *parent = nullptr);

    void bindDeck(DeckPlayer *deck);
    void refresh();

private:
    DeckPlayer *deck_ = nullptr;
    QLabel *loop_hdr_ = nullptr;
    std::vector<QPushButton *> loop_btns_;

    bool last_looping_ = false;
    int last_loop_idx_ = -1;
};
