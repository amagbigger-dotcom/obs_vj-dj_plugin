#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QHBoxLayout>

class DeckPlayer;

class TempoControl : public QWidget {
    Q_OBJECT
public:
    explicit TempoControl(DeckPlayer *deck = nullptr, QWidget *parent = nullptr);
    ~TempoControl() override;

    void bindDeck(DeckPlayer *deck);
    void applyTheme(bool is_a);
    void refresh();

private:
    void updateLabelsAndSlider();
    void updateKeyLockButton();

    DeckPlayer *deck_ = nullptr;
    QTimer *pitch_throttle_timer_ = nullptr;
    int pending_throttle_speed_ = 100;

    QLabel *tempo_hdr_ = nullptr;
    QPushButton *bend_minus_btn_ = nullptr;
    QSlider *pitch_slider_ = nullptr;
    QPushButton *bend_plus_btn_ = nullptr;
    QLabel *pitch_val_lbl_ = nullptr;
    QPushButton *key_lock_btn_ = nullptr;
    QPushButton *pitch_reset_btn_ = nullptr;
};
