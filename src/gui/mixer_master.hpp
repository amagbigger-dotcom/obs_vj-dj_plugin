#pragma once

#include <QGroupBox>
#include <chrono>
#include <vector>

class DeckPair;
class DJSlider;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QKeyEvent;
class CrossfadeFxDialog;
class LfoDialog;

class MixerMasterPanel final : public QGroupBox {
    Q_OBJECT
public:
    explicit MixerMasterPanel(DeckPair *decks, QWidget *parent = nullptr);
    ~MixerMasterPanel() override = default;

    void refresh();
    bool handleKeyPress(QKeyEvent *event);

private:
    void setupUi();
    void commitMasterBpm();
    void updateCfLabel(int val);

    DeckPair *decks_ = nullptr;

    QPushButton *master_clock_btn_ = nullptr;
    QPushButton *master_clock_ext_btn_ = nullptr;
    QPushButton *master_clock_send_btn_ = nullptr;
    QLineEdit *master_clock_bpm_edit_ = nullptr;
    QPushButton *master_clock_tap_btn_ = nullptr;
    QPushButton *master_clock_minus_btn_ = nullptr;
    QPushButton *master_clock_plus_btn_ = nullptr;
    std::vector<std::chrono::steady_clock::time_point> master_tap_times_;

    DJSlider *crossfader_ = nullptr;
    QLabel *cf_val_lbl_ = nullptr;
    QComboBox *cf_fx_combo_ = nullptr;
    QPushButton *cf_fx_settings_btn_ = nullptr;
    QPushButton *lfo_btn_ = nullptr;
    QPushButton *scale_btn_ = nullptr;
    CrossfadeFxDialog *cf_fx_dialog_ = nullptr;
    LfoDialog *lfo_dialog_ = nullptr;

    DJSlider *hp_vol_slider_ = nullptr;
    QLabel *hp_vol_val_lbl_ = nullptr;
    DJSlider *hp_mix_slider_ = nullptr;
    QLabel *hp_mix_val_lbl_ = nullptr;
};
