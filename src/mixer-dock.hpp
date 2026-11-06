#pragma once

#include <QDockWidget>

class DeckPair;
class QSlider;

class MixerDock final : public QDockWidget {
    Q_OBJECT

public:
    explicit MixerDock(DeckPair *decks, QWidget *parent = nullptr);

private:
    void buildDeckPanel(QWidget *parent, bool deck_a);
    void refreshDeck(bool deck_a);

    DeckPair *decks_ = nullptr;
    QSlider *seek_a_ = nullptr;
    QSlider *seek_b_ = nullptr;
};
