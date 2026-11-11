#include "mixer-dock.hpp"
#include "deck-engine.hpp"
#include "waveform-widget.hpp"
#include "midi-handler.hpp"

#include <obs-frontend-api.h>
#include <graphics/vec2.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QFileDialog>
#include <QTimer>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>

namespace {

constexpr const char *TRAKTOR_V2_STYLESHEET = R"(
QDialog {
    background-color: #12151b;
    color: #c8d0de;
    font-family: 'Segoe UI', Arial, sans-serif;
}
QGroupBox {
    background-color: #181d25;
    border: 1px solid #28313e;
    border-radius: 4px;
    margin-top: 14px;
    padding: 6px;
    font-weight: bold;
    font-size: 11px;
    color: #8da0b8;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    color: #00d2ff;
}
QPushButton {
    background-color: #222935;
    color: #e0e6f0;
    border: 1px solid #364254;
    border-radius: 3px;
    padding: 4px 8px;
    font-weight: bold;
    font-size: 10px;
}
QPushButton:hover {
    background-color: #2d3747;
    border-color: #48576e;
    color: #ffffff;
}
QPushButton:pressed {
    background-color: #0088aa;
    color: #ffffff;
}
QPushButton:checked {
    background-color: #e68a00;
    color: #ffffff;
    border-color: #ffaa00;
}
QSlider::groove:horizontal {
    height: 6px;
    background: #101318;
    border: 1px solid #262e3b;
    border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: #202733;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #cdd5e2;
    width: 14px;
    margin: -5px 0;
    border-radius: 2px;
    border: 1px solid #788599;
}
QSlider::handle:horizontal:hover {
    background: #ffffff;
}
QSlider::groove:vertical {
    width: 6px;
    background: #101318;
    border: 1px solid #262e3b;
    border-radius: 3px;
}
QSlider::sub-page:vertical {
    background: #202733;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: #cdd5e2;
    height: 14px;
    margin: 0 -5px;
    border-radius: 2px;
    border: 1px solid #788599;
}
QSlider::handle:vertical:hover {
    background: #ffffff;
}
QComboBox {
    background-color: #171b23;
    color: #d8e0ec;
    border: 1px solid #2b3544;
    border-radius: 3px;
    padding: 3px 6px;
    font-size: 10px;
}
QComboBox:hover {
    border-color: #44546c;
}
QLabel {
    color: #8da0b8;
}
QProgressBar {
    background-color: #101318;
    border: 1px solid #242c38;
    border-radius: 2px;
    text-align: center;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:1, x2:0, y2:0,
        stop:0.0 #00d2ff, stop:0.7 #00ff88, stop:0.9 #ffcc00, stop:1.0 #ff3300);
    border-radius: 1px;
}
)";

} // namespace

MixerWindow::MixerWindow(DeckPair *decks, QWidget *parent)
    : QDialog(parent, Qt::Window),
      decks_(decks)
{
    setWindowTitle(QStringLiteral("OBS VJ/DJ Plugin"));
    resize(1140, 560);
    setStyleSheet(QString::fromUtf8(TRAKTOR_V2_STYLESHEET));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 8, 10, 8);
    main_layout->setSpacing(6);

    // 1. Top Bar: Master controls, OBS Scene integration, Audio Devices & MIDI Status
    buildTopBar(main_layout);

    // 2. Main 3-Column Mixing Deck Area
    auto *deck_row = new QHBoxLayout();
    deck_row->setContentsMargins(0, 0, 0, 0);
    deck_row->setSpacing(8);

    QWidget *deck_a_widget = buildDeckPanel(true);
    QWidget *center_mixer = buildCenterMixerPanel();
    QWidget *deck_b_widget = buildDeckPanel(false);

    deck_row->addWidget(deck_a_widget, 5);
    deck_row->addWidget(center_mixer, 4);
    deck_row->addWidget(deck_b_widget, 5);

    main_layout->addLayout(deck_row, 1);

    loadAudioDevices();

    // 50ms GUI update loop
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        refreshDeck(true);
        refreshDeck(false);
        refreshMixer();
    });
    timer->start(50);

    addToActiveScene();
}

void MixerWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    addToActiveScene();
}

void MixerWindow::buildTopBar(QVBoxLayout *main_layout)
{
    auto *top_bar = new QHBoxLayout();
    top_bar->setContentsMargins(0, 0, 0, 0);
    top_bar->setSpacing(6);

    // Title / Brand
    auto *title_lbl = new QLabel(QStringLiteral("OBS VJ/DJ MIXER"), this);
    title_lbl->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px; letter-spacing: 1px;"));
    top_bar->addWidget(title_lbl);

    top_bar->addSpacing(10);

    // OBS Output Selector Buttons
    auto *out_lbl = new QLabel(QStringLiteral("OBS OUT:"), this);
    out_lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 10px; color: #8da0b8;"));
    top_bar->addWidget(out_lbl);

    obs_out_master_btn_ = new QPushButton(QStringLiteral("MASTER MIX"), this);
    obs_out_deck_a_btn_ = new QPushButton(QStringLiteral("DECK A"), this);
    obs_out_deck_b_btn_ = new QPushButton(QStringLiteral("DECK B"), this);

    obs_out_master_btn_->setCheckable(true);
    obs_out_deck_a_btn_->setCheckable(true);
    obs_out_deck_b_btn_->setCheckable(true);
    obs_out_master_btn_->setChecked(true);

    top_bar->addWidget(obs_out_master_btn_);
    top_bar->addWidget(obs_out_deck_a_btn_);
    top_bar->addWidget(obs_out_deck_b_btn_);

    connect(obs_out_master_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(0); });
    connect(obs_out_deck_a_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(1); });
    connect(obs_out_deck_b_btn_, &QPushButton::clicked, this, [this]() { updateObsOutputButtons(2); });

    // Add to Active Scene button
    add_to_scene_btn_ = new QPushButton(QStringLiteral("📺 ADD TO SCENE"), this);
    add_to_scene_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c3628; border: 1px solid #2e7d32; color: #4caf50; font-weight: bold; font-size: 10px; } "
        "QPushButton:hover { background: #254a36; color: #fff; }"));
    add_to_scene_btn_->setToolTip(QStringLiteral("Add VJ Master Output source directly to current active OBS scene"));
    top_bar->addWidget(add_to_scene_btn_);
    connect(add_to_scene_btn_, &QPushButton::clicked, this, &MixerWindow::addToActiveScene);

    top_bar->addStretch(1);

    // Headphone CUE Monitoring Soundcard Selector
    auto *hp_card_lbl = new QLabel(QStringLiteral("🎧 CUE MONITOR:"), this);
    hp_card_lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 10px; color: #ffaa00;"));
    headphone_device_combo_ = new QComboBox(this);
    headphone_device_combo_->setMinimumWidth(160);
    top_bar->addWidget(hp_card_lbl);
    top_bar->addWidget(headphone_device_combo_);

    top_bar->addSpacing(8);

    // MIDI Hardware Status
    midi_status_lbl_ = new QLabel(QStringLiteral("MIDI: Scanning..."), this);
    midi_status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 10px;"));
    top_bar->addWidget(midi_status_lbl_);

    main_layout->addLayout(top_bar);
}

