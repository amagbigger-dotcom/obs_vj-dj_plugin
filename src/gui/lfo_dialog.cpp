#include "gui/lfo_dialog.hpp"
#include "engine/deck_engine.hpp"
#include <obs-frontend-api.h>
#include <util/platform.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QSignalBlocker>
#include <cmath>
#include <algorithm>

// -----------------------------------------------------------------------------
// Live Mini Oscilloscope for LFO
// -----------------------------------------------------------------------------
class LfoScopeWidget final : public QWidget {
public:
    explicit LfoScopeWidget(int lfo_idx, DeckPair *decks, const QColor &color, QWidget *parent = nullptr)
        : QWidget(parent), lfo_idx_(lfo_idx), decks_(decks), color_(color)
    {
        setFixedHeight(50);
        setMinimumWidth(180);
        setStyleSheet(QStringLiteral("background: #090c10; border: 1px solid #1c2635; border-radius: 4px;"));
    }

    void setConfig(const LfoConfig &cfg) {
        cfg_ = cfg;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int w = width();
        const int h = height();
        const float mid_y = h * 0.5f;

        // Background grid
        p.fillRect(rect(), QColor(9, 12, 16));
        p.setPen(QPen(QColor(24, 34, 48), 1, Qt::DashLine));
        p.drawLine(0, static_cast<int>(mid_y), w, static_cast<int>(mid_y));
        p.drawLine(w / 4, 0, w / 4, h);
        p.drawLine(w / 2, 0, w / 2, h);
        p.drawLine(w * 3 / 4, 0, w * 3 / 4, h);

        // Draw waveform path across 1 full cycle
        QPainterPath path;
        const int steps = w;
        for (int x = 0; x < steps; ++x) {
            float phi = static_cast<float>(x) / static_cast<float>(steps);
            float raw = 0.0f;
            switch (cfg_.waveform) {
            case 0: // Szinusz
                raw = std::sin(2.0f * 3.14159265f * phi);
                break;
            case 1: // Háromszög
                raw = 2.0f * std::abs(2.0f * (phi - std::floor(phi + 0.5f))) - 1.0f;
                break;
            case 2: // Fűrészfog Fel
                raw = 2.0f * (phi - std::floor(phi)) - 1.0f;
                break;
            case 3: // Fűrészfog Le
                raw = 1.0f - 2.0f * (phi - std::floor(phi));
                break;
            case 4: // Négyszög
                raw = (phi < 0.5f) ? 1.0f : -1.0f;
                break;
            default:
                raw = std::sin(2.0f * 3.14159265f * phi);
                break;
            }
            float val = std::clamp(raw * cfg_.depth + cfg_.offset, -1.0f, 1.0f);
            float y = mid_y - val * (mid_y * 0.85f);
            if (x == 0) path.moveTo(x, y);
            else path.lineTo(x, y);
        }

        p.setPen(QPen(color_, 2));
        p.drawPath(path);

        // Draw current real-time dot marker
        if (decks_) {
            const double now_sec = static_cast<double>(os_gettime_ns() / 1000000ULL) * 0.001;
            const double bpm = decks_->effectiveMasterBpm();
            const float rate_beats = getLfoRateBeats(cfg_.rate_idx);
            const double effective_bpm = (bpm > 20.0) ? bpm : 120.0;
            const double total_beats = now_sec * (effective_bpm / 60.0);
            double cur_phi = std::fmod((total_beats / static_cast<double>(rate_beats)) + (static_cast<double>(cfg_.phase_deg) / 360.0), 1.0);
            if (cur_phi < 0.0) cur_phi += 1.0;

            float cur_val = (lfo_idx_ == 0) ? decks_->currentLfo1Value() : decks_->currentLfo2Value();
            float dot_x = static_cast<float>(cur_phi) * w;
            float dot_y = mid_y - cur_val * (mid_y * 0.85f);

            p.setBrush(QColor(255, 255, 255));
            p.setPen(QPen(color_, 2));
            p.drawEllipse(QPointF(dot_x, dot_y), 4.0, 4.0);
        }
    }

private:
    int lfo_idx_ = 0;
    DeckPair *decks_ = nullptr;
    QColor color_;
    LfoConfig cfg_;
};

