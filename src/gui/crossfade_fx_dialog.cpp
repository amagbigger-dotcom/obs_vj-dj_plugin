#include "gui/crossfade_fx_dialog.hpp"
#include "engine/deck_engine.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <algorithm>

namespace {

constexpr const char *CF_FX_STYLESHEET = R"(
QDialog {
    background-color: #11141a;
    color: #c8d0de;
    font-family: 'Segoe UI', Arial, sans-serif;
}
QPushButton {
    background-color: #1f2733;
    color: #e0e6f0;
    border: 1px solid #2d3848;
    border-radius: 4px;
    font-weight: bold;
    font-size: 10px;
    padding: 3px 8px;
}
QPushButton:hover {
    background-color: #2b3648;
    border-color: #ffaa00;
}
)";

const char *getCrossfadeFxName(int mode) {
    static const char *names[] = {
        "Ki (Normál Fade)",
        "Spirális Örvény (Swirl Vortex)",
        "Halszem Optika (Fisheye Lens)",
        "RGB Split (Chromatic Aberration)",
        "Radial Zoom Blur (Suhanó nagyítás)",
        "Pixelesedés (Pixelate / Mosaic)",
        "Luma Dissolve (Fényerő áttűnés)",
        "Folyadék Hullámzás (Liquid Ripple Warp)",
        "Kaleidoszkóp (Mirror Fold)",
        "Glitch Displacement (Zajelcsúszás)",
        "CRT / VHS Scanline (Analóg TV & Csík)",
        "Pop-Art Halftone (Képregény Pontrács)",
        "Neon Kontúr (Sobel Edge Glow)"
    };
    if (mode >= 0 && mode < 13) {
        return names[mode];
    }
    return "Egyedi FX";
}

} // namespace

CrossfadeFxDialog::CrossfadeFxDialog(DeckPair *decks, QWidget *parent)
    : QDialog(parent), decks_(decks)
{
    setWindowTitle(QStringLiteral("Crossfader Vizuális FX Paraméterek"));
    setFixedSize(380, 215);
    setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setStyleSheet(QString::fromUtf8(CF_FX_STYLESHEET));

    setupUi();
    updateFromDecks();
}

