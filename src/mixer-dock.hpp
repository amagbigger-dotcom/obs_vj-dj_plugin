#pragma once

#include <QDialog>

class DeckPair;
class QSlider;
class QLabel;
class QPushButton;
class QComboBox;

class VideoPreviewWidget;

class MixerWindow final : public QDialog {
    Q_OBJECT

public:
    explicit MixerWindow(DeckPair *decks, QWidget *parent = nullptr);
    ~MixerWindow() override = default;

private:
    void buildDeckPanel(QWidget *parent, bool is_deck_a);
    void buildMixerSection(QWidget *parent);
    void refreshDeck(bool is_deck_a);
    void refreshMixer();

    static QString formatTime(qint64 ms);

    DeckPair *decks_ = nullptr;

    QSlider *seek_a_ = nullptr;
    QSlider *seek_b_ = nullptr;
    QLabel *time_a_ = nullptr;
    QLabel *time_b_ = nullptr;
    QPushButton *play_btn_a_ = nullptr;
    QPushButton *play_btn_b_ = nullptr;
    QLabel *rate_lbl_a_ = nullptr;
    QLabel *rate_lbl_b_ = nullptr;
    QSlider *pitch_slider_a_ = nullptr;
    QSlider *pitch_slider_b_ = nullptr;
    QComboBox *loop_combo_a_ = nullptr;
    QComboBox *loop_combo_b_ = nullptr;

    QSlider *hi_a_ = nullptr;
    QSlider *mid_a_ = nullptr;
    QSlider *low_a_ = nullptr;
    QSlider *flt_a_ = nullptr;
    QSlider *vol_a_ = nullptr;

    QSlider *hi_b_ = nullptr;
    QSlider *mid_b_ = nullptr;
    QSlider *low_b_ = nullptr;
    QSlider *flt_b_ = nullptr;
    QSlider *vol_b_ = nullptr;

    QSlider *crossfader_ = nullptr;
    QLabel *midi_status_lbl_ = nullptr;
};