// -----------------------------------------------------------------------------
// LfoTab Implementation
// -----------------------------------------------------------------------------
LfoTab::LfoTab(DeckPair *decks, QWidget *parent)
    : QWidget(parent), decks_(decks)
{
    setupUi();

    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (scope1_) scope1_->update();
        if (scope2_) scope2_->update();
    });
    refresh_timer_->start(30); // ~33 FPS smooth oscilloscope
}

LfoTab::~LfoTab()
{
    if (refresh_timer_) {
        refresh_timer_->stop();
        delete refresh_timer_;
    }
}

void LfoTab::setTimerActive(bool active)
{
    if (!refresh_timer_) return;
    if (active && !refresh_timer_->isActive()) {
        refresh_timer_->start(30);
    } else if (!active && refresh_timer_->isActive()) {
        refresh_timer_->stop();
    }
}

void LfoTab::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(6);

    // Title & Reset Row
    auto *hdr_layout = new QHBoxLayout();
    hdr_layout->setContentsMargins(0, 0, 0, 0);

    auto *title_lbl = new QLabel(QStringLiteral("BPM-SZINKRONIZÁLT LFO VEZÉRLÉS"), this);
    title_lbl->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #00d2ff;"));
    hdr_layout->addWidget(title_lbl, 1);

    auto *reset_btn = new QPushButton(QStringLiteral("↺ Reset LFO"), this);
    reset_btn->setFixedHeight(20);
    reset_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f2733; color: #a0b0c8; border: 1px solid #2d3848; border-radius: 3px; font-weight: bold; font-size: 9px; padding: 1px 6px; } "
        "QPushButton:hover { background: #2b3648; color: #00d2ff; }"));
    connect(reset_btn, &QPushButton::clicked, this, [this]() {
        if (!decks_) return;
        decks_->setLfo1Config({0, 6, 1.0f, 0.0f, 0.0f});
        decks_->setLfo2Config({1, 4, 1.0f, 0.0f, 90.0f});
        refresh();
    });
    hdr_layout->addWidget(reset_btn);

    main_layout->addLayout(hdr_layout);

    // LFO 1 Section (Ciánkék)
    main_layout->addWidget(createLfoSection(0, QStringLiteral("〰 LFO 1 (ELSŐDLEGES / MASTER)"), QStringLiteral("#00d2ff")));

    // LFO 2 Section (Arany/Narancs)
    main_layout->addWidget(createLfoSection(1, QStringLiteral("∿ LFO 2 (MÁSODLAGOS / ELLENFÁZIS)"), QStringLiteral("#ffaa00")));

    main_layout->addStretch(1);

    refresh();
}

