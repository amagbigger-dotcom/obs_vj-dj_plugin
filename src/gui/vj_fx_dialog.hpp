#pragma once

#include <QDialog>

class DeckPair;
class DeckPlayer;
class QPushButton;
class QLabel;
class QComboBox;
class QWidget;
class QVBoxLayout;
class QTabWidget;
class CrossfadeFxTab;
class LfoTab;

class VjFxDialog final : public QDialog {
    Q_OBJECT
public:
    explicit VjFxDialog(DeckPair *decks, QWidget *parent = nullptr);
    ~VjFxDialog() override;

    void selectTab(int tab_idx); // 0 = Shader, 1 = Crossfade FX, 2 = LFO
    void selectDeck(int deck_idx); // 0 = A, 1 = B
    void reloadShaderList();
    void refreshCrossfade();
    void refreshLfo();
    void applyWindowPosition();

protected:
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    QWidget *createShaderTabWidget();
    void rebuildParamControls();
    void updateColorUi();
    DeckPlayer *currentDeck() const;

    DeckPair *decks_ = nullptr;
    int current_deck_idx_ = 0; // 0 = A, 1 = B

    QTabWidget *tab_widget_ = nullptr;

    // Tab 0: Shader widgets
    QPushButton *deck_a_btn_ = nullptr;
    QPushButton *deck_b_btn_ = nullptr;
    QLabel *track_title_lbl_ = nullptr;
    QComboBox *shader_combo_ = nullptr;
    QPushButton *color_btn_ = nullptr;
    QComboBox *color_mod_combo_ = nullptr;
    QWidget *params_container_ = nullptr;
    QVBoxLayout *params_layout_ = nullptr;

    // Tab 1: Crossfade FX tab
    CrossfadeFxTab *crossfade_tab_ = nullptr;

    // Tab 2: LFO tab
    LfoTab *lfo_tab_ = nullptr;
};

void vdj_open_vj_fx_dialog(int target_deck = 0);
void vdj_open_vj_fx_dialog_tab(int tab_idx, int target_deck = 0);
void vdj_free_vj_fx_dialog();
VjFxDialog *vdj_get_vj_fx_dialog();
