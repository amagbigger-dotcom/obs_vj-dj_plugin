#include "gui/mixer_master.hpp"
#include "gui/crossfade_fx_dialog.hpp"
#include "gui/lfo_dialog.hpp"
#include "engine/deck_engine.hpp"
#include "controllers/midi_handler.hpp"
#include "widget/dj_slider.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>

MixerMasterPanel::MixerMasterPanel(DeckPair *decks, QWidget *parent)
    : QGroupBox(QStringLiteral("MASTER & CROSSFADER"), parent),
      decks_(decks)
{
    setupUi();
}

void MixerMasterPanel::setupUi()
{
    setStyleSheet(QStringLiteral(
        "QGroupBox { background: #13171e; border: 1px solid #222936; border-radius: 4px; padding: 4px; } "
        "QGroupBox::title { color: #8da0b8; font-weight: bold; font-size: 10px; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    // 0. Master Clock & Tempo Section
    auto *clock_box = new QFrame(this);
    clock_box->setStyleSheet(QStringLiteral("background: #0f1319; border: 1px solid #1f2733; border-radius: 3px; padding: 2px;"));
    auto *clock_vbox = new QVBoxLayout(clock_box);
    clock_vbox->setContentsMargins(2, 2, 2, 2);
    clock_vbox->setSpacing(2);

    auto *clock_row1 = new QHBoxLayout();
    clock_row1->setSpacing(3);

    master_clock_btn_ = new QPushButton(QStringLiteral("MASTER"), clock_box);
    master_clock_btn_->setCheckable(true);
    master_clock_btn_->setFixedHeight(20);
    master_clock_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 8px; padding: 1px 3px; } "
        "QPushButton:checked { background: #d97706; border: 1px solid #f59e0b; color: #fff; }"));
    master_clock_btn_->setToolTip(QStringLiteral("Enable Central Master Clock mode"));
    clock_row1->addWidget(master_clock_btn_);

    master_clock_ext_btn_ = new QPushButton(QStringLiteral("EXT SYNC"), clock_box);
    master_clock_ext_btn_->setCheckable(true);
    master_clock_ext_btn_->setFixedHeight(20);
    master_clock_ext_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 8px; padding: 1px 3px; } "
        "QPushButton:checked { background: #0284c7; border: 1px solid #38bdf8; color: #fff; }"));
    master_clock_ext_btn_->setToolTip(QStringLiteral("Sync Master Clock to incoming external MIDI Clock"));
    clock_row1->addWidget(master_clock_ext_btn_);

    master_clock_send_btn_ = new QPushButton(QStringLiteral("SEND CLOCK"), clock_box);
    master_clock_send_btn_->setCheckable(true);
    master_clock_send_btn_->setFixedHeight(20);
    master_clock_send_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a222d; border: 1px solid #2d3b4d; color: #8da0b8; font-weight: bold; font-size: 8px; padding: 1px 3px; } "
        "QPushButton:checked { background: #2e7d32; border: 1px solid #4caf50; color: #fff; }"));
    master_clock_send_btn_->setToolTip(QStringLiteral("Transmit MIDI Clock (24 PPQN) to external MIDI hardware / software"));
    clock_row1->addWidget(master_clock_send_btn_);

    clock_vbox->addLayout(clock_row1);

    auto *clock_row2 = new QHBoxLayout();
    clock_row2->setSpacing(4);

    master_clock_minus_btn_ = new QPushButton(QStringLiteral("−"), clock_box);
    master_clock_minus_btn_->setFixedSize(22, 22);
    master_clock_minus_btn_->setStyleSheet(QStringLiteral("background: #232b36; border: 1px solid #334050; color: #e0e6f0; font-size: 11px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_minus_btn_);

    master_clock_bpm_edit_ = new QLineEdit(QStringLiteral("128.00"), clock_box);
    master_clock_bpm_edit_->setAlignment(Qt::AlignCenter);
    master_clock_bpm_edit_->setFixedHeight(22);
    master_clock_bpm_edit_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #141a22; color: #ffaa00; border: 1px solid #2d3b4d; border-radius: 3px; font-size: 11px; font-family: monospace; font-weight: bold; padding: 1px; } "
        "QLineEdit:focus { border: 1px solid #ffaa00; background: #1a222d; }"));
    master_clock_bpm_edit_->setToolTip(QStringLiteral("Master Clock BPM. Írd be és nyomj Entert!"));
    clock_row2->addWidget(master_clock_bpm_edit_, 1);

    connect(master_clock_bpm_edit_, &QLineEdit::returnPressed, this, [this]() {
        commitMasterBpm();
        master_clock_bpm_edit_->clearFocus();
    });
    connect(master_clock_bpm_edit_, &QLineEdit::editingFinished, this, [this]() {
        commitMasterBpm();
    });

    master_clock_plus_btn_ = new QPushButton(QStringLiteral("+"), clock_box);
    master_clock_plus_btn_->setFixedSize(22, 22);
    master_clock_plus_btn_->setStyleSheet(QStringLiteral("background: #232b36; border: 1px solid #334050; color: #e0e6f0; font-size: 11px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_plus_btn_);

    master_clock_tap_btn_ = new QPushButton(QStringLiteral("TAP"), clock_box);
    master_clock_tap_btn_->setFixedSize(32, 22);
    master_clock_tap_btn_->setStyleSheet(QStringLiteral("background: #1c2634; border: 1px solid #385070; color: #00d2ff; font-size: 9px; font-weight: bold;"));
    clock_row2->addWidget(master_clock_tap_btn_);

    clock_vbox->addLayout(clock_row2);
    layout->addWidget(clock_box);

    connect(master_clock_btn_, &QPushButton::clicked, this, [this]() {
        if (!decks_) return;
        if (decks_->masterMode() != DeckPair::MASTER_CLOCK) {
            const double cur_tempo = decks_->effectiveMasterBpm();
            if (cur_tempo > 30.0) {
                decks_->setMasterClockBpm(cur_tempo);
            } else {
                decks_->setMasterMode(DeckPair::MASTER_CLOCK);
            }
            master_clock_btn_->setChecked(true);
        } else {
            decks_->setMasterMode(DeckPair::MASTER_CLOCK);
        }
        decks_->updateSync();
    });
    connect(master_clock_ext_btn_, &QPushButton::clicked, this, [this]() {
        if (!decks_) return;
        if (decks_->masterMode() == DeckPair::MASTER_EXT_MIDI) {
            decks_->setMasterMode(DeckPair::MASTER_CLOCK);
        } else {
            decks_->setMasterMode(DeckPair::MASTER_EXT_MIDI);
        }
        decks_->updateSync();
    });
    connect(master_clock_send_btn_, &QPushButton::clicked, this, [this]() {
        const bool send = !vdj_midi_is_clock_send();
        vdj_midi_set_clock_send(send);
        master_clock_send_btn_->setChecked(send);
    });
    connect(master_clock_minus_btn_, &QPushButton::clicked, this, [this]() {
        if (decks_) {
            decks_->setMasterClockBpm(decks_->masterClockBpm() - 0.1);
            decks_->updateSync();
            if (master_clock_btn_) master_clock_btn_->setChecked(true);
        }
    });
    connect(master_clock_plus_btn_, &QPushButton::clicked, this, [this]() {
        if (decks_) {
            decks_->setMasterClockBpm(decks_->masterClockBpm() + 0.1);
            decks_->updateSync();
            if (master_clock_btn_) master_clock_btn_->setChecked(true);
        }
    });
    connect(master_clock_tap_btn_, &QPushButton::clicked, this, [this]() {
        const auto now = std::chrono::steady_clock::now();
        if (!master_tap_times_.empty()) {
            const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - master_tap_times_.back()).count();
            if (diff > 2500)
                master_tap_times_.clear();
        }
        master_tap_times_.push_back(now);
        while (master_tap_times_.size() > 8)
            master_tap_times_.erase(master_tap_times_.begin());
        if (master_tap_times_.size() >= 3 && decks_) {
            const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(master_tap_times_.back() - master_tap_times_.front()).count();
            const double avg_ms = static_cast<double>(total_ms) / static_cast<double>(master_tap_times_.size() - 1);
            if (avg_ms > 150.0) {
                const double tap_bpm = 60000.0 / avg_ms;
                decks_->setMasterClockBpm(tap_bpm);
                decks_->updateSync();
                if (master_clock_btn_) master_clock_btn_->setChecked(true);
            }
        }
    });

    // 1. Headphone Monitoring Section
    auto *hp_box = new QFrame(this);
    hp_box->setStyleSheet(QStringLiteral("background: #0f1218; border: 1px solid #1c232f; border-radius: 3px; padding: 4px;"));
    auto *hp_vbox = new QVBoxLayout(hp_box);
    hp_vbox->setContentsMargins(4, 2, 4, 2);
    hp_vbox->setSpacing(3);

    auto *hp_hdr_lbl = new QLabel(QStringLiteral("HEADPHONE MONITOR"), hp_box);
    hp_hdr_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #ffaa00;"));
    hp_vbox->addWidget(hp_hdr_lbl);

    auto *hp_sl_row = new QHBoxLayout();
    hp_sl_row->setSpacing(6);

    auto *hp_vol_vbox = new QVBoxLayout();
    auto *hp_vol_hdr = new QHBoxLayout();
    auto *hp_vol_lbl = new QLabel(QStringLiteral("HP VOL"), hp_box);
    hp_vol_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #8da0b8;"));
    hp_vol_val_lbl_ = new QLabel(QStringLiteral("100%"), hp_box);
    hp_vol_val_lbl_->setFixedWidth(34);
    hp_vol_val_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hp_vol_val_lbl_->setStyleSheet(QStringLiteral("font-size: 8px; font-family: monospace; color: #ffaa00;"));
    hp_vol_hdr->addWidget(hp_vol_lbl);
    hp_vol_hdr->addStretch();
    hp_vol_hdr->addWidget(hp_vol_val_lbl_);

    hp_vol_slider_ = new DJSlider(Qt::Horizontal, 100, hp_box);
    hp_vol_slider_->setRange(0, 100);
    hp_vol_slider_->setValue(100);
    hp_vol_vbox->addLayout(hp_vol_hdr);
    hp_vol_vbox->addWidget(hp_vol_slider_);

    auto *hp_mix_vbox = new QVBoxLayout();
    auto *hp_mix_hdr = new QHBoxLayout();
    auto *hp_mix_lbl = new QLabel(QStringLiteral("CUE / PGM"), hp_box);
    hp_mix_lbl->setStyleSheet(QStringLiteral("font-size: 8px; font-weight: bold; color: #8da0b8;"));
    hp_mix_val_lbl_ = new QLabel(QStringLiteral("50/50"), hp_box);
    hp_mix_val_lbl_->setFixedWidth(38);
    hp_mix_val_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hp_mix_val_lbl_->setStyleSheet(QStringLiteral("font-size: 8px; font-family: monospace; color: #ffaa00;"));
    hp_mix_hdr->addWidget(hp_mix_lbl);
    hp_mix_hdr->addStretch();
    hp_mix_hdr->addWidget(hp_mix_val_lbl_);

    hp_mix_slider_ = new DJSlider(Qt::Horizontal, 50, hp_box);
    hp_mix_slider_->setRange(0, 100);
    hp_mix_slider_->setValue(50);
    hp_mix_vbox->addLayout(hp_mix_hdr);
    hp_mix_vbox->addWidget(hp_mix_slider_);

    hp_sl_row->addLayout(hp_vol_vbox);
    hp_sl_row->addLayout(hp_mix_vbox);
    hp_vbox->addLayout(hp_sl_row);

    layout->addWidget(hp_box);

    connect(hp_vol_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_) decks_->setHeadphoneVolume(static_cast<float>(v) / 100.0f);
        if (hp_vol_val_lbl_) hp_vol_val_lbl_->setText(QString::asprintf("%d%%", v));
    });
    connect(hp_mix_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (decks_) decks_->setHeadphoneMix(static_cast<float>(v) / 100.0f);
        if (hp_mix_val_lbl_) hp_mix_val_lbl_->setText(QString::asprintf("%d/%d", 100 - v, v));
    });

    layout->addStretch(1);

    // 2. Center Master Crossfader
    auto *cf_box = new QVBoxLayout();
    cf_box->setContentsMargins(0, 2, 0, 2);

    auto *cf_hdr = new QHBoxLayout();
    auto *cf_a = new QLabel(QStringLiteral("◄ A"), this);
    cf_a->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold; font-size: 11px;"));
    auto *cf_title = new QLabel(QStringLiteral("CROSSFADER"), this);
    cf_title->setStyleSheet(QStringLiteral("color: #788599; font-weight: bold; font-size: 9px;"));
    auto *cf_b = new QLabel(QStringLiteral("B ►"), this);
    cf_b->setStyleSheet(QStringLiteral("color: #ff7700; font-weight: bold; font-size: 11px;"));

    cf_hdr->addWidget(cf_a);
    cf_hdr->addStretch();
    cf_hdr->addWidget(cf_title);
    cf_hdr->addStretch();
    cf_hdr->addWidget(cf_b);
    cf_box->addLayout(cf_hdr);

    cf_val_lbl_ = new QLabel(QStringLiteral("◄ 50% | 50% ►"), this);
    cf_val_lbl_->setFixedWidth(130);
    cf_val_lbl_->setAlignment(Qt::AlignCenter);
    cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #8da0b8; font-weight: bold;"));

    auto *cf_val_row = new QHBoxLayout();
    cf_val_row->addStretch(1);
    cf_val_row->addWidget(cf_val_lbl_);
    cf_val_row->addStretch(1);
    cf_box->addLayout(cf_val_row);

    // Crossfader FX Dropdown & Canvas Scale Mode (FIT vs FILL)
    auto *fx_scale_row = new QHBoxLayout();
    fx_scale_row->setContentsMargins(0, 0, 0, 0);
    fx_scale_row->setSpacing(4);

    cf_fx_combo_ = new QComboBox(this);
    cf_fx_combo_->setFixedHeight(20);
    cf_fx_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #12161f; color: #8da0b8; border: 1px solid #242d3d; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #181d26; color: #fff; selection-background-color: #005588; }"));
    cf_fx_combo_->view()->setMinimumWidth(280);
    cf_fx_combo_->view()->setTextElideMode(Qt::ElideNone);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Ki (Normál)"), 0);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Spirális Örvény"), 1);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Halszem Optika"), 2);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: RGB Split"), 3);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Radial Zoom Blur"), 4);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Pixelesedés (Mosaic)"), 5);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Luma Dissolve"), 6);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Folyadék Hullám"), 7);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Kaleidoszkóp"), 8);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Glitch Displacement"), 9);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: CRT / VHS Scanline"), 10);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Pop-Art Halftone"), 11);
    cf_fx_combo_->addItem(QStringLiteral("XFADE FX: Neon Kontúr"), 12);
    cf_fx_combo_->setCurrentIndex(decks_ ? decks_->crossfadeFxMode() : 0);
    connect(cf_fx_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (decks_) decks_->setCrossfadeFxMode(idx);
    });
    fx_scale_row->addWidget(cf_fx_combo_, 1);

    cf_fx_settings_btn_ = new QPushButton(QStringLiteral("⚙ FX"), this);
    cf_fx_settings_btn_->setFixedHeight(20);
    cf_fx_settings_btn_->setToolTip(QStringLiteral("Crossfader Vizuális FX Paraméterek és Zenei Moduláció"));
    cf_fx_settings_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a2230; border: 1px solid #2d3e56; color: #ffaa00; font-size: 8px; font-weight: bold; padding: 1px 5px; border-radius: 3px; } "
        "QPushButton:hover { background: #253347; border-color: #ffaa00; }"));
    connect(cf_fx_settings_btn_, &QPushButton::clicked, this, []() {
        vdj_open_crossfade_fx_dialog();
    });
    fx_scale_row->addWidget(cf_fx_settings_btn_);

    lfo_btn_ = new QPushButton(QStringLiteral("〰 LFO"), this);
    lfo_btn_->setFixedHeight(20);
    lfo_btn_->setToolTip(QStringLiteral("BPM-Szinkronizált LFO Oszcillátorok (1/16..64 ütem, 4 hullámforma)"));
    lfo_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a2230; border: 1px solid #2d3e56; color: #00d2ff; font-size: 8px; font-weight: bold; padding: 1px 5px; border-radius: 3px; } "
        "QPushButton:hover { background: #253347; border-color: #00d2ff; }"));
    connect(lfo_btn_, &QPushButton::clicked, this, []() {
        vdj_open_lfo_dialog();
    });
    fx_scale_row->addWidget(lfo_btn_);

    cf_box->addLayout(fx_scale_row);

    crossfader_ = new DJSlider(Qt::Horizontal, 500, this);
    crossfader_->setRange(0, 1000);
    crossfader_->setValue(500);
    crossfader_->setFixedHeight(28);
    crossfader_->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 10px; background: #0c0f14; border: 1px solid #232b38; border-radius: 5px; } "
        "QSlider::handle:horizontal { background: #ffffff; width: 26px; margin: -6px 0; border-radius: 3px; border: 2px solid #0088aa; }"));
    crossfader_->setToolTip(QStringLiteral("Master Crossfader (Double-click resets to 50/50 center)"));
    cf_box->addWidget(crossfader_);

    connect(crossfader_, &QSlider::valueChanged, this, [this](int val) {
        if (decks_) {
            decks_->setCrossfader(static_cast<double>(val) / 1000.0);
        }
        updateCfLabel(val);
    });

    layout->addLayout(cf_box);
}