void MixerWindow::addToActiveScene()
{
    obs_source_t *scene_source = obs_frontend_get_current_scene();
    if (!scene_source)
        return;

    obs_scene_t *scene = obs_scene_from_source(scene_source);
    if (scene) {
        obs_sceneitem_t *existing = obs_scene_find_source(scene, "VJ Master Output");
        if (!existing) {
            obs_source_t *master_src = obs_get_source_by_name("VJ Master Output");
            if (!master_src) {
                master_src = obs_source_create("vdj_master_mixer", "VJ Master Output", nullptr, nullptr);
            }
            if (master_src) {
                obs_sceneitem_t *item = obs_scene_add(scene, master_src);
                if (item) {
                    obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
                    struct vec2 bounds = {1920.0f, 1080.0f};
                    obs_sceneitem_set_bounds(item, &bounds);
                    struct vec2 pos = {0.0f, 0.0f};
                    obs_sceneitem_set_pos(item, &pos);
                }
                obs_source_release(master_src);
            }
        }
    }
    obs_source_release(scene_source);
}

QWidget *MixerWindow::buildDeckPanel(bool is_deck_a)
{
    auto *panel = new QGroupBox(is_deck_a ? QStringLiteral("DECK A") : QStringLiteral("DECK B"), this);
    const QString group_color = is_deck_a ? QStringLiteral("#00d2ff") : QStringLiteral("#ff7700");
    panel->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #161b22; border: 1px solid #28313e; border-radius: 4px; padding: 6px; } "
        "QGroupBox::title { color: %1; font-weight: bold; }").arg(group_color));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    DeckPlayer *deck = is_deck_a ? &decks_->deckA() : &decks_->deckB();

    // 1. Waveform Widget
    auto *waveform = new WaveformWidget(deck, is_deck_a, panel);
    if (is_deck_a)
        waveform_a_ = waveform;
    else
        waveform_b_ = waveform;
    layout->addWidget(waveform);

    // 2. Load File Row
    auto *load_row = new QHBoxLayout();
    auto *load_btn = new QPushButton(is_deck_a ? QStringLiteral("📂 LOAD TRACK A") : QStringLiteral("📂 LOAD TRACK B"), panel);
    load_btn->setStyleSheet(QStringLiteral("font-size: 10px; padding: 3px 6px; font-weight: bold;"));
    load_row->addWidget(load_btn);

    connect(load_btn, &QPushButton::clicked, this, [this, deck, waveform]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select Media"),
            QString(),
            QStringLiteral("Video/Audio (*.mp4 *.mkv *.mov *.avi *.webm *.mp3 *.wav);;All (*.*)"));

        if (!path.isEmpty()) {
            deck->loadFile(path);
            waveform->updateTrackInfo(QFileInfo(path).fileName());
        }
    });

    layout->addLayout(load_row);

    // 3. MOVE & GRID Alignment Row
    auto *grid_row = new QHBoxLayout();
    grid_row->setContentsMargins(0, 0, 0, 0);
    grid_row->setSpacing(4);

    auto *move_lbl = new QLabel(QStringLiteral("MOVE:"), panel);
    move_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #8da0b8;"));
    grid_row->addWidget(move_lbl);

    auto *nudge_left_btn = new QPushButton(QStringLiteral("<<"), panel);
    nudge_left_btn->setFixedSize(32, 22);
    nudge_left_btn->setToolTip(QStringLiteral("Shift beatgrid 10ms earlier"));
    grid_row->addWidget(nudge_left_btn);

    auto *set_grid_btn = new QPushButton(QStringLiteral("SET 1"), panel);
    set_grid_btn->setFixedSize(46, 22);
    set_grid_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2e3d; border: 1px solid #2a5270; color: #00d2ff; font-size: 9px; font-weight: bold; }"));
    set_grid_btn->setToolTip(QStringLiteral("Set downbeat 1 (anchor) exactly to current playhead"));
    grid_row->addWidget(set_grid_btn);

    auto *nudge_right_btn = new QPushButton(QStringLiteral(">>"), panel);
    nudge_right_btn->setFixedSize(32, 22);
    nudge_right_btn->setToolTip(QStringLiteral("Shift beatgrid 10ms later"));
    grid_row->addWidget(nudge_right_btn);

    auto *grid_val_lbl = new QLabel(QStringLiteral("GRID: 0ms"), panel);
    grid_val_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #a0b0c4;"));
    if (is_deck_a)
        grid_lbl_a_ = grid_val_lbl;
    else
        grid_lbl_b_ = grid_val_lbl;
    grid_row->addWidget(grid_val_lbl);
    grid_row->addStretch(1);

    layout->addLayout(grid_row);

    connect(nudge_left_btn, &QPushButton::clicked, this, [deck, waveform, grid_val_lbl]() {
        deck->nudgeGrid(-10);
        grid_val_lbl->setText(QStringLiteral("GRID: %1ms").arg(deck->gridOffsetMs()));
        waveform->refreshWaveform();
    });
    connect(nudge_right_btn, &QPushButton::clicked, this, [deck, waveform, grid_val_lbl]() {
        deck->nudgeGrid(+10);
        grid_val_lbl->setText(QStringLiteral("GRID: %1ms").arg(deck->gridOffsetMs()));
        waveform->refreshWaveform();
    });
    connect(set_grid_btn, &QPushButton::clicked, this, [deck, waveform, grid_val_lbl]() {
        deck->setGridAnchorAtCurrent();
        grid_val_lbl->setText(QStringLiteral("GRID: %1ms").arg(deck->gridOffsetMs()));
        waveform->refreshWaveform();
    });

    // 4. LOOP Matrix & Auto Loop Row
    auto *loop_row = new QHBoxLayout();
    loop_row->setContentsMargins(0, 0, 0, 0);
    loop_row->setSpacing(2);

    const std::vector<QString> loop_labels = {
        QStringLiteral("1/2"), QStringLiteral("1"), QStringLiteral("2"),
        QStringLiteral("4"), QStringLiteral("8"), QStringLiteral("16"), QStringLiteral("32")
    };

    auto &loop_btns = is_deck_a ? loop_btns_a_ : loop_btns_b_;
    loop_btns.clear();

    for (int i = 0; i < static_cast<int>(loop_labels.size()); ++i) {
        auto *btn = new QPushButton(loop_labels[i], panel);
        btn->setCheckable(true);
        btn->setFixedSize(30, 22);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #19202a; border: 1px solid #2a3545; color: #a0b0c4; font-size: 9px; font-weight: bold; } "
            "QPushButton:checked { background: #ffaa00; color: #000; border-color: #ffcc00; }"));
        if (i == 3) // 4 beats default
            btn->setChecked(true);
        loop_btns.push_back(btn);
        loop_row->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, deck, i, &loop_btns]() {
            deck->setLoopBeatIndex(i);
            for (int b = 0; b < static_cast<int>(loop_btns.size()); ++b)
                loop_btns[b]->setChecked(b == i);
        });
    }

    auto *auto_loop_btn = new QPushButton(QStringLiteral("LOOP"), panel);
    auto_loop_btn->setCheckable(true);
    auto_loop_btn->setFixedSize(46, 22);
    auto_loop_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #15281e; border: 1px solid #2e7d32; color: #4caf50; font-size: 9px; font-weight: bold; } "
        "QPushButton:checked { background: #2e7d32; color: #fff; border-color: #4caf50; }"));
    if (is_deck_a)
        loop_toggle_a_ = auto_loop_btn;
    else
        loop_toggle_b_ = auto_loop_btn;
    loop_row->addWidget(auto_loop_btn);

    connect(auto_loop_btn, &QPushButton::toggled, this, [deck](bool checked) {
        deck->setLooping(checked);
    });

    layout->addLayout(loop_row);

    // 5. Transport Controls Row (Traktor style)
    auto *trans_row = new QHBoxLayout();
    trans_row->setContentsMargins(0, 0, 0, 0);
    trans_row->setSpacing(4);

    auto *cue_btn = new QPushButton(QStringLiteral("CUE ||"), panel);
    cue_btn->setFixedHeight(34);
    cue_btn->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; background: #232c38; color: #fff;"));
    trans_row->addWidget(cue_btn, 1);

    auto *set_btn = new QPushButton(QStringLiteral("SET"), panel);
    set_btn->setFixedHeight(34);
    set_btn->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; background: #232c38; color: #00d2ff;"));
    trans_row->addWidget(set_btn, 1);

    auto *cue_play_btn = new QPushButton(QStringLiteral("CUE ▶"), panel);
    cue_play_btn->setFixedHeight(34);
    cue_play_btn->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; background: #232c38; color: #fff;"));
    trans_row->addWidget(cue_play_btn, 1);

    auto *play_btn = new QPushButton(QStringLiteral("▶ / || PLAY"), panel);
    play_btn->setCheckable(true);
    play_btn->setFixedHeight(34);
    play_btn->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; font-weight: bold; background: #252e3c; color: #ffffff; border: 1px solid #3d4a5d; } "
        "QPushButton:checked { background: #e68a00; color: #ffffff; border-color: #ffaa00; }"));
    trans_row->addWidget(play_btn, 2);

    if (is_deck_a) {
        play_btn_a_ = play_btn;
        cue_btn_a_ = cue_btn;
        set_btn_a_ = set_btn;
        cue_play_btn_a_ = cue_play_btn;
    } else {
        play_btn_b_ = play_btn;
        cue_btn_b_ = cue_btn;
        set_btn_b_ = set_btn;
        cue_play_btn_b_ = cue_play_btn;
    }

    connect(play_btn, &QPushButton::clicked, this, [deck]() { deck->togglePlayback(); });
    connect(cue_btn, &QPushButton::clicked, this, [deck]() { deck->cue(); });
    connect(set_btn, &QPushButton::clicked, this, [deck]() {
        deck->setPlaying(false);
        deck->cue();
    });
    connect(cue_play_btn, &QPushButton::clicked, this, [deck]() {
        deck->cue();
        deck->setPlaying(true);
    });

    layout->addLayout(trans_row);

    // 6. Tempo & Pitch Bend Row
    auto *tempo_row = new QHBoxLayout();
    tempo_row->setContentsMargins(0, 0, 0, 0);
    tempo_row->setSpacing(4);

    auto *master_btn = new QPushButton(QStringLiteral("MASTER"), panel);
    master_btn->setCheckable(true);
    master_btn->setFixedHeight(26);
    master_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #192733; border: 1px solid #224d6d; color: #8da0b8; font-weight: bold; font-size: 10px; } "
        "QPushButton:checked { background: #d97706; border: 1px solid #f59e0b; color: #fff; }"));
    tempo_row->addWidget(master_btn);

    auto *sync_btn = new QPushButton(QStringLiteral("SYNC"), panel);
    sync_btn->setCheckable(true);
    sync_btn->setFixedHeight(26);
    sync_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #192733; border: 1px solid #224d6d; color: #00d2ff; font-weight: bold; font-size: 10px; } "
        "QPushButton:checked { background: #0284c7; border: 1px solid #38bdf8; color: #fff; }"));
    tempo_row->addWidget(sync_btn);

    auto *bend_minus = new QPushButton(QStringLiteral("^ -"), panel);
    bend_minus->setFixedSize(28, 26);
    bend_minus->setToolTip(QStringLiteral("Pitch bend slow down"));
    tempo_row->addWidget(bend_minus);

    auto *bend_plus = new QPushButton(QStringLiteral("v +"), panel);
    bend_plus->setFixedSize(28, 26);
    bend_plus->setToolTip(QStringLiteral("Pitch bend speed up"));
    tempo_row->addWidget(bend_plus);

    auto *rate_lbl = new QLabel(QStringLiteral("100%"), panel);
    rate_lbl->setFixedWidth(46);
    rate_lbl->setAlignment(Qt::AlignCenter);
    rate_lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 10px; font-family: monospace; color: #e0e6f0;"));
    tempo_row->addWidget(rate_lbl);

    auto *pitch_slider = new DJSlider(Qt::Horizontal, 100, panel);
    pitch_slider->setRange(50, 150);
    pitch_slider->setValue(100);
    pitch_slider->setToolTip(QStringLiteral("Pitch / Speed Fader (50% - 150%). Double-click resets to 100%."));
    tempo_row->addWidget(pitch_slider, 1);

    auto *rst_pitch_btn = new QPushButton(QStringLiteral("RST"), panel);
    rst_pitch_btn->setFixedSize(30, 24);
    rst_pitch_btn->setStyleSheet(QStringLiteral("font-size: 9px; padding: 1px; font-weight: bold;"));
    tempo_row->addWidget(rst_pitch_btn);

    if (is_deck_a) {
        pitch_slider_a_ = pitch_slider;
        rate_lbl_a_ = rate_lbl;
        master_btn_a_ = master_btn;
        sync_btn_a_ = sync_btn;
    } else {
        pitch_slider_b_ = pitch_slider;
        rate_lbl_b_ = rate_lbl;
        master_btn_b_ = master_btn;
        sync_btn_b_ = sync_btn;
    }

    connect(master_btn, &QPushButton::clicked, this, [this, is_deck_a]() {
        decks_->setDeckMaster(is_deck_a ? 0 : 1);
    });

    connect(sync_btn, &QPushButton::clicked, this, [this, is_deck_a]() {
        const int idx = is_deck_a ? 0 : 1;
        decks_->setSync(idx, !decks_->isSync(idx));
    });

    connect(bend_minus, &QPushButton::pressed, this, [deck]() {
        deck->setSpeed(deck->speed() - 4);
    });
    connect(bend_minus, &QPushButton::released, this, [this, is_deck_a, deck]() {
        auto *sl = is_deck_a ? pitch_slider_a_ : pitch_slider_b_;
        deck->applySpeedSettings(sl->value());
    });

    connect(bend_plus, &QPushButton::pressed, this, [deck]() {
        deck->setSpeed(deck->speed() + 4);
    });
    connect(bend_plus, &QPushButton::released, this, [this, is_deck_a, deck]() {
        auto *sl = is_deck_a ? pitch_slider_a_ : pitch_slider_b_;
        deck->applySpeedSettings(sl->value());
    });

    connect(rst_pitch_btn, &QPushButton::clicked, this, [pitch_slider, deck]() {
        pitch_slider->setValue(100);
        deck->applySpeedSettings(100);
    });

    connect(pitch_slider, &QSlider::valueChanged, this, [deck, rate_lbl](int val) {
        deck->setSpeed(val);
        rate_lbl->setText(QStringLiteral("%1%").arg(val));
    });
    connect(pitch_slider, &QSlider::sliderReleased, this, [deck, pitch_slider]() {
        deck->applySpeedSettings(pitch_slider->value());
    });

    layout->addLayout(tempo_row);

    return panel;
}

