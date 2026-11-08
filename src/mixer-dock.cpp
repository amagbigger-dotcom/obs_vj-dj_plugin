#include "mixer-dock.hpp"
#include "deck-engine.hpp"
#include "midi-handler.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

static const char *TRAKTOR_S2_STYLESHEET = R"(
QDialog {
    background-color: #121316;
    color: #e0e2e6;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 12px;
}
QGroupBox {
    background: #1a1c22;
    border: 1px solid #2a2d36;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 12px;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top center;
    padding: 2px 10px;
    border-radius: 3px;
}
QGroupBox#deck_a {
    border: 1px solid #0088aa;
}
QGroupBox#deck_a::title {
    background-color: #007799;
    color: #ffffff;
}
QGroupBox#deck_b {
    border: 1px solid #cc5500;
}
QGroupBox#deck_b::title {
    background-color: #bb4400;
    color: #ffffff;
}
QPushButton {
    background-color: #272a33;
    color: #ffffff;
    border: 1px solid #3c404d;
    border-radius: 4px;
    padding: 6px 12px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: #343844;
}
QPushButton:pressed {
    background-color: #1a1c23;
}
QPushButton#play_btn {
    background-color: #1b3d2b;
    border: 1px solid #2ecc71;
    color: #2ecc71;
    font-size: 13px;
    font-weight: bold;
    min-height: 28px;
}
QPushButton#play_btn:hover {
    background-color: #27ae60;
    color: #ffffff;
}
QPushButton#cue_btn {
    background-color: #4a3412;
    border: 1px solid #f39c12;
    color: #f39c12;
    font-size: 13px;
    font-weight: bold;
    min-height: 28px;
}
QPushButton#cue_btn:hover {
    background-color: #e67e22;
    color: #ffffff;
}
QPushButton#sync_btn {
    background-color: #1c2e42;
    border: 1px solid #3498db;
    color: #3498db;
    font-weight: bold;
}
QSlider::groove:horizontal {
    height: 6px;
    background: #252831;
    border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: #0088aa;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #e0e2e6;
    width: 14px;
    margin: -4px 0;
    border-radius: 7px;
}
QSlider::groove:vertical {
    width: 6px;
    background: #252831;
    border-radius: 3px;
}
QSlider::sub-page:vertical {
    background: #3c404d;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: #e0e2e6;
    height: 16px;
    margin: 0 -5px;
    border-radius: 3px;
}
QComboBox {
    background-color: #22252e;
    color: #ffffff;
    border: 1px solid #3a3e4c;
    border-radius: 4px;
    padding: 4px 8px;
}
QLabel {
    color: #9aa0ac;
}
)";

QString MixerWindow::formatTime(qint64 ms)
{
    const qint64 total_sec = ms / 1000;
    const qint64 minutes = total_sec / 60;
    const qint64 seconds = total_sec % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("Traktor S2 VJ Mixer - OBS"));
    resize(1040, 580);
    setStyleSheet(TRAKTOR_S2_STYLESHEET);

    auto *main_v_layout = new QVBoxLayout(this);
    main_v_layout->setContentsMargins(12, 10, 12, 12);
    main_v_layout->setSpacing(8);

    // Felso allapotsor: MIDI Kapcsolat
    auto *status_bar = new QHBoxLayout();
    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Scanning..."), this);
    midi_status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px;"));
    status_bar->addWidget(midi_status_lbl_);
    status_bar->addStretch();
    main_v_layout->addLayout(status_bar);

    auto *root_layout = new QHBoxLayout();
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(10);

    auto *deck_a_group = new QGroupBox(QStringLiteral("DECK A"), this);
    deck_a_group->setObjectName(QStringLiteral("deck_a"));

    auto *mixer_group = new QGroupBox(QStringLiteral("TRAKTOR 2-CH MIXER"), this);
    mixer_group->setObjectName(QStringLiteral("mixer_section"));

    auto *deck_b_group = new QGroupBox(QStringLiteral("DECK B"), this);
    deck_b_group->setObjectName(QStringLiteral("deck_b"));

    buildDeckPanel(deck_a_group, true);
    buildMixerSection(mixer_group);
    buildDeckPanel(deck_b_group, false);

    root_layout->addWidget(deck_a_group, 3);
    root_layout->addWidget(mixer_group, 2);
    root_layout->addWidget(deck_b_group, 3);

    main_v_layout->addLayout(root_layout);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        refreshDeck(true);
        refreshDeck(false);
        refreshMixer();
    });
    timer->start(80);
}

