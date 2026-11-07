#pragma once

#include <QDialog>

class DeckPair;
class QSlider;

class MixerWindow final : public QDialog {
    Q_OBJECT

public:
    explicit MixerWindow(DeckPair *decks, QWidget *parent = nullptr);

private:
    void buildDeckPanel(QWidget *parent, bool deck_a);
    void refreshDeck(bool deck_a);

    DeckPair *decks_ = nullptr;
    QSlider *seek_a_ = nullptr;
    QSlider *seek_b_ = nullptr;
};