QWidget *LfoTab::createLfoSection(int lfo_idx, const QString &title, const QString &color_hex)
{
    auto *box = new QFrame(this);
    box->setStyleSheet(QStringLiteral(
        "QFrame { background: #131822; border: 1px solid #222d3d; border-radius: 4px; padding: 4px; }"));
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    // Title Row
    auto *title_row = new QHBoxLayout();
    auto *lbl = new QLabel(title, box);
    lbl->setStyleSheet(QStringLiteral("font-size: 9px; font-weight: bold; color: %1;").arg(color_hex));
    title_row->addWidget(lbl);
    title_row->addStretch(1);
    layout->addLayout(title_row);

    // Controls Row 1: Waveform & Rate
    auto *ctrl_row1 = new QHBoxLayout();
    ctrl_row1->setSpacing(6);

    auto *wf_lbl = new QLabel(QStringLiteral("Hullám:"), box);
    wf_lbl->setStyleSheet(QStringLiteral("font-size: 8px; color: #8da0b8; font-weight: bold;"));
    ctrl_row1->addWidget(wf_lbl);

    auto *wf_combo = new QComboBox(box);
    wf_combo->setFixedHeight(20);
    wf_combo->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1a222f; color: #fff; border: 1px solid #2d3e56; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 2px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #12171f; color: #fff; selection-background-color: #006699; }"));
    for (int i = 0; i < 5; ++i) {
        wf_combo->addItem(QString::fromUtf8(getLfoWaveformName(i)), i);
    }
    ctrl_row1->addWidget(wf_combo, 1);

    auto *rate_lbl = new QLabel(QStringLiteral("Rate:"), box);
    rate_lbl->setStyleSheet(QStringLiteral("font-size: 8px; color: #8da0b8; font-weight: bold;"));
    ctrl_row1->addWidget(rate_lbl);

    auto *rate_combo = new QComboBox(box);
    rate_combo->setFixedHeight(20);
    rate_combo->setStyleSheet(QStringLiteral(
        "QComboBox { background: #1a222f; color: #fff; border: 1px solid #2d3e56; font-size: 8px; font-weight: bold; padding: 1px 4px; border-radius: 2px; } "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background: #12171f; color: #fff; selection-background-color: #006699; }"));
    for (int i = 0; i < 11; ++i) {
        rate_combo->addItem(QString::fromUtf8(getLfoRateName(i)), i);
    }
    ctrl_row1->addWidget(rate_combo, 1);

    layout->addLayout(ctrl_row1);

    // Controls Row 2: Sliders & Scope
    auto *split_layout = new QHBoxLayout();
    split_layout->setSpacing(6);

    auto *sliders_vbox = new QVBoxLayout();
    sliders_vbox->setSpacing(3);

    auto makeSliderRow = [this, box, color_hex](const QString &label, float min_v, float max_v, float step,
                                                auto getter, auto setter, auto formatter) {
        auto *row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);

        auto *l = new QLabel(label, box);
        l->setFixedWidth(42);
        l->setStyleSheet(QStringLiteral("font-size: 8px; color: #a0b0c8; font-weight: bold;"));
        row->addWidget(l);

        auto *val_edit = new QLineEdit(box);
        val_edit->setFixedWidth(38);
        val_edit->setFixedHeight(16);
        val_edit->setAlignment(Qt::AlignCenter);
        val_edit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #0c1017; color: %1; border: 1px solid #253549; border-radius: 2px; font-family: monospace; font-size: 8px; font-weight: bold; } "
            "QLineEdit:focus { border: 1px solid %1; background: #121924; color: #fff; }").arg(color_hex));
        row->addWidget(val_edit);

        auto *slider = new QSlider(Qt::Horizontal, box);
        slider->setRange(0, 1000);
        slider->setFixedHeight(14);
        slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 3px; background: #202b3a; border-radius: 1px; } "
            "QSlider::handle:horizontal { background: %1; width: 8px; margin: -3px 0; border-radius: 4px; }").arg(color_hex));
        row->addWidget(slider, 1);

        auto updateUi = [slider, val_edit, getter, formatter, min_v, max_v]() {
            float val = getter();
            float norm = (max_v > min_v) ? ((val - min_v) / (max_v - min_v)) : 0.0f;
            norm = std::clamp(norm, 0.0f, 1.0f);
            const QSignalBlocker b(slider);
            slider->setValue(static_cast<int>(norm * 1000.0f));
            val_edit->setText(formatter(val));
        };

        connect(slider, &QSlider::valueChanged, this, [slider, setter, val_edit, formatter, min_v, max_v](int iv) {
            float norm = static_cast<float>(iv) / 1000.0f;
            float val = min_v + norm * (max_v - min_v);
            setter(val);
            val_edit->setText(formatter(val));
        });

        connect(val_edit, &QLineEdit::editingFinished, this, [val_edit, setter, updateUi]() {
            QString s = val_edit->text().remove(QLatin1Char('%')).remove(QStringLiteral("°")).trimmed();
            bool ok = false;
            float parsed = s.toFloat(&ok);
            if (ok) setter(parsed);
            updateUi();
            val_edit->clearFocus();
        });

        updateUi();
        return row;
    };

    // Depth: 0% .. 100%
    sliders_vbox->addLayout(makeSliderRow(
        QStringLiteral("Depth:"), 0.0f, 1.0f, 0.05f,
        [this, lfo_idx]() { return decks_ ? ((lfo_idx == 0) ? decks_->lfo1Config().depth : decks_->lfo2Config().depth) : 1.0f; },
        [this, lfo_idx](float v) {
            if (!decks_) return;
            LfoConfig cfg = (lfo_idx == 0) ? decks_->lfo1Config() : decks_->lfo2Config();
            cfg.depth = std::clamp(v, 0.0f, 1.0f);
            if (lfo_idx == 0) { decks_->setLfo1Config(cfg); if (scope1_) scope1_->setConfig(cfg); }
            else { decks_->setLfo2Config(cfg); if (scope2_) scope2_->setConfig(cfg); }
        },
        [](float v) { return QStringLiteral("%1%").arg(static_cast<int>(std::round(v * 100.0f))); }
    ));

    // Offset: -1.00 .. +1.00
    sliders_vbox->addLayout(makeSliderRow(
        QStringLiteral("Offset:"), -1.0f, 1.0f, 0.05f,
        [this, lfo_idx]() { return decks_ ? ((lfo_idx == 0) ? decks_->lfo1Config().offset : decks_->lfo2Config().offset) : 0.0f; },
        [this, lfo_idx](float v) {
            if (!decks_) return;
            LfoConfig cfg = (lfo_idx == 0) ? decks_->lfo1Config() : decks_->lfo2Config();
            cfg.offset = std::clamp(v, -1.0f, 1.0f);
            if (lfo_idx == 0) { decks_->setLfo1Config(cfg); if (scope1_) scope1_->setConfig(cfg); }
            else { decks_->setLfo2Config(cfg); if (scope2_) scope2_->setConfig(cfg); }
        },
        [](float v) { return QString::number(v, 'f', 2); }
    ));

    // Phase: 0° .. 360°
    sliders_vbox->addLayout(makeSliderRow(
        QStringLiteral("Fázis:"), 0.0f, 360.0f, 15.0f,
        [this, lfo_idx]() { return decks_ ? ((lfo_idx == 0) ? decks_->lfo1Config().phase_deg : decks_->lfo2Config().phase_deg) : 0.0f; },
        [this, lfo_idx](float v) {
            if (!decks_) return;
            LfoConfig cfg = (lfo_idx == 0) ? decks_->lfo1Config() : decks_->lfo2Config();
            cfg.phase_deg = std::clamp(v, 0.0f, 360.0f);
            if (lfo_idx == 0) { decks_->setLfo1Config(cfg); if (scope1_) scope1_->setConfig(cfg); }
            else { decks_->setLfo2Config(cfg); if (scope2_) scope2_->setConfig(cfg); }
        },
        [](float v) { return QStringLiteral("%1°").arg(static_cast<int>(std::round(v))); }
    ));

    split_layout->addLayout(sliders_vbox, 1);

    // Mini Live Oscilloscope
    auto *scope = new LfoScopeWidget(lfo_idx, decks_, QColor(color_hex), box);
    if (lfo_idx == 0) scope1_ = scope;
    else scope2_ = scope;
    split_layout->addWidget(scope);

    layout->addLayout(split_layout);

    // Wire Waveform & Rate Combo updates
    connect(wf_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, lfo_idx](int idx) {
        if (!decks_) return;
        LfoConfig cfg = (lfo_idx == 0) ? decks_->lfo1Config() : decks_->lfo2Config();
        cfg.waveform = idx;
        if (lfo_idx == 0) { decks_->setLfo1Config(cfg); if (scope1_) scope1_->setConfig(cfg); }
        else { decks_->setLfo2Config(cfg); if (scope2_) scope2_->setConfig(cfg); }
    });

    connect(rate_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, lfo_idx](int idx) {
        if (!decks_) return;
        LfoConfig cfg = (lfo_idx == 0) ? decks_->lfo1Config() : decks_->lfo2Config();
        cfg.rate_idx = idx;
        if (lfo_idx == 0) { decks_->setLfo1Config(cfg); if (scope1_) scope1_->setConfig(cfg); }
        else { decks_->setLfo2Config(cfg); if (scope2_) scope2_->setConfig(cfg); }
    });

    return box;
}

void LfoTab::refresh()
{
    if (!decks_) return;
    if (scope1_) scope1_->setConfig(decks_->lfo1Config());
    if (scope2_) scope2_->setConfig(decks_->lfo2Config());
}

void vdj_open_lfo_dialog()
{
    extern void vdj_open_vj_fx_dialog_tab(int tab_idx, int target_deck);
    vdj_open_vj_fx_dialog_tab(2, 0);
}

void vdj_free_lfo_dialog()
{
    // A közös dialógus felszabadítása végzi
}