void MixerWindow::buildDeckPanel(QWidget *parent, bool is_deck_a)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    DeckPlayer *deck = is_deck_a ? &decks_->deckA() : &decks_->deckB();

    // 1. File Info & Browse
    auto *file_row = new QHBoxLayout();
    auto *file_label = new QLabel(QStringLiteral("Empty Deck - Click Load"), parent);
    file_label->setStyleSheet(QStringLiteral("color: #7d8494; font-size: 11px; font-weight: bold;"));
    file_label->setWordWrap(true);
    file_label->setMaximumHeight(32);

    auto *load_btn = new QPushButton(QStringLiteral("LOAD"), parent);
    load_btn->setFixedWidth(60);
    file_row->addWidget(file_label, 1);
    file_row->addWidget(load_btn);
    layout->addLayout(file_row);

    // 2. Waveform / Position Scrub Bar + Time Display
    auto *time_row = new QHBoxLayout();
    auto *pos_lbl = new QLabel(QStringLiteral("00:00 / 00:00"), parent);
    pos_lbl->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-family: monospace;"));
    if (is_deck_a) time_a_ = pos_lbl;
    else time_b_ = pos_lbl;
    time_row->addWidget(new QLabel(QStringLiteral("Position"), parent));
    time_row->addStretch();
    time_row->addWidget(pos_lbl);
    layout->addLayout(time_row);

    auto *seek_slider = new QSlider(Qt::Horizontal, parent);
    seek_slider->setRange(0, 1000);
    layout->addWidget(seek_slider);
    if (is_deck_a) seek_a_ = seek_slider;
    else seek_b_ = seek_slider;

    // 3. Middle Section: Pitch Slider (Traktor S2 Tempo Fader) + Loop Bar
    auto *mid_row = new QHBoxLayout();

    auto *pitch_box = new QVBoxLayout();
    auto *rate_lbl = new QLabel(QStringLiteral("0.0%"), parent);
    rate_lbl->setAlignment(Qt::AlignCenter);
    rate_lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    if (is_deck_a) rate_lbl_a_ = rate_lbl;
    else rate_lbl_b_ = rate_lbl;

    auto *pitch_slider = new QSlider(Qt::Vertical, parent);
    pitch_slider->setRange(50, 150);
    pitch_slider->setValue(100);
    pitch_slider->setFixedHeight(120);

    if (is_deck_a) pitch_slider_a_ = pitch_slider;
    else pitch_slider_b_ = pitch_slider;

    pitch_box->addWidget(new QLabel(QStringLiteral("TEMPO"), parent), 0, Qt::AlignCenter);
    pitch_box->addWidget(pitch_slider, 0, Qt::AlignCenter);
    pitch_box->addWidget(rate_lbl, 0, Qt::AlignCenter);
    mid_row->addLayout(pitch_box);

    auto *loop_box = new QVBoxLayout();
    auto *loop_lbl = new QLabel(QStringLiteral("LOOP SECTION"), parent);
    loop_lbl->setStyleSheet(QStringLiteral("font-weight: bold; color: #a0a6b5;"));
    loop_box->addWidget(loop_lbl);

    auto *loop_btn = new QPushButton(QStringLiteral("AUTO LOOP"), parent);
    loop_btn->setCheckable(true);
    loop_box->addWidget(loop_btn);

    auto *reset_pitch_btn = new QPushButton(QStringLiteral("RESET PITCH"), parent);
    loop_box->addWidget(reset_pitch_btn);

    loop_box->addStretch();
    mid_row->addLayout(loop_box);
    layout->addLayout(mid_row);

    // 4. Transport Controls (Traktor S2: PLAY, CUE, SYNC)
    auto *transport_row = new QHBoxLayout();
    auto *play_btn = new QPushButton(QStringLiteral("PLAY / PAUSE"), parent);
    play_btn->setObjectName(QStringLiteral("play_btn"));
    if (is_deck_a) play_btn_a_ = play_btn;
    else play_btn_b_ = play_btn;

    auto *cue_btn = new QPushButton(QStringLiteral("CUE"), parent);
    cue_btn->setObjectName(QStringLiteral("cue_btn"));

    auto *sync_btn = new QPushButton(QStringLiteral("SYNC"), parent);
    sync_btn->setObjectName(QStringLiteral("sync_btn"));

    transport_row->addWidget(play_btn, 2);
    transport_row->addWidget(cue_btn, 1);
    transport_row->addWidget(sync_btn, 1);
    layout->addLayout(transport_row);

    // Event Connections
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
    connect(cue_btn, &QPushButton::clicked, this, [deck]() { deck->cue(); });
    connect(loop_btn, &QPushButton::toggled, this, [deck](bool en) { deck->setLooping(en); });

    connect(seek_slider, &QSlider::sliderMoved, this, [deck](int val) {
        deck->seek(static_cast<double>(val) / 1000.0);
    });

    connect(pitch_slider, &QSlider::valueChanged, this, [deck, rate_lbl](int val) {
        deck->setSpeed(val);
        const double diff = static_cast<double>(val - 100);
        rate_lbl->setText(QString("%1%2%").arg(diff >= 0 ? "+" : "").arg(diff, 0, 'f', 1));
    });

    connect(reset_pitch_btn, &QPushButton::clicked, this, [pitch_slider]() {
        pitch_slider->setValue(100);
    });

    connect(sync_btn, &QPushButton::clicked, this, [this, is_deck_a, pitch_slider]() {
        DeckPlayer &other = is_deck_a ? decks_->deckB() : decks_->deckA();
        pitch_slider->setValue(other.speed());
    });
}

