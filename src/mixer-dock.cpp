#include "mixer-dock.hpp"
#include "deck-engine.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

static const char *DARK_DJ_STYLESHEET = R"(
QDialog {
    background-color: #121316;
    color: #e0e0e0;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 13px;
}
QGroupBox {
    background-color: #1a1c22;
    border: 1px solid #2d313b;
    border-radius: 8px;
    margin-top: 14px;
    padding-top: 10px;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top center;
    padding: 2px 12px;
    border-radius: 4px;
}
QGroupBox#deck_a {
    border: 1px solid #00a8cc;
}
QGroupBox#deck_a::title {
    background-color: #007791;
    color: #ffffff;
}
QGroupBox#deck_b {
    border: 1px solid #e85d04;
}
QGroupBox#deck_b::title {
    background-color: #d00000;
    color: #ffffff;
}
QPushButton {
    background-color: #2b2e38;
    color: #ffffff;
    border: 1px solid #3c4250;
    border-radius: 5px;
    padding: 7px 14px;
    font-weight: 600;
}
QPushButton:hover {
    background-color: #383d4a;
    border-color: #555c6e;
}
QPushButton:pressed {
    background-color: #1f2229;
}
QPushButton:checked {
    background-color: #007791;
    border-color: #00a8cc;
}
QPushButton#play_btn {
    background-color: #1e3a2f;
    border: 1px solid #2ecc71;
    color: #2ecc71;
    font-size: 14px;
}
QPushButton#play_btn:hover {
    background-color: #27ae60;
    color: #ffffff;
}
QComboBox {
    background-color: #242730;
    color: #ffffff;
    border: 1px solid #3c4250;
    border-radius: 5px;
    padding: 5px 10px;
}
QComboBox QAbstractItemView {
    background-color: #1a1c22;
    color: #ffffff;
    selection-background-color: #007791;
}
QSlider::groove:horizontal {
    height: 6px;
    background: #252830;
    border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: #00a8cc;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #ffffff;
    border: 1px solid #999999;
    width: 16px;
    margin-top: -5px;
    margin-bottom: -5px;
    border-radius: 8px;
}
QSlider::handle:horizontal:hover {
    background: #00d2ff;
}
QLabel {
    color: #b0b4be;
}
QLabel#title_lbl {
    font-size: 16px;
    font-weight: bold;
    color: #ffffff;
}
)";

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("OBS Video DJ Mixer"));
    resize(980, 520);
    setStyleSheet(DARK_DJ_STYLESHEET);

    auto *root_layout = new QHBoxLayout(this);
    root_layout->setContentsMargins(15, 15, 15, 15);
    root_layout->setSpacing(15);

    auto *deck_a_group = new QGroupBox(QStringLiteral("DECK A"), this);
    deck_a_group->setObjectName(QStringLiteral("deck_a"));

    auto *mixer_group = new QGroupBox(QStringLiteral("MASTER & CROSSFADER"), this);
    mixer_group->setObjectName(QStringLiteral("mixer_master"));

    auto *deck_b_group = new QGroupBox(QStringLiteral("DECK B"), this);
    deck_b_group->setObjectName(QStringLiteral("deck_b"));

    buildDeckPanel(deck_a_group, true);
    buildDeckPanel(deck_b_group, false);

    // Center Mixer Layout
    auto *mixer_layout = new QVBoxLayout(mixer_group);
    mixer_layout->setContentsMargins(15, 20, 15, 15);
    mixer_layout->setSpacing(12);

    auto *trans_lbl = new QLabel(QStringLiteral("Transition Effect"), mixer_group);
    trans_lbl->setAlignment(Qt::AlignCenter);
    mixer_layout->addWidget(trans_lbl);

    auto *transition = new QComboBox(mixer_group);
    transition->addItem(QStringLiteral("Linear Fade"));
    transition->addItem(QStringLiteral("Horizontal Wipe"));
    transition->addItem(QStringLiteral("Additive Blend"));
    transition->addItem(QStringLiteral("Hard Cut"));