QWidget *MixerWindow::buildCenterMixerPanel()
{
    auto *panel = new QGroupBox(QStringLiteral("MIXER"), this);
    panel->setStyleSheet(QStringLiteral(
        "QGroupBox { background: #14181f; border: 1px solid #262e3a; border-radius: 4px; padding: 6px; } "
        "QGroupBox::title { color: #8da0b8; font-weight: bold; }"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    // 0. Traktor Master Clock & Tempo Section
    auto *clock_box = new QFrame(panel);
    clock_box->setStyleSheet(QStringLiteral("background: #0f1319; border: 1px solid #1f2733; border-radius: 3px; padding: 2px;"));
    auto *clock_vbox = new QVBoxLayout(clock_box);
    clock_vbox->setContentsMargins(4, 2, 4, 2);
    clock_vbox->setSpacing(3);

    auto *clock_row1 = new QHBoxLayout();
    clock_row1->setSpacing(4);

    master_clock_btn_ = new QPushButton(QStringLiteral("MASTER"), clock_box);
    master_clock_btn_->setCheckable(true);
    master_clock_btn_->setFixedHeight(22);
    master_clock_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 9px; padding: 1px 5px; } "
        "QPushButton:checked { background: #d97706; border: 1px solid #f59e0b; color: #fff; }"));
    master_clock_btn_->setToolTip(QStringLiteral("Enable Central Master Clock mode"));
    clock_row1->addWidget(master_clock_btn_);

    master_clock_ext_btn_ = new QPushButton(QStringLiteral("EXT SYNC"), clock_box);
    master_clock_ext_btn_->setCheckable(true);
    master_clock_ext_btn_->setFixedHeight(22);
    master_clock_ext_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 9px; padding: 1px 5px; } "
        "QPushButton:checked { background: #0284c7; border: 1px solid #38bdf8; color: #fff; }"));
    master_clock_ext_btn_->setToolTip(QStringLiteral("Sync Master Clock to incoming external MIDI Clock"));
    clock_row1->addWidget(master_clock_ext_btn_);

    master_clock_send_btn_ = new QPushButton(QStringLiteral("SEND CLOCK"), clock_box);
    master_clock_send_btn_->setCheckable(true);
    master_clock_send_btn_->setFixedHeight(22);
    master_clock_send_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 9px; padding: 1px 5px; } "
        "QPushButton:checked { background: #2e7d32; border: 1px solid #4caf50; color: #fff; }"));
    master_clock_send_btn_->setToolTip(QStringLiteral("Transmit MIDI Clock (24 PPQN) to external MIDI hardware / software"));
    clock_row1->addWidget(master_clock_send_btn_);

    clock_vbox->addLayout(clock_row1);

    auto *clock_row2 = new QHBoxLayout();
    clock_row2->setSpacing(4);

    master_clock_bpm_lbl_ = new QLabel(QStringLiteral("128.00 BPM"), clock_box);
    master_clock_bpm_lbl_->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; font-weight: bold; color: #ffaa00;"));
    clock_row2->addWidget(master_clock_bpm_lbl_, 1, Qt::AlignCenter);

    master_clock_tap_btn_ = new QPushButton(QStringLiteral("TAP"), clock_box);
    master_clock_tap_btn_->setFixedSize(30, 22);
    master_clock_tap_btn_->setStyleSheet(QStringLiteral("background: #232b36; border: 1px solid #334050; color: #e0e6f0; font-size: 9px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_tap_btn_);

    master_clock_minus_btn_ = new QPushButton(QStringLiteral("-"), clock_box);
    master_clock_minus_btn_->setFixedSize(22, 22);
    master_clock_minus_btn_->setStyleSheet(QStringLiteral("background: #232b36; border: 1px solid #334050; color: #e0e6f0; font-size: 11px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_minus_btn_);

    master_clock_plus_btn_ = new QPushButton(QStringLiteral("+"), clock_box);
    master_clock_plus_btn_->setFixedSize(22, 22);
    master_clock_plus_btn_->setStyleSheet(QStringLiteral("background: #232b36; border: 1px solid #334050; color: #e0e6f0; font-size: 11px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_plus_btn_);

    clock_vbox->addLayout(clock_row2);

    layout->addWidget(clock_box);

    connect(master_clock_btn_, &QPushButton::clicked, this, [this]() {
        decks_->setMasterMode(DeckPair::MASTER_CLOCK);
    });
    connect(master_clock_ext_btn_, &QPushButton::clicked, this, [this]() {
        if (decks_->masterMode() == DeckPair::MASTER_EXT_MIDI) {
            decks_->setMasterMode(DeckPair::MASTER_CLOCK);
        } else {
            decks_->setMasterMode(DeckPair::MASTER_EXT_MIDI);
        }
    });
    connect(master_clock_send_btn_, &QPushButton::clicked, this, [this]() {
        const bool send = !vdj_midi_is_clock_send();
        vdj_midi_set_clock_send(send);
        master_clock_send_btn_->setChecked(send);
    });
    connect(master_clock_minus_btn_, &QPushButton::clicked, this, [this]() {
        decks_->setMasterClockBpm(decks_->masterClockBpm() - 0.1);
    });
    connect(master_clock_plus_btn_, &QPushButton::clicked, this, [this]() {
        decks_->setMasterClockBpm(decks_->masterClockBpm() + 0.1);
    });
    connect(master_clock_tap_btn_, &QPushButton::clicked, this, [this]() {
        const auto now = std::chrono::steady_clock::now();
        if (!master_tap_times_.empty()) {
            const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - master_tap_times_.back()).count();
            if (diff > 2500)
                master_tap_times_.clear();
        }
        master_tap_times_.push_back(now);
        if (master_tap_times_.size() > 8)
            master_tap_times_.erase(master_tap_times_.begin());
        if (master_tap_times_.size() >= 3) {
            double total_ms = 0.0;
            for (size_t i = 1; i < master_tap_times_.size(); ++i) {
                total_ms += std::chrono::duration_cast<std::chrono::milliseconds>(master_tap_times_[i] - master_tap_times_[i - 1]).count();
            }
            const double avg_ms = total_ms / static_cast<double>(master_tap_times_.size() - 1);
            if (avg_ms > 200.0 && avg_ms < 1500.0) {
                const double tapped_bpm = std::round((60000.0 / avg_ms) * 10.0) / 10.0;
                decks_->setMasterClockBpm(tapped_bpm);
            }
        }
    });

    // 1. FX Transition Selector
    auto *trans_row = new QHBoxLayout();
    auto *trans_lbl = new QLabel(QStringLiteral("TRANSITION:"), panel);
    trans_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #8da0b8;"));
    auto *trans_combo = new QComboBox(panel);
    trans_combo->addItem(QStringLiteral("Linear Fade"), 0);
    trans_combo->addItem(QStringLiteral("Horizontal Wipe"), 1);
    trans_combo->addItem(QStringLiteral("Additive Blend"), 2);
    trans_combo->addItem(QStringLiteral("Hard Cut"), 3);
    trans_row->addWidget(trans_lbl);
    trans_row->addWidget(trans_combo, 1);
    layout->addLayout(trans_row);

    connect(trans_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, trans_combo](int) {
                if (decks_)
                    decks_->setTransitionMode(trans_combo->currentData().toInt());
            });

    // 2. Channel Strips (EQs, Peak Meters, Volume Faders)
    auto *strip_layout = new QHBoxLayout();
    strip_layout->setSpacing(6);

    auto createEqColumn = [this, panel](bool is_deck_a,
                                        DJSlider **out_gain,
                                        DJSlider **out_hi,
                                        DJSlider **out_mid,
                                        DJSlider **out_low,
                                        DJSlider **out_flt) {
        auto *col = new QVBoxLayout();
        col->setSpacing(2);

        DeckPlayer *deck = is_deck_a ? &decks_->deckA() : &decks_->deckB();

        auto addEqSlider = [panel, col, deck](const QString &name, DJSlider **out_sl, int def_val, auto setter) {
            auto *row = new QHBoxLayout();
            auto *lbl = new QLabel(name, panel);
            lbl->setFixedWidth(28);
            lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #788599;"));

            auto *sl = new DJSlider(Qt::Horizontal, def_val, panel);
            sl->setRange(-100, 100);
            sl->setValue(def_val);
            sl->setToolTip(QStringLiteral("%1 EQ. Double click resets to center (0 dB).").arg(name));
            *out_sl = sl;

            row->addWidget(lbl);
            row->addWidget(sl, 1);
            col->addLayout(row);

            QObject::connect(sl, &QSlider::valueChanged, sl, [setter](int val) {
                setter(1.0f + static_cast<float>(val) / 100.0f);
            });
        };

        addEqSlider(QStringLiteral("GAIN"), out_gain, 0, [deck](float v) { deck->setGain(v); });
        addEqSlider(QStringLiteral("HI"), out_hi, 0, [deck](float v) { deck->setEqHi(v); });
        addEqSlider(QStringLiteral("MID"), out_mid, 0, [deck](float v) { deck->setEqMid(v); });
        addEqSlider(QStringLiteral("LOW"), out_low, 0, [deck](float v) { deck->setEqLow(v); });
        addEqSlider(QStringLiteral("FLT"), out_flt, 0, [deck](float v) { deck->setFilterVal(v - 1.0f); });

        return col;
    };

    auto *eq_col_a = createEqColumn(true, &gain_a_, &hi_a_, &mid_a_, &low_a_, &flt_a_);
    auto *eq_col_b = createEqColumn(false, &gain_b_, &hi_b_, &mid_b_, &low_b_, &flt_b_);

    // Center Meters
    auto *meter_col = new QVBoxLayout();
    meter_col->setSpacing(2);
    auto *meter_row = new QHBoxLayout();
    meter_row->setSpacing(3);

    meter_a_ = new QProgressBar(panel);
    meter_a_->setOrientation(Qt::Vertical);
    meter_a_->setRange(0, 100);
    meter_a_->setValue(0);
    meter_a_->setFixedWidth(8);
    meter_a_->setFixedHeight(110);
    meter_a_->setTextVisible(false);

    meter_b_ = new QProgressBar(panel);
    meter_b_->setOrientation(Qt::Vertical);
    meter_b_->setRange(0, 100);
    meter_b_->setValue(0);
    meter_b_->setFixedWidth(8);
    meter_b_->setFixedHeight(110);
    meter_b_->setTextVisible(false);

    meter_row->addWidget(meter_a_);
    meter_row->addWidget(meter_b_);
    meter_col->addLayout(meter_row);

    strip_layout->addLayout(eq_col_a, 2);
    strip_layout->addLayout(meter_col, 1);
    strip_layout->addLayout(eq_col_b, 2);
    layout->addLayout(strip_layout);

    // 3. Channel Faders & Headphone CUE Row
    auto *fader_row = new QHBoxLayout();
    fader_row->setSpacing(10);

    // Deck A Fader + Cue
    auto *fader_col_a = new QVBoxLayout();
    hp_cue_a_btn_ = new QPushButton(QStringLiteral("🎧 CUE A"), panel);
    hp_cue_a_btn_->setCheckable(true);
    hp_cue_a_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #19232d; border: 1px solid #2b4156; color: #00d2ff; font-weight: bold; font-size: 9px; } "
        "QPushButton:checked { background: #0088cc; color: #fff; border-color: #00d2ff; }"));
    vol_a_ = new DJSlider(Qt::Vertical, 100, panel);
    vol_a_->setRange(0, 100);
    vol_a_->setValue(100);
    vol_a_->setFixedHeight(115);
    vol_a_->setToolTip(QStringLiteral("Deck A Volume Fader. Double-click resets to 100%."));
    fader_col_a->addWidget(hp_cue_a_btn_);
    fader_col_a->addWidget(vol_a_, 0, Qt::AlignCenter);

    // Deck B Fader + Cue
    auto *fader_col_b = new QVBoxLayout();
    hp_cue_b_btn_ = new QPushButton(QStringLiteral("🎧 CUE B"), panel);
    hp_cue_b_btn_->setCheckable(true);
    hp_cue_b_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #281e19; border: 1px solid #56362b; color: #ff7700; font-weight: bold; font-size: 9px; } "
        "QPushButton:checked { background: #cc5500; color: #fff; border-color: #ff7700; }"));
    vol_b_ = new DJSlider(Qt::Vertical, 100, panel);
    vol_b_->setRange(0, 100);
    vol_b_->setValue(100);
    vol_b_->setFixedHeight(115);
    vol_b_->setToolTip(QStringLiteral("Deck B Volume Fader. Double-click resets to 100%."));
    fader_col_b->addWidget(hp_cue_b_btn_);
    fader_col_b->addWidget(vol_b_, 0, Qt::AlignCenter);

    // Center Headphone Mix / Vol
    auto *hp_ctrl_col = new QVBoxLayout();
    hp_ctrl_col->setSpacing(2);

    auto *hp_vol_lbl = new QLabel(QStringLiteral("HP VOL"), panel);
    hp_vol_lbl->setAlignment(Qt::AlignCenter);
    hp_vol_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #ffaa00;"));
    hp_vol_slider_ = new DJSlider(Qt::Horizontal, 100, panel);
    hp_vol_slider_->setRange(0, 100);
    hp_vol_slider_->setValue(100);
    hp_vol_slider_->setToolTip(QStringLiteral("Headphone Monitor Volume. Double click resets to 100%."));

    auto *hp_mix_lbl = new QLabel(QStringLiteral("CUE / PGM"), panel);
    hp_mix_lbl->setAlignment(Qt::AlignCenter);
    hp_mix_lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: #ffaa00;"));
    hp_mix_slider_ = new DJSlider(Qt::Horizontal, 50, panel);
    hp_mix_slider_->setRange(0, 100);
    hp_mix_slider_->setValue(50);
    hp_mix_slider_->setToolTip(QStringLiteral("Headphone Cue / Master mix. Double click resets to 50/50."));

    hp_ctrl_col->addWidget(hp_vol_lbl);
    hp_ctrl_col->addWidget(hp_vol_slider_);
    hp_ctrl_col->addWidget(hp_mix_lbl);
    hp_ctrl_col->addWidget(hp_mix_slider_);

    fader_row->addLayout(fader_col_a);
    fader_row->addLayout(hp_ctrl_col, 1);
    fader_row->addLayout(fader_col_b);
    layout->addLayout(fader_row);

    connect(hp_cue_a_btn_, &QPushButton::toggled, this, [this](bool chk) {
        if (decks_)
            decks_->deckA().setHeadphoneCue(chk);
    });
    connect(hp_cue_b_btn_, &QPushButton::toggled, this, [this](bool chk) {
        if (decks_)
            decks_->deckB().setHeadphoneCue(chk);
    });

    connect(vol_a_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_)
            decks_->deckA().setVolume(static_cast<float>(v) / 100.0f);
    });
    connect(vol_b_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_)
            decks_->deckB().setVolume(static_cast<float>(v) / 100.0f);
    });

    connect(hp_vol_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_)
            decks_->setHeadphoneVolume(static_cast<float>(v) / 100.0f);
    });
    connect(hp_mix_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_)
            decks_->setHeadphoneMix(static_cast<float>(v) / 100.0f);
    });

    // 4. Center Master Crossfader
    auto *cf_box = new QVBoxLayout();
    cf_box->setContentsMargins(0, 4, 0, 2);

    auto *cf_hdr = new QHBoxLayout();
    auto *cf_a = new QLabel(QStringLiteral("◄ DECK A"), panel);
    cf_a->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 10px;"));
    auto *cf_title = new QLabel(QStringLiteral("CROSSFADER"), panel);
    cf_title->setStyleSheet(QStringLiteral("color: #788599; font-weight: bold; font-size: 9px;"));
    auto *cf_b = new QLabel(QStringLiteral("DECK B ►"), panel);
    cf_b->setStyleSheet(QStringLiteral("color: #ff7700; font-weight: bold; font-size: 10px;"));

    cf_hdr->addWidget(cf_a);
    cf_hdr->addStretch();
    cf_hdr->addWidget(cf_title);
    cf_hdr->addStretch();
    cf_hdr->addWidget(cf_b);
    cf_box->addLayout(cf_hdr);

    crossfader_ = new DJSlider(Qt::Horizontal, 500, panel);
    crossfader_->setRange(0, 1000);
    crossfader_->setValue(500);
    crossfader_->setFixedHeight(26);
    crossfader_->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 8px; background: #0e1218; border: 1px solid #283240; border-radius: 4px; } "
        "QSlider::handle:horizontal { background: #ffffff; width: 22px; margin: -5px 0; border-radius: 2px; border: 2px solid #0088aa; }"));
    crossfader_->setToolTip(QStringLiteral("Master Crossfader. Double click resets to exact center (50/50)."));
    cf_box->addWidget(crossfader_);

    connect(crossfader_, &QSlider::valueChanged, this, [this](int val) {
        if (decks_)
            decks_->setCrossfader(static_cast<double>(val) / 1000.0);
    });

    layout->addLayout(cf_box);

    return panel;
}