void MixerWindow::buildMixerSection(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    auto *strips_layout = new QHBoxLayout();

    // Deck A Strip
    auto *col_a = new QVBoxLayout();
    col_a->addWidget(new QLabel(QStringLiteral("CH A"), parent), 0, Qt::AlignCenter);

    col_a->addWidget(new QLabel(QStringLiteral("HI"), parent), 0, Qt::AlignCenter);
    hi_a_ = new QSlider(Qt::Horizontal, parent);
    hi_a_->setRange(0, 200); hi_a_->setValue(100);
    col_a->addWidget(hi_a_);
    connect(hi_a_, &QSlider::valueChanged, this, [this](int v) { decks_->deckA().setRed(v / 100.0f); });

    col_a->addWidget(new QLabel(QStringLiteral("MID"), parent), 0, Qt::AlignCenter);
    mid_a_ = new QSlider(Qt::Horizontal, parent);
    mid_a_->setRange(0, 200); mid_a_->setValue(100);
    col_a->addWidget(mid_a_);
    connect(mid_a_, &QSlider::valueChanged, this, [this](int v) { decks_->deckA().setGreen(v / 100.0f); });

    col_a->addWidget(new QLabel(QStringLiteral("LOW"), parent), 0, Qt::AlignCenter);
    low_a_ = new QSlider(Qt::Horizontal, parent);
    low_a_->setRange(0, 200); low_a_->setValue(100);
    col_a->addWidget(low_a_);
    connect(low_a_, &QSlider::valueChanged, this, [this](int v) { decks_->deckA().setBlue(v / 100.0f); });

    col_a->addWidget(new QLabel(QStringLiteral("FILTER"), parent), 0, Qt::AlignCenter);
    flt_a_ = new QSlider(Qt::Horizontal, parent);
    flt_a_->setRange(0, 200); flt_a_->setValue(100);
    col_a->addWidget(flt_a_);
    connect(flt_a_, &QSlider::valueChanged, this, [this](int v) { decks_->deckA().setSaturation(v / 100.0f); });

    col_a->addWidget(new QLabel(QStringLiteral("VOL"), parent), 0, Qt::AlignCenter);
    vol_a_ = new QSlider(Qt::Vertical, parent);
    vol_a_->setRange(0, 100); vol_a_->setValue(100);
    vol_a_->setFixedHeight(90);
    col_a->addWidget(vol_a_, 0, Qt::AlignCenter);
    connect(vol_a_, &QSlider::valueChanged, this, [this](int v) { decks_->deckA().setVolume(v / 100.0f); });

    strips_layout->addLayout(col_a);

    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet(QStringLiteral("color: #2c303a;"));
    strips_layout->addWidget(line);

    // Deck B Strip
    auto *col_b = new QVBoxLayout();
    col_b->addWidget(new QLabel(QStringLiteral("CH B"), parent), 0, Qt::AlignCenter);

    col_b->addWidget(new QLabel(QStringLiteral("HI"), parent), 0, Qt::AlignCenter);
    hi_b_ = new QSlider(Qt::Horizontal, parent);
    hi_b_->setRange(0, 200); hi_b_->setValue(100);
    col_b->addWidget(hi_b_);
    connect(hi_b_, &QSlider::valueChanged, this, [this](int v) { decks_->deckB().setRed(v / 100.0f); });

    col_b->addWidget(new QLabel(QStringLiteral("MID"), parent), 0, Qt::AlignCenter);
    mid_b_ = new QSlider(Qt::Horizontal, parent);
    mid_b_->setRange(0, 200); mid_b_->setValue(100);
    col_b->addWidget(mid_b_);
    connect(mid_b_, &QSlider::valueChanged, this, [this](int v) { decks_->deckB().setGreen(v / 100.0f); });

    col_b->addWidget(new QLabel(QStringLiteral("LOW"), parent), 0, Qt::AlignCenter);
    low_b_ = new QSlider(Qt::Horizontal, parent);
    low_b_->setRange(0, 200); low_b_->setValue(100);
    col_b->addWidget(low_b_);
    connect(low_b_, &QSlider::valueChanged, this, [this](int v) { decks_->deckB().setBlue(v / 100.0f); });

    col_b->addWidget(new QLabel(QStringLiteral("FILTER"), parent), 0, Qt::AlignCenter);
    flt_b_ = new QSlider(Qt::Horizontal, parent);
    flt_b_->setRange(0, 200); flt_b_->setValue(100);
    col_b->addWidget(flt_b_);
    connect(flt_b_, &QSlider::valueChanged, this, [this](int v) { decks_->deckB().setSaturation(v / 100.0f); });

    col_b->addWidget(new QLabel(QStringLiteral("VOL"), parent), 0, Qt::AlignCenter);
    vol_b_ = new QSlider(Qt::Vertical, parent);
    vol_b_->setRange(0, 100); vol_b_->setValue(100);
    vol_b_->setFixedHeight(90);
    col_b->addWidget(vol_b_, 0, Qt::AlignCenter);
    connect(vol_b_, &QSlider::valueChanged, this, [this](int v) { decks_->deckB().setVolume(v / 100.0f); });

    strips_layout->addLayout(col_b);
    layout->addLayout(strips_layout);

    layout->addSpacing(6);

    auto *trans_lbl = new QLabel(QStringLiteral("FX Transition:"), parent);
    layout->addWidget(trans_lbl);

    auto *transition = new QComboBox(parent);
    transition->addItem(QStringLiteral("Linear Fade"));
    transition->addItem(QStringLiteral("Horizontal Wipe"));
    transition->addItem(QStringLiteral("Additive Blend"));
    transition->addItem(QStringLiteral("Hard Cut"));
    layout->addWidget(transition);

    connect(transition, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (decks_) decks_->setTransitionMode(idx);
    });

    layout->addSpacing(8);

    auto *xfade_lbl = new QLabel(QStringLiteral("CROSSFADER (A <-> B)"), parent);
    xfade_lbl->setAlignment(Qt::AlignCenter);
    xfade_lbl->setStyleSheet(QStringLiteral("font-weight: bold; color: #ffffff;"));
    layout->addWidget(xfade_lbl);

    crossfader_ = new QSlider(Qt::Horizontal, parent);
    crossfader_->setRange(0, 1000);
    crossfader_->setValue(500);
    crossfader_->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 12px; background: #14161b; border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: #0088aa; border-radius: 6px; }"
        "QSlider::handle:horizontal { background: #ff9e00; width: 24px; margin: -6px 0; border-radius: 12px; }"
    ));
    layout->addWidget(crossfader_);

    connect(crossfader_, &QSlider::valueChanged, this, [this](int val) {
        if (decks_) decks_->setCrossfader(static_cast<double>(val) / 1000.0);
    });
}

