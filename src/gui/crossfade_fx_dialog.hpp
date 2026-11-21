#pragma once

#include <QWidget>
#include <vector>

class DeckPair;
class QLabel;
class QSlider;
class QComboBox;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class CrossfadeFxTab final : public QWidget {
    Q_OBJECT
public:
    explicit CrossfadeFxTab(DeckPair *decks, QWidget *parent = nullptr);
    ~CrossfadeFxTab() override = default;

    void refresh();
    void rebuildParamControls();

private:
    void setupUi();

    DeckPair *decks_ = nullptr;
    QComboBox *fx_combo_ = nullptr;
    QScrollArea *scroll_area_ = nullptr;
    QWidget *params_container_ = nullptr;
    QVBoxLayout *params_layout_ = nullptr;
    int last_fx_mode_ = -1;
};

void vdj_open_crossfade_fx_dialog();
void vdj_free_crossfade_fx_dialog();