void MixerWindow::updateObsOutputButtons(int mode)
{
    if (decks_)
        decks_->setObsOutputMode(mode);

    obs_out_master_btn_->setChecked(mode == 0);
    obs_out_deck_a_btn_->setChecked(mode == 1);
    obs_out_deck_b_btn_->setChecked(mode == 2);
}

void MixerWindow::loadAudioDevices()
{
    if (!headphone_device_combo_)
        return;

    const QSignalBlocker blocker(headphone_device_combo_);
    headphone_device_combo_->clear();

    obs_enum_audio_monitoring_devices(
        [](void *param, const char *name, const char *id) -> bool {
            auto *cb = static_cast<QComboBox *>(param);
            const QString dev_name = QString::fromUtf8(name);
            const QString dev_id = QString::fromUtf8(id);

            cb->addItem(dev_name, dev_id);
            return true;
        },
        headphone_device_combo_);

    const char *current_name = nullptr;
    const char *current_id = nullptr;
    obs_get_audio_monitoring_device(&current_name, &current_id);
    if (current_id && *current_id) {
        const QString cur_str = QString::fromUtf8(current_id);
        const int idx_h = headphone_device_combo_->findData(cur_str);
        if (idx_h >= 0)
            headphone_device_combo_->setCurrentIndex(idx_h);
    }

    connect(headphone_device_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (index < 0 || !headphone_device_combo_)
                    return;
                const QString id = headphone_device_combo_->itemData(index).toString();
                const QString name = headphone_device_combo_->itemText(index);
                if (!id.isEmpty() && obs_audio_monitoring_available()) {
                    obs_set_audio_monitoring_device(name.toUtf8().constData(), id.toUtf8().constData());
                }
            });
}

