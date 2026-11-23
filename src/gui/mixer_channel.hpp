#pragma once

#include <QGroupBox>

class DeckPlayer;
class DJSlider;
class QLabel;
class QPushButton;
class QProgressBar;
class QComboBox;

class MixerChannelStrip final : public QGroupBox {
    Q_OBJECT
public:
    explicit MixerChannelStrip(bool is_deck_a, DeckPlayer *deck, QWidget *parent = nullptr);
    ~MixerChannelStrip() override = default;

    void refresh();
    void bindDeck(DeckPlayer *deck);

private:
    void setupUi();
    void updateSliderStyles(bool inverted);
    void showSlotMenu(int slot, const QPoint &pos);
    void updateSlotBtn(int slot);

    bool is_deck_a_ = true;
    DeckPlayer *deck_ = nullptr;

    DJSlider *gain_slider_ = nullptr;
    DJSlider *hi_slider_ = nullptr;
    DJSlider *mid_slider_ = nullptr;
    DJSlider *low_slider_ = nullptr;
    DJSlider *flt_slider_ = nullptr;
    DJSlider *vol_slider_ = nullptr;

    QLabel *gain_val_lbl_ = nullptr;
    QLabel *hi_val_lbl_ = nullptr;
    QLabel *mid_val_lbl_ = nullptr;
    QLabel *low_val_lbl_ = nullptr;
    QLabel *flt_val_lbl_ = nullptr;
    QLabel *vol_val_lbl_ = nullptr;

    QLabel *hi_title_lbl_ = nullptr;
    QLabel *low_title_lbl_ = nullptr;
    QLabel *flt_title_lbl_ = nullptr;

    QPushButton *slot_btn_[4] = {nullptr, nullptr, nullptr, nullptr};

    QProgressBar *meter_ = nullptr;
    QPushButton *hp_cue_btn_ = nullptr;
    QPushButton *viz_eq_btn_ = nullptr;
    QPushButton *rgb_inv_btn_ = nullptr;
    QComboBox *vol_fx_combo_ = nullptr;
};
