#pragma once

#include <QDialog>

class DeckPair;
class DeckPlayer;
class QPushButton;
class QLabel;
class QComboBox;
class QWidget;
class QVBoxLayout;

class VjFxDialog final : public QDialog {
    Q_OBJECT
public:
    explicit VjFxDialog(DeckPair *decks, QWidget *parent = nullptr);
    ~VjFxDialog() override;

    void selectDeck(int deck_idx);

private:
    void rebuildParamControls();
    void updateColorUi();
    DeckPlayer *currentDeck() const;

    DeckPair *decks_ = nullptr;
    int current_deck_idx_ = 0; // 0 = A, 1 = B

    QPushButton *deck_a_btn_ = nullptr;
    QPushButton *deck_b_btn_ = nullptr;
    QLabel *track_title_lbl_ = nullptr;
    QComboBox *shader_combo_ = nullptr;
    QPushButton *color_btn_ = nullptr;
    QComboBox *color_mod_combo_ = nullptr;
    QWidget *params_container_ = nullptr;
    QVBoxLayout *params_layout_ = nullptr;
};

void vdj_open_vj_fx_dialog(int target_deck = 0);
void vdj_free_vj_fx_dialog();