void MixerWindow::refreshDeck(bool is_deck_a)
{
    DeckPlayer &deck = is_deck_a ? decks_->deckA() : decks_->deckB();
    QSlider *slider = is_deck_a ? seek_a_ : seek_b_;
    QLabel *time_lbl = is_deck_a ? time_a_ : time_b_;

    if (slider && !slider->isSliderDown()) {
        const QSignalBlocker blocker(slider);
        slider->setValue(static_cast<int>(deck.position() * 1000.0));
    }

    if (time_lbl) {
        const qint64 cur = deck.currentTimeMs();
        const qint64 dur = deck.durationMs();
        time_lbl->setText(QString("%1 / %2").arg(formatTime(cur), formatTime(dur)));
    }
}

void MixerWindow::refreshMixer()
{
    if (midi_status_lbl_) {
        if (vdj_midi_is_connected()) {
            midi_status_lbl_->setText(QString("MIDI: Connected (%1)").arg(vdj_midi_get_device_name()));
            midi_status_lbl_->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold; font-size: 11px;"));
        } else {
            midi_status_lbl_->setText(QStringLiteral("MIDI: Disconnected (Plug in Traktor S2 MK3)"));
            midi_status_lbl_->setStyleSheet(QStringLiteral("color: #e67e22; font-weight: bold; font-size: 11px;"));
        }
    }

    if (!decks_)
        return;

    auto sync_slider = [](QSlider *s, float val, float scale) {
        if (s && !s->isSliderDown()) {
            const QSignalBlocker blocker(s);
            s->setValue(static_cast<int>(val * scale));
        }
    };

    // Deck A
    sync_slider(hi_a_, decks_->deckA().red(), 100.0f);
    sync_slider(mid_a_, decks_->deckA().green(), 100.0f);
    sync_slider(low_a_, decks_->deckA().blue(), 100.0f);
    sync_slider(flt_a_, decks_->deckA().saturation(), 100.0f);
    sync_slider(vol_a_, decks_->deckA().volume(), 100.0f);
    if (pitch_slider_a_ && !pitch_slider_a_->isSliderDown()) {
        const QSignalBlocker blocker(pitch_slider_a_);
        pitch_slider_a_->setValue(decks_->deckA().speed());
        const double diff = static_cast<double>(decks_->deckA().speed() - 100);
        if (rate_lbl_a_) rate_lbl_a_->setText(QString("%1%2%").arg(diff >= 0 ? "+" : "").arg(diff, 0, 'f', 1));
    }

    // Deck B
    sync_slider(hi_b_, decks_->deckB().red(), 100.0f);
    sync_slider(mid_b_, decks_->deckB().green(), 100.0f);
    sync_slider(low_b_, decks_->deckB().blue(), 100.0f);
    sync_slider(flt_b_, decks_->deckB().saturation(), 100.0f);
    sync_slider(vol_b_, decks_->deckB().volume(), 100.0f);
    if (pitch_slider_b_ && !pitch_slider_b_->isSliderDown()) {
        const QSignalBlocker blocker(pitch_slider_b_);
        pitch_slider_b_->setValue(decks_->deckB().speed());
        const double diff = static_cast<double>(decks_->deckB().speed() - 100);
        if (rate_lbl_b_) rate_lbl_b_->setText(QString("%1%2%").arg(diff >= 0 ? "+" : "").arg(diff, 0, 'f', 1));
    }

    // Crossfader
    if (crossfader_ && !crossfader_->isSliderDown()) {
        const QSignalBlocker blocker(crossfader_);
        crossfader_->setValue(static_cast<int>(decks_->crossfader() * 1000.0));
    }
}
