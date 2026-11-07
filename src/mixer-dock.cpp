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

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("OBS Video DJ Mixer"));
    resize(900, 480);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #18191c; color: #ffffff; font-size: 13px; }"
        "QGroupBox { background: #202227; border: 1px solid #363940; border-radius: 6px; margin-top: 10px; padding-top: 10px; font-weight: bold; color: #ffffff; }"
        "QPushButton { background: #2f323a; color: #ffffff; border: 1px solid #454954; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background: #3c404b; }"
        "QSlider::groove:horizontal { height: 6px; background: #2d3038; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #0099cc; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #ffffff; width: 14px; margin: -4px 0; border-radius: 7px; }"
    ));

    auto *root_layout = new QHBoxLayout(this);

    auto *deck_a_group = new QGroupBox(QStringLiteral("DECK A"), this);
    auto *mixer_group = new QGroupBox(QStringLiteral("MIXER / TRANSITION"), this);
    auto *deck_b_group = new QGroupBox(QStringLiteral("DECK B"), this);

    buildDeckPanel(deck_a_group, true);
    buildDeckPanel(deck_b_group, false);

    // Keverő sáv
    auto *mixer_layout = new QVBoxLayout(mixer_group);
    mixer_layout->addWidget(new QLabel(QStringLiteral("Transition Effect:"), mixer_group));

    auto *transition = new QComboBox(mixer_group);
    transition->addItem(QStringLiteral("Linear Fade"));
    transition->addItem(QStringLiteral("Horizontal Wipe"));
    transition->addItem(QStringLiteral("Additive Blend"));
    transition->addItem(QStringLiteral("Hard Cut"));
    mixer_layout->addWidget(transition);

    mixer_layout->addSpacing(20);
    mixer_layout->addWidget(new QLabel(QStringLiteral("CROSSFADER (A <-> B):"), mixer_group));

    auto *crossfader = new QSlider(Qt::Horizontal, mixer_group);
    crossfader->setRange(0, 1000);
    crossfader->setValue(500);
    crossfader->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 10px; background: #111; border-radius: 5px; }"
        "QSlider::handle:horizontal { background: #ffaa00; width: 22px; margin: -6px 0; border-radius: 11px; }"
    ));
    mixer_layout->addWidget(crossfader);

    auto *lbl_layout = new QHBoxLayout();
    lbl_layout->addWidget(new QLabel(QStringLiteral("Deck A (100%)"), mixer_group));
    auto *lbl_b = new QLabel(QStringLiteral("Deck B (100%)"), mixer_group);
    lbl_b->setAlignment(Qt::AlignRight);
    lbl_layout->addWidget(lbl_b);
    mixer_layout->addLayout(lbl_layout);

    mixer_layout->addStretch();

    connect(transition, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (decks_) decks_->setTransitionMode(index);
    });

    connect(crossfader, &QSlider::valueChanged, this, [this](int value) {
        if (decks_) decks_->setCrossfader(static_cast<double>(value) / 1000.0);
    });

    root_layout->addWidget(deck_a_group, 2);
    root_layout->addWidget(mixer_group, 1);
    root_layout->addWidget(deck_b_group, 2);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        refreshDeck(true);
        refreshDeck(false);
    });
    timer->start(100);
}

void MixerWindow::buildDeckPanel(QWidget *parent, bool is_deck_a)
{
    auto *layout = new QVBoxLayout(parent);
    DeckPlayer *deck = is_deck_a ? &decks_->deckA() : &decks_->deckB();

    auto *file_label = new QLabel(QStringLiteral("No media loaded"), parent);
    file_label->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    file_label->setWordWrap(true);
    layout->addWidget(file_label);

    auto *btn_row = new QHBoxLayout();
    auto *load_btn = new QPushButton(QStringLiteral("Browse File"), parent);
    auto *play_btn = new QPushButton(QStringLiteral("PLAY / PAUSE"), parent);
    play_btn->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));

    auto *loop_btn = new QPushButton(QStringLiteral("LOOP"), parent);
    loop_btn->setCheckable(true);

    btn_row->addWidget(load_btn);
    btn_row->addWidget(play_btn);
    btn_row->addWidget(loop_btn);
    layout->addLayout(btn_row);

    if (is_deck_a) play_btn_a_ = play_btn;
    else play_btn_b_ = play_btn;

    auto *seek_slider = new QSlider(Qt::Horizontal, parent);
    seek_slider->setRange(0, 1000);
    layout->addWidget(new QLabel(QStringLiteral("Position"), parent));
    layout->addWidget(seek_slider);

    if (is_deck_a) seek_a_ = seek_slider;
    else seek_b_ = seek_slider;

    auto add_slider = [layout, parent, deck](const QString &title, void (DeckPlayer::*setter)(float)) {
        layout->addWidget(new QLabel(title, parent));
        auto *s = new QSlider(Qt::Horizontal, parent);
        s->setRange(0, 200);
        s->setValue(100);
        layout->addWidget(s);
        QObject::connect(s, &QSlider::valueChanged, parent, [deck, setter](int v) {
            (deck->*setter)(static_cast<float>(v) / 100.0f);
        });
    };

    add_slider(QStringLiteral("EQ HIGH (Red)"), &DeckPlayer::setRed);
    add_slider(QStringLiteral("EQ MID (Green)"), &DeckPlayer::setGreen);
    add_slider(QStringLiteral("EQ LOW (Blue)"), &DeckPlayer::setBlue);
    add_slider(QStringLiteral("COLOR FX (Saturation)"), &DeckPlayer::setSaturation);

    connect(load_btn, &QPushButton::clicked, this, [this, deck, file_label]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Select Media"), QString(),
            QStringLiteral("Video/Audio (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All (*.*)"));
        if (!path.isEmpty()) {
            deck->loadFile(path);
            file_label->setText(QFileInfo(path).fileName());
        }
    });

    connect(play_btn, &QPushButton::clicked, this, [deck]() { deck->togglePlayback(); });
    connect(loop_btn, &QPushButton::toggled, this, [deck](bool en) { deck->setLooping(en); });
    connect(seek_slider, &QSlider::sliderMoved, this, [deck](int val) {
        deck->seek(static_cast<double>(val) / 1000.0);
    });
}

void MixerWindow::refreshDeck(bool is_deck_a)
{
    DeckPlayer &deck = is_deck_a ? decks_->deckA() : decks_->deckB();
    QSlider *slider = is_deck_a ? seek_a_ : seek_b_;

    if (slider && !slider->isSliderDown()) {
        const QSignalBlocker blocker(slider);
        slider->setValue(static_cast<int>(deck.position() * 1000.0));
    }
}