void MixerWindow::refreshDeck(bool is_deck_a)
{
    if (!decks_)
        return;

    DeckPlayer &deck = is_deck_a ? decks_->deckA() : decks_->deckB();

    // Run seamless beat loop check on every GUI tick (every 50 ms)
    deck.updateLoop();

    auto *waveform = is_deck_a ? waveform_a_ : waveform_b_;
    auto *play_btn = is_deck_a ? play_btn_a_ : play_btn_b_;
    auto *loop_btn = is_deck_a ? loop_toggle_a_ : loop_toggle_b_;
    auto *pitch_sl = is_deck_a ? pitch_slider_a_ : pitch_slider_b_;
    auto *rate_lbl = is_deck_a ? rate_lbl_a_ : rate_lbl_b_;
    auto *grid_lbl = is_deck_a ? grid_lbl_a_ : grid_lbl_b_;

    if (waveform)
        waveform->refreshWaveform();

    if (play_btn && play_btn->isChecked() != deck.isPlaying()) {
        QSignalBlocker blocker(play_btn);
        play_btn->setChecked(deck.isPlaying());
    }

    if (loop_btn && loop_btn->isChecked() != deck.isLooping()) {
        QSignalBlocker blocker(loop_btn);
        loop_btn->setChecked(deck.isLooping());
    }

    if (pitch_sl && !pitch_sl->isSliderDown()) {
        const int spd = deck.speed();
        if (pitch_sl->value() != spd) {
            QSignalBlocker blocker(pitch_sl);
            pitch_sl->setValue(spd);
            if (rate_lbl)
                rate_lbl->setText(QStringLiteral("%1%").arg(spd));
        }
    }

    auto syncSlider = [](DJSlider *sl, int val) {
        if (sl && !sl->isSliderDown() && std::abs(sl->value() - val) > 1) {
            QSignalBlocker blocker(sl);
            sl->setValue(val);
        }
    };
    syncSlider(is_deck_a ? hi_a_ : hi_b_, static_cast<int>((deck.eqHi() - 1.0f) * 100.0f));
    syncSlider(is_deck_a ? mid_a_ : mid_b_, static_cast<int>((deck.eqMid() - 1.0f) * 100.0f));
    syncSlider(is_deck_a ? low_a_ : low_b_, static_cast<int>((deck.eqLow() - 1.0f) * 100.0f));
    syncSlider(is_deck_a ? flt_a_ : flt_b_, static_cast<int>(deck.filterVal() * 100.0f));
    syncSlider(is_deck_a ? vol_a_ : vol_b_, static_cast<int>(deck.volume() * 100.0f));

    if (grid_lbl) {
        grid_lbl->setText(QStringLiteral("GRID: %1ms").arg(deck.gridOffsetMs()));
    }

    auto *master_btn = is_deck_a ? master_btn_a_ : master_btn_b_;
    if (master_btn) {
        const bool is_m = is_deck_a ? (decks_->masterMode() == DeckPair::MASTER_DECK_A) : (decks_->masterMode() == DeckPair::MASTER_DECK_B);
        if (master_btn->isChecked() != is_m) {
            QSignalBlocker blocker(master_btn);
            master_btn->setChecked(is_m);
        }
    }

    auto *sync_btn = is_deck_a ? sync_btn_a_ : sync_btn_b_;
    if (sync_btn) {
        const bool is_s = decks_->isSync(is_deck_a ? 0 : 1);
        if (sync_btn->isChecked() != is_s) {
            QSignalBlocker blocker(sync_btn);
            sync_btn->setChecked(is_s);
        }
    }
}

