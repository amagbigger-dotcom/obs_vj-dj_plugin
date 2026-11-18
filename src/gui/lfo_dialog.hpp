#pragma once

#include <QDialog>
#include "engine/lfo_engine.hpp"

class DeckPair;
class QComboBox;
class QSlider;
class QLineEdit;
class QPushButton;
class QTimer;
class LfoScopeWidget;

class LfoDialog final : public QDialog {
    Q_OBJECT
public:
    explicit LfoDialog(DeckPair *decks, QWidget *parent = nullptr);
    ~LfoDialog() override;

    void refresh();

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
