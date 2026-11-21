#pragma once

#include <QDialog>

class DeckPair;
class MixerChannelStrip;
class MixerMasterPanel;
class QLabel;
class QPushButton;
class QTimer;

class MixerWindow final : public QDialog {
    Q_OBJECT
public:
    explicit MixerWindow(DeckPair *decks, QWidget *parent = nullptr);
    ~MixerWindow() override;

    void applyWindowPosition();

protected:
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void refreshMixer();

    DeckPair *decks_ = nullptr;

    MixerChannelStrip *channel_a_ = nullptr;
    MixerMasterPanel *master_panel_ = nullptr;
    MixerChannelStrip *channel_b_ = nullptr;

    QLabel *midi_status_lbl_ = nullptr;
    QLabel *bottom_bpm_lbl_ = nullptr;

    QTimer *refresh_timer_ = nullptr;
};

void vdj_open_mixer_window();
void vdj_free_mixer_window();
MixerWindow *vdj_get_mixer_window();
