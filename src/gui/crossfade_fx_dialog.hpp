#pragma once

#include <QDialog>
#include <vector>

class DeckPair;
class QLabel;
class QSlider;
class QComboBox;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class CrossfadeFxDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CrossfadeFxDialog(DeckPair *decks, QWidget *parent = nullptr);
    ~CrossfadeFxDialog() override;

    void refresh();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void rebuildParamControls();

    DeckPair *decks_ = nullptr;

    QComboBox *fx_combo_ = nullptr;

    QScrollArea *scroll_area_ = nullptr;
    QWidget *params_container_ = nullptr;
    QVBoxLayout *params_layout_ = nullptr;

    int last_fx_mode_ = -1;
};