void MixerMasterPanel::commitMasterBpm()
{
    if (!decks_ || !master_clock_bpm_edit_)
        return;

    bool ok = false;
    const double val = master_clock_bpm_edit_->text().toDouble(&ok);
    if (ok && val >= 30.0 && val <= 300.0) {
        decks_->setMasterClockBpm(val);
        decks_->updateSync();
        if (master_clock_btn_) master_clock_btn_->setChecked(true);
    }
}

void MixerMasterPanel::updateCfLabel(int val)
{
    if (!cf_val_lbl_)
        return;

    const int pct_b = val / 10;
    const int pct_a = 100 - pct_b;
    if (val == 500) {
        cf_val_lbl_->setText(QStringLiteral("◄ 50% | 50% ►"));
        cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #8da0b8; font-weight: bold;"));
    } else if (val < 500) {
        cf_val_lbl_->setText(QString::asprintf("◄ A %d%% | %d%%", pct_a, pct_b));
        cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #00d2ff; font-weight: bold;"));
    } else {
        cf_val_lbl_->setText(QString::asprintf("%d%% | B %d%% ►", pct_a, pct_b));
        cf_val_lbl_->setStyleSheet(QStringLiteral("font-size: 9px; font-family: monospace; color: #ff7700; font-weight: bold;"));
    }
}

