#pragma once

#include <QWidget>
#include "engine/lfo_engine.hpp"

class DeckPair;
class QComboBox;
class QSlider;
class QLineEdit;
class QPushButton;
class QTimer;
class LfoScopeWidget;

class LfoTab final : public QWidget {
    Q_OBJECT
public:
    explicit LfoTab(DeckPair *decks, QWidget *parent = nullptr);
    ~LfoTab() override;

    void refresh();
    void setTimerActive(bool active);

private:
    void setupUi();
    QWidget *createLfoSection(int lfo_idx, const QString &title, const QString &color_hex);

    DeckPair *decks_ = nullptr;
    QTimer *refresh_timer_ = nullptr;

    LfoScopeWidget *scope1_ = nullptr;
    LfoScopeWidget *scope2_ = nullptr;
};

void vdj_open_lfo_dialog();
void vdj_free_lfo_dialog();
