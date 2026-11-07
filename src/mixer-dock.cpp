#include "mixer-dock.hpp"
#include "deck-engine.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(tr("Video DJ Mixer"));
    resize(950, 450);

    auto *root_layout = new QHBoxLayout(this);

    auto *deck_a_panel = new QWidget(this);
    auto *mixer_panel = new QWidget(this);
    auto *deck_b_panel = new QWidget(this);

    buildDeckPanel(deck_a_panel, true);
    buildDeckPanel(deck_b_panel, false);

    auto *mixer_layout = new QVBoxLayout(mixer_panel);
    mixer_layout->addWidget(new QLabel(tr("Transition"), mixer_panel));

    auto *transition = new QComboBox(mixer_panel);
    transition->addItems({
        tr("Linear Fade"),
        tr("Horizontal Wipe"),
        tr("Additive"),
        tr("Hard Cut")
    });
    mixer_layout->addWidget(transition);

    mixer_layout->addWidget(new QLabel(tr("Crossfader"), mixer_panel));

    auto *crossfader = new QSlider(Qt::Horizontal, mixer_panel);
    crossfader->setRange(0, 1000);
    crossfader->setValue(500);
    mixer_layout->addWidget(crossfader);

    connect(
        transition,
        &QComboBox::currentIndexChanged,
        this,
        [this](int index) {
            decks_->setTransitionMode(index);
        });

    connect(
        crossfader,
        &QSlider::valueChanged,
        this,
        [this](int value) {
            decks_->setCrossfader(static_cast<double>(value) / 1000.0);
        });

    root_layout->addWidget(deck_a_panel, 1);
    root_layout->addWidget(mixer_panel);
    root_layout->addWidget(deck_b_panel, 1);

    auto *refresh_timer = new QTimer(this);
    connect(
        refresh_timer,
        &QTimer::timeout,
        this,
        [this]() {
            refreshDeck(true);
            refreshDeck(false);
        });
    refresh_timer->start(200);
}

void MixerWindow::buildDeckPanel(QWidget *parent, bool deck_a)
{
    auto *layout = new QVBoxLayout(parent);
    DeckPlayer *deck = deck_a ? &decks_->deckA() : &decks_->deckB();

    layout->addWidget(
        new QLabel(deck_a ? tr("Deck A") : tr("Deck B"), parent));

    auto *file_label = new QLabel(tr("No media loaded"), parent);
    file_label->setWordWrap(true);
    layout->addWidget(file_label);

    auto *load_button = new QPushButton(tr("Load file…"), parent);
    auto *play_button = new QPushButton(tr("Play / Pause"), parent);
    auto *loop_button = new QPushButton(tr("Loop"), parent);
    loop_button->setCheckable(true);

    layout->addWidget(load_button);
    layout->addWidget(play_button);
    layout->addWidget(loop_button);

    auto *seek_slider = new QSlider(Qt::Horizontal, parent);
    seek_slider->setRange(0, 1000);
    layout->addWidget(new QLabel(tr("Position"), parent));
    layout->addWidget(seek_slider);

    auto add_color_slider =
        [layout, parent, deck](const QString &title,
                               void (DeckPlayer::*setter)(float)) {
            layout->addWidget(new QLabel(title, parent));

            auto *slider = new QSlider(Qt::Horizontal, parent);
            slider->setRange(0, 200);
            slider->setValue(100);
            slider->setToolTip(tr("Range: 0.00–2.00; default: 1.00"));
            layout->addWidget(slider);

            QObject::connect(
                slider,
                &QSlider::valueChanged,
                parent,
                [deck, setter](int value) {
                    const float normalized =
                        static_cast<float>(value) / 100.0f;
                    (deck->*setter)(normalized);
                });
        };

    add_color_slider(tr("EQ HI → Red"), &DeckPlayer::setRed);
    add_color_slider(tr("EQ MID → Green"), &DeckPlayer::setGreen);
    add_color_slider(tr("EQ LOW → Blue"), &DeckPlayer::setBlue);
    add_color_slider(
        tr("FILTER / MIXER FX → Saturation"),
        &DeckPlayer::setSaturation);

    connect(
        load_button,
        &QPushButton::clicked,
        this,
        [this, deck, file_label]() {
            const QString path = QFileDialog::getOpenFileName(
                this,
                tr("Load video"),
                QString(),
                tr("Video files (*.*)"));

            if (path.isEmpty())
                return;

            deck->loadFile(path);
            file_label->setText(path);
        });

    connect(
        play_button,
        &QPushButton::clicked,
        this,
        [deck]() {
            deck->togglePlayback();
        });

    connect(
        loop_button,
        &QPushButton::toggled,
        this,
        [deck](bool enabled) {
            deck->setLooping(enabled);
        });

    connect(
        seek_slider,
        &QSlider::sliderMoved,
        this,
        [deck](int value) {
            deck->seek(static_cast<double>(value) / 1000.0);
        });

    if (deck_a)
        seek_a_ = seek_slider;
    else
        seek_b_ = seek_slider;
}

void MixerWindow::refreshDeck(bool deck_a)
{
    DeckPlayer &deck = deck_a ? decks_->deckA() : decks_->deckB();
    QSlider *slider = deck_a ? seek_a_ : seek_b_;

    if (!slider || slider->isSliderDown())
        return;

    const QSignalBlocker blocker(slider);
    slider->setValue(static_cast<int>(deck.position() * 1000.0));
}