void MixerWindow::refreshMixer()
{
    if (!decks_)
        return;

    decks_->updateSync();

    if (master_clock_btn_) {
        const bool is_clock_m = (decks_->masterMode() == DeckPair::MASTER_CLOCK);
        if (master_clock_btn_->isChecked() != is_clock_m) {
            QSignalBlocker blocker(master_clock_btn_);
            master_clock_btn_->setChecked(is_clock_m);
        }
    }

    if (master_clock_ext_btn_) {
        const bool is_ext_m = (decks_->masterMode() == DeckPair::MASTER_EXT_MIDI);
        if (master_clock_ext_btn_->isChecked() != is_ext_m) {
            QSignalBlocker blocker(master_clock_ext_btn_);
            master_clock_ext_btn_->setChecked(is_ext_m);
        }
    }

    if (master_clock_send_btn_) {
        const bool is_send = vdj_midi_is_clock_send();
        if (master_clock_send_btn_->isChecked() != is_send) {
            QSignalBlocker blocker(master_clock_send_btn_);
            master_clock_send_btn_->setChecked(is_send);
        }
    }

    if (master_clock_bpm_lbl_) {
        master_clock_bpm_lbl_->setText(QString::asprintf("%.2f BPM", decks_->effectiveMasterBpm()));
    }

    if (crossfader_ && !crossfader_->isSliderDown()) {
        const int cf_val = static_cast<int>(decks_->crossfader() * 1000.0);
        if (std::abs(crossfader_->value() - cf_val) > 2) {
            QSignalBlocker blocker(crossfader_);
            crossfader_->setValue(cf_val);
        }
    }

    // Animate Peak Meters based on playing status and volume
    if (meter_a_) {
        const bool playing_a = decks_->deckA().isPlaying();
        const int vol_a = static_cast<int>(decks_->deckA().volume() * 85.0f);
        meter_a_->setValue(playing_a ? std::min(100, vol_a + (rand() % 12)) : 0);
    }
    if (meter_b_) {
        const bool playing_b = decks_->deckB().isPlaying();
        const int vol_b = static_cast<int>(decks_->deckB().volume() * 85.0f);
        meter_b_->setValue(playing_b ? std::min(100, vol_b + (rand() % 12)) : 0);
    }

    if (midi_status_lbl_) {
        const bool conn = vdj_midi_is_connected();
        midi_status_lbl_->setText(conn ? QStringLiteral("MIDI: NI S2 MK3 CONNECTED") : QStringLiteral("MIDI: Disconnected"));
        midi_status_lbl_->setStyleSheet(conn
            ? QStringLiteral("color: #00ff88; font-weight: bold; font-size: 10px;")
            : QStringLiteral("color: #ff5555; font-weight: bold; font-size: 10px;"));
    }
}