void CrossfadeFxDialog::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 8, 10, 8);
    main_layout->setSpacing(6);

    // Title banner
    auto *hdr_layout = new QHBoxLayout();
    hdr_layout->setContentsMargins(0, 0, 0, 0);

    auto *icon_lbl = new QLabel(QStringLiteral("⚡"), this);
    icon_lbl->setStyleSheet(QStringLiteral("font-size: 13px; color: #ffaa00; font-weight: bold;"));
    hdr_layout->addWidget(icon_lbl);

    fx_title_lbl_ = new QLabel(this);
    fx_title_lbl_->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #00d2ff;"));
    hdr_layout->addWidget(fx_title_lbl_, 1);

    reset_btn_ = new QPushButton(QStringLiteral("↺ Reset"), this);
    reset_btn_->setFixedHeight(20);
    reset_btn_->setToolTip(QStringLiteral("Paraméterek visszaállítása alapértelmezett 1.0x-re"));
    connect(reset_btn_, &QPushButton::clicked, this, [this]() {
        if (!decks_) return;
        decks_->setCrossfadeFxIntensity(1.0f);
        decks_->setCrossfadeFxIntensityMod(0);
        decks_->setCrossfadeFxRate(1.0f);
        decks_->setCrossfadeFxRateMod(0);
        updateFromDecks();
    });
    hdr_layout->addWidget(reset_btn_);

    main_layout->addLayout(hdr_layout);

    // Container Frame
    auto *box = new QFrame(this);
    box->setStyleSheet(QStringLiteral("background: #141922; border: 1px solid #222b3a; border-radius: 4px; padding: 6px;"));
    auto *box_layout = new QVBoxLayout(box);
    box_layout->setContentsMargins(6, 4, 6, 4);
    box_layout->setSpacing(6);

    // --- Param 1: Intensity ---
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(6);

    auto *lbl1 = new QLabel(QStringLiteral("Erősség / Intensity:"), box);
    lbl1->setFixedWidth(115);
    lbl1->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #d0d8e8;"));
    row1->addWidget(lbl1);

    intensity_val_lbl_ = new QLabel(QStringLiteral("1.00x"), box);
    intensity_val_lbl_->setFixedWidth(36);
    intensity_val_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    intensity_val_lbl_->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace; color: #00d2ff; font-weight: bold;"));
    row1->addWidget(intensity_val_lbl_);

    intensity_slider_ = new QSlider(Qt::Horizontal, box);
    intensity_slider_->setRange(0, 200); // 0.00x to 2.00x
    intensity_slider_->setValue(100);
    intensity_slider_->setFixedHeight(16);
    intensity_slider_->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; background: #263345; border-radius: 2px; } "
        "QSlider::sub-page:horizontal { background: #0088cc; border-radius: 2px; } "
        "QSlider::handle:horizontal { background: #00d2ff; width: 12px; margin: -4px 0; border-radius: 6px; }"));
    connect(intensity_slider_, &QSlider::valueChanged, this, [this](int val) {
        float fval = static_cast<float>(val) / 100.0f;
        intensity_val_lbl_->setText(QString::number(fval, 'f', 2) + QStringLiteral("x"));
        if (decks_) decks_->setCrossfadeFxIntensity(fval);
    });
    row1->addWidget(intensity_slider_, 1);

    intensity_mod_combo_ = new QComboBox(box);
    intensity_mod_combo_->setFixedWidth(125);
    intensity_mod_combo_->setFixedHeight(20);
    intensity_mod_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1a222f; color: #e0e6f0; border: 1px solid #2d3e56; font-size: 8px; padding: 1px 4px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #12171f; color: #fff; selection-background-color: #006699; }"));
    intensity_mod_combo_->addItem(QStringLiteral("✋ Kézi (Off)"), 0);
    intensity_mod_combo_->addItem(QStringLiteral("🥁 Kick Pulzus"), 1);
    intensity_mod_combo_->addItem(QStringLiteral("📈 Zenei Tónus"), 2);
    intensity_mod_combo_->addItem(QStringLiteral("⚡ Mindkettő"), 3);
    connect(intensity_mod_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (decks_) decks_->setCrossfadeFxIntensityMod(idx);
    });
    row1->addWidget(intensity_mod_combo_);

    box_layout->addLayout(row1);

    // --- Param 2: Rate / Speed / Scale ---
    auto *row2 = new QHBoxLayout();
    row2->setSpacing(6);

    auto *lbl2 = new QLabel(QStringLiteral("Lépték / Ráta:"), box);
    lbl2->setFixedWidth(115);
    lbl2->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #d0d8e8;"));
    row2->addWidget(lbl2);

    rate_val_lbl_ = new QLabel(QStringLiteral("1.00x"), box);
    rate_val_lbl_->setFixedWidth(36);
    rate_val_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rate_val_lbl_->setStyleSheet(QStringLiteral("font-size: 10px; font-family: monospace; color: #ffaa00; font-weight: bold;"));
    row2->addWidget(rate_val_lbl_);

    rate_slider_ = new QSlider(Qt::Horizontal, box);
    rate_slider_->setRange(10, 300); // 0.10x to 3.00x
    rate_slider_->setValue(100);
    rate_slider_->setFixedHeight(16);
    rate_slider_->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; background: #263345; border-radius: 2px; } "
        "QSlider::sub-page:horizontal { background: #cc7700; border-radius: 2px; } "
        "QSlider::handle:horizontal { background: #ffaa00; width: 12px; margin: -4px 0; border-radius: 6px; }"));
    connect(rate_slider_, &QSlider::valueChanged, this, [this](int val) {
        float fval = static_cast<float>(val) / 100.0f;
        rate_val_lbl_->setText(QString::number(fval, 'f', 2) + QStringLiteral("x"));
        if (decks_) decks_->setCrossfadeFxRate(fval);
    });
    row2->addWidget(rate_slider_, 1);

    rate_mod_combo_ = new QComboBox(box);
    rate_mod_combo_->setFixedWidth(125);
    rate_mod_combo_->setFixedHeight(20);
    rate_mod_combo_->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1a222f; color: #e0e6f0; border: 1px solid #2d3e56; font-size: 8px; padding: 1px 4px; border-radius: 3px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #12171f; color: #fff; selection-background-color: #aa5500; }"));
    rate_mod_combo_->addItem(QStringLiteral("✋ Kézi (Off)"), 0);
    rate_mod_combo_->addItem(QStringLiteral("🥁 Kick Pulzus"), 1);
    rate_mod_combo_->addItem(QStringLiteral("📈 Zenei Tónus"), 2);
    rate_mod_combo_->addItem(QStringLiteral("⚡ Mindkettő"), 3);
    connect(rate_mod_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (decks_) decks_->setCrossfadeFxRateMod(idx);
    });
    row2->addWidget(rate_mod_combo_);

    box_layout->addLayout(row2);
    main_layout->addWidget(box);

    auto *hint_lbl = new QLabel(QStringLiteral("💡 A beállított értékek valós időben vezérlik a választott crossfader effektet.\n"
                                              "A Kick Pulzus leütésre tágítja, a Zenei Tónus a zene felépülésével fokozza a hatást."), this);
    hint_lbl->setStyleSheet(QStringLiteral("font-size: 8px; color: #7f8fa4; font-style: italic;"));
    main_layout->addWidget(hint_lbl);
}

void CrossfadeFxDialog::updateFromDecks()
{
    if (!decks_) return;

    const int mode = decks_->crossfadeFxMode();
    fx_title_lbl_->setText(QStringLiteral("Effekt: %1").arg(QString::fromUtf8(getCrossfadeFxName(mode))));

    const float inten = decks_->crossfadeFxIntensity();
    const int inten_mod = decks_->crossfadeFxIntensityMod();
    const float rate = decks_->crossfadeFxRate();
    const int rate_mod = decks_->crossfadeFxRateMod();

    if (intensity_slider_ && !intensity_slider_->isSliderDown()) {
        QSignalBlocker b(intensity_slider_);
        intensity_slider_->setValue(static_cast<int>(std::round(inten * 100.0f)));
    }
    if (intensity_val_lbl_) {
        intensity_val_lbl_->setText(QString::number(inten, 'f', 2) + QStringLiteral("x"));
    }
    if (intensity_mod_combo_) {
        QSignalBlocker b(intensity_mod_combo_);
        intensity_mod_combo_->setCurrentIndex(std::clamp(inten_mod, 0, 3));
    }

    if (rate_slider_ && !rate_slider_->isSliderDown()) {
        QSignalBlocker b(rate_slider_);
        rate_slider_->setValue(static_cast<int>(std::round(rate * 100.0f)));
    }
    if (rate_val_lbl_) {
        rate_val_lbl_->setText(QString::number(rate, 'f', 2) + QStringLiteral("x"));
    }
    if (rate_mod_combo_) {
        QSignalBlocker b(rate_mod_combo_);
        rate_mod_combo_->setCurrentIndex(std::clamp(rate_mod, 0, 3));
    }
}

void CrossfadeFxDialog::refresh()
{
    if (!isVisible() || !decks_)
        return;
    updateFromDecks();
}
