#pragma once

#include <QDialog>

class DeckPair;
class QLabel;
class QSlider;
class QComboBox;
class QPushButton;

class CrossfadeFxDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CrossfadeFxDialog(DeckPair *decks, QWidget *parent = nullptr);
    ~CrossfadeFxDialog() override = default;

    void refresh();

private:
    void setupUi();
    void updateFromDecks();

    DeckPair *decks_ = nullptr;

    QLabel *fx_title_lbl_ = nullptr;
    QLabel *intensity_val_lbl_ = nullptr;
    QSlider *intensity_slider_ = nullptr;
    QComboBox *intensity_mod_combo_ = nullptr;

    QLabel *rate_val_lbl_ = nullptr;
    QSlider *rate_slider_ = nullptr;
    QComboBox *rate_mod_combo_ = nullptr;

    QPushButton *reset_btn_ = nullptr;
};