void MixerMasterPanel::refresh()
{
    if (!decks_)
        return;

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
    if (master_clock_bpm_edit_ && !master_clock_bpm_edit_->hasFocus()) {
        master_clock_bpm_edit_->setText(QString::asprintf("%.2f", decks_->effectiveMasterBpm()));
    }

    if (crossfader_ && !crossfader_->isSliderDown()) {
        const int cf_val = static_cast<int>(decks_->crossfader() * 1000.0);
        if (std::abs(crossfader_->value() - cf_val) > 2) {
            QSignalBlocker blocker(crossfader_);
            crossfader_->setValue(cf_val);
        }
        updateCfLabel(cf_val);
    }

    if (cf_fx_dialog_ && cf_fx_dialog_->isVisible()) {
        cf_fx_dialog_->refresh();
    }
}

bool MixerMasterPanel::handleKeyPress(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if (master_clock_bpm_edit_ && master_clock_bpm_edit_->hasFocus()) {
            commitMasterBpm();
            master_clock_bpm_edit_->clearFocus();
            event->accept();
            return true;
        }
    }
    if (event->key() == Qt::Key_Escape) {
        if (master_clock_bpm_edit_ && master_clock_bpm_edit_->hasFocus()) {
            master_clock_bpm_edit_->clearFocus();
            event->accept();
            return true;
        }
    }
    return false;
}
