#include "gui/isf_converter_dialog.hpp"
#include "gui/vj_fx_dialog.hpp"
#include "effects/custom_shader_manager.hpp"
#include <obs-frontend-api.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QScreen>
#include <QKeyEvent>
#include <QFontDatabase>
#include <QFileDialog>
#include <QFile>

namespace {

static IsfConverterDialog *g_isf_dialog = nullptr;

constexpr const char *DIALOG_STYLESHEET = R"(
QDialog {
    background-color: #0f131a;
    color: #d6e0ee;
    font-family: 'Segoe UI', Arial, sans-serif;
}
QFrame#CardFrame {
    background-color: #141923;
    border: 1px solid #232e40;
    border-radius: 6px;
    padding: 10px;
}
QLabel {
    color: #cfd8e6;
    font-size: 11px;
}
QLineEdit {
    background-color: #0b0e14;
    color: #00d2ff;
    border: 1px solid #2a374c;
    border-radius: 4px;
    padding: 5px 8px;
    font-size: 11px;
    font-weight: bold;
}
QLineEdit:focus {
    border-color: #00d2ff;
}
QPlainTextEdit {
    background-color: #07090d;
    color: #bbf2f6;
    border: 1px solid #1c2738;
    border-radius: 4px;
    padding: 8px;
    font-family: 'Consolas', 'Courier New', monospace;
    font-size: 11px;
    line-height: 1.4;
}
QPlainTextEdit:focus {
    border-color: #00aaff;
}
QComboBox {
    background-color: #1a2332;
    color: #ffffff;
    border: 1px solid #2a374c;
    border-radius: 4px;
    padding: 4px 8px;
    font-weight: bold;
    font-size: 11px;
}
QComboBox QAbstractItemView {
    background-color: #141a24;
    color: #ffffff;
    selection-background-color: #0088cc;
}
QPushButton {
    background-color: #1e2838;
    color: #ffffff;
    border: 1px solid #2d3c52;
    border-radius: 4px;
    padding: 6px 12px;
    font-weight: bold;
    font-size: 11px;
}
QPushButton:hover {
    background-color: #27354a;
    border-color: #00b4d8;
}
)";

} // namespace

IsfConverterDialog::IsfConverterDialog(VjFxDialog *vj_dialog, QWidget *parent)
    : QDialog(parent), vj_dialog_(vj_dialog)
{
    g_isf_dialog = this;
    setWindowTitle(QStringLiteral("OBS VJ Shader Studio - ISF Shader Beágyazó Motor"));
    resize(780, 680);
    setStyleSheet(QString::fromUtf8(DIALOG_STYLESHEET));

    setupUi();
    refreshExistingList();
}

void IsfConverterDialog::setupUi()
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(14, 12, 14, 12);
    main_layout->setSpacing(10);

    // 1. Header Card (Title & Instructions)
    auto *header_card = new QFrame(this);
    header_card->setObjectName(QStringLiteral("CardFrame"));
    auto *header_layout = new QVBoxLayout(header_card);
    header_layout->setContentsMargins(10, 8, 10, 8);
    header_layout->setSpacing(6);

    auto *top_row = new QHBoxLayout();
    auto *title_lbl = new QLabel(QStringLiteral("🪄 ISF & GLSL SHADER BEÁGYAZÓ MOTOR (Direct3D 11 / HLSL)"), header_card);
    title_lbl->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #00d2ff; letter-spacing: 0.5px;"));
    top_row->addWidget(title_lbl, 1);

    copy_prompt_btn_ = new QPushButton(QStringLiteral("📋 AI Prompt Másolása Vágólapra"), header_card);
    copy_prompt_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #005f73, stop:1 #0a9396); "
        "border: 1px solid #94d2bd; color: #ffffff; font-weight: bold; font-size: 11px; padding: 5px 12px; border-radius: 4px; } "
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0a9396, stop:1 #00b4d8); border-color: #00d2ff; }"
    ));
    copy_prompt_btn_->setAutoDefault(false);
    copy_prompt_btn_->setDefault(false);
    copy_prompt_btn_->setCursor(Qt::PointingHandCursor);
    connect(copy_prompt_btn_, &QPushButton::clicked, this, &IsfConverterDialog::onCopyPromptClicked);
    top_row->addWidget(copy_prompt_btn_);

    header_layout->addLayout(top_row);

    auto *desc_lbl = new QLabel(
        QStringLiteral("⚡ Közvetlen ISF & GLSL Konverter: Másold ki a kódot az editor.isf.video vagy shadertoy.com oldalról, vagy tallózz be egy .fs fájlt!\n"
                       "A beépített C++ motor azonnal Direct3D 11 HLSL kódra fordítja a forrást, felparaméterezi a 8 VJ csúszkát és lefordítja a GPU-n."),
        header_card
    );
    desc_lbl->setStyleSheet(QStringLiteral("font-size: 10px; color: #8da4be; line-height: 1.3;"));
    header_layout->addWidget(desc_lbl);

    main_layout->addWidget(header_card);

    // 2. Select Existing or Create New
    auto *select_row = new QHBoxLayout();
    select_row->setSpacing(6);

    auto *select_lbl = new QLabel(QStringLiteral("Szerkesztés:"), this);
    select_lbl->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #8da4be;"));
    select_row->addWidget(select_lbl);

    existing_combo_ = new QComboBox(this);
    existing_combo_->setFixedHeight(26);
    connect(existing_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &IsfConverterDialog::onExistingSelectionChanged);
    select_row->addWidget(existing_combo_, 1);

    browse_fs_btn_ = new QPushButton(QStringLiteral("📂 .fs Fájl Tallózása"), this);
    browse_fs_btn_->setFixedHeight(26);
    browse_fs_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c2e42; color: #64dfdf; border: 1px solid #2d4f6e; font-weight: bold; font-size: 11px; padding: 3px 10px; border-radius: 4px; } "
        "QPushButton:hover { background: #263e59; border-color: #48cae4; color: #ffffff; }"
    ));
    browse_fs_btn_->setAutoDefault(false);
    browse_fs_btn_->setDefault(false);
    browse_fs_btn_->setCursor(Qt::PointingHandCursor);
    connect(browse_fs_btn_, &QPushButton::clicked, this, &IsfConverterDialog::onBrowseFsFileClicked);
    select_row->addWidget(browse_fs_btn_);

    convert_isf_btn_ = new QPushButton(QStringLiteral("⚡ ISF Átalakítás"), this);
    convert_isf_btn_->setFixedHeight(26);
    convert_isf_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #104868, stop:1 #0088cc); "
        "border: 1px solid #00b4d8; color: #ffffff; font-weight: bold; font-size: 11px; padding: 3px 10px; border-radius: 4px; } "
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #14608c, stop:1 #00aaff); border-color: #00d2ff; }"
    ));
    convert_isf_btn_->setAutoDefault(false);
    convert_isf_btn_->setDefault(false);
    convert_isf_btn_->setCursor(Qt::PointingHandCursor);
    connect(convert_isf_btn_, &QPushButton::clicked, this, &IsfConverterDialog::onConvertIsfClicked);
    select_row->addWidget(convert_isf_btn_);

    paste_btn_ = new QPushButton(QStringLiteral("📋 Vágólap Beillesztése"), this);
    paste_btn_->setFixedHeight(26);
    paste_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1e2838; color: #ffffff; border: 1px solid #2d3c52; font-weight: bold; font-size: 11px; padding: 3px 10px; border-radius: 4px; } "
        "QPushButton:hover { background: #27354a; border-color: #00b4d8; }"
    ));
    paste_btn_->setAutoDefault(false);
    paste_btn_->setDefault(false);
    paste_btn_->setCursor(Qt::PointingHandCursor);
    connect(paste_btn_, &QPushButton::clicked, this, &IsfConverterDialog::onPasteClipboardClicked);
    select_row->addWidget(paste_btn_);

    main_layout->addLayout(select_row);

    // 3. Name & Category Row
    auto *meta_row = new QHBoxLayout();
    meta_row->setSpacing(10);

    auto *name_layout = new QVBoxLayout();
    name_layout->setSpacing(2);
    auto *name_title = new QLabel(QStringLiteral("Shader Neve:"), this);
    name_title->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #8da4be;"));
    name_layout->addWidget(name_title);
    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText(QStringLiteral("pl. Cyber Tunnel 3D"));
    name_layout->addWidget(name_edit_);
    meta_row->addLayout(name_layout, 2);

    auto *cat_layout = new QVBoxLayout();
    cat_layout->setSpacing(2);
    auto *cat_title = new QLabel(QStringLiteral("Kategória / Stílus:"), this);
    cat_title->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #8da4be;"));
    cat_layout->addWidget(cat_title);
    category_edit_ = new QLineEdit(this);
    category_edit_->setPlaceholderText(QStringLiteral("pl. ISF / Raymarching"));
    cat_layout->addWidget(category_edit_);
    meta_row->addLayout(cat_layout, 1);

    main_layout->addLayout(meta_row);

    // 4. Code Editor
    auto *code_title_row = new QHBoxLayout();
    auto *code_title = new QLabel(QStringLiteral("Direct3D 11 HLSL Kód vagy AI Blokkok ([NAME], [PARAMS], [CODE]):"), this);
    code_title->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #8da4be;"));
    code_title_row->addWidget(code_title);
    main_layout->addLayout(code_title_row);

    code_edit_ = new QPlainTextEdit(this);
    code_edit_->setLineWrapMode(QPlainTextEdit::NoWrap);
    code_edit_->setPlaceholderText(QStringLiteral(
        "[NAME]\nCyber Tunnel 3D\n\n"
        "[CATEGORY]\nISF / Tunnel\n\n"
        "[PARAMS]\n"
        "p0 | Rate | 0.1 | 4.0 | 1.0 | 0.05\n"
        "p1 | Zoom | 0.2 | 5.0 | 1.0 | 0.05\n"
        "p2 | Detail | 1.0 | 10.0 | 3.0 | 0.5\n"
        "p3 | Symmetry | 2.0 | 16.0 | 6.0 | 1.0\n"
        "p4 | Pulse | 0.0 | 2.0 | 0.5 | 0.05\n"
        "p5 | Glow | 0.1 | 3.0 | 1.0 | 0.05\n"
        "p6 | Color Shift | 0.0 | 1.0 | 0.0 | 0.02\n"
        "p7 | Contrast | 0.5 | 3.0 | 1.2 | 0.05\n\n"
        "[CODE]\n"
        "float4 render_custom(float2 uv)\n"
        "{\n"
        "    float2 uvn = (uv - 0.5) * float2(16.0 / 9.0, 1.0);\n"
        "    float3 theme = get_base_theme_color();\n"
        "    // ... HLSL logika ...\n"
        "    return float4(saturate(col), 1.0);\n"
        "}"
    ));
    main_layout->addWidget(code_edit_, 1);

    // 5. Status / Compiler Log
    status_lbl_ = new QLabel(QStringLiteral("Állapot: Kész a kód beillesztésére és tesztelésére."), this);
    status_lbl_->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #7f95ac; padding: 4px;"));
    status_lbl_->setWordWrap(true);
    main_layout->addWidget(status_lbl_);

    // 6. Action Button Bar
    auto *btn_bar = new QHBoxLayout();
    btn_bar->setSpacing(8);

    delete_btn_ = new QPushButton(QStringLiteral("🗑️ Shader Törlése"), this);
    delete_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3d1b1f; color: #ff6b6b; border: 1px solid #5a262c; } "
        "QPushButton:hover { background: #5c2027; border-color: #ff4d4d; color: #ffffff; }"
    ));
    delete_btn_->setAutoDefault(false);
    delete_btn_->setDefault(false);
    delete_btn_->setEnabled(false);
    connect(delete_btn_, &QPushButton::clicked, this, &IsfConverterDialog::onDeleteShaderClicked);
    btn_bar->addWidget(delete_btn_);

    btn_bar->addStretch(1);

    test_btn_ = new QPushButton(QStringLiteral("⚡ Tesztelés & Hibaellenőrzés"), this);
    test_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1c3248; color: #00d2ff; border: 1px solid #2a5275; } "
        "QPushButton:hover { background: #234464; border-color: #00e5ff; color: #ffffff; }"
    ));
    test_btn_->setAutoDefault(false);
    test_btn_->setDefault(false);
    test_btn_->setCursor(Qt::PointingHandCursor);
    connect(test_btn_, &QPushButton::clicked, this, &IsfConverterDialog::onTestCompileClicked);
    btn_bar->addWidget(test_btn_);

    save_btn_ = new QPushButton(QStringLiteral("💾 Beágyazás & Mentés"), this);
    save_btn_->setStyleSheet(QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0077b6, stop:1 #00b4d8); "
        "border: 1px solid #48cae4; color: #ffffff; font-weight: bold; font-size: 11px; padding: 6px 16px; border-radius: 4px; } "
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0096c7, stop:1 #48cae4); border-color: #90e0ef; }"
    ));
    save_btn_->setAutoDefault(false);
    save_btn_->setDefault(false);
    save_btn_->setCursor(Qt::PointingHandCursor);
    connect(save_btn_, &QPushButton::clicked, this, &IsfConverterDialog::onSaveShaderClicked);
    btn_bar->addWidget(save_btn_);

    close_btn_ = new QPushButton(QStringLiteral("Bezárás"), this);
    close_btn_->setAutoDefault(false);
    close_btn_->setDefault(false);
    connect(close_btn_, &QPushButton::clicked, this, &QDialog::close);
    btn_bar->addWidget(close_btn_);

    main_layout->addLayout(btn_bar);
}

void IsfConverterDialog::refreshExistingList()
{
    existing_combo_->blockSignals(true);
    existing_combo_->clear();

    existing_combo_->addItem(QStringLiteral("➕ [ Új ISF Shader Hozzáadása ]"), QString());

    const auto &customs = CustomShaderManager::instance().customShaders();
    for (size_t i = 0; i < customs.size(); ++i) {
        const auto &cs = customs[i];
        const int display_id = static_cast<int>(16 + i); // 1-based, 16..
        existing_combo_->addItem(
            QStringLiteral("%1. %2 (%3)").arg(display_id).arg(QString::fromStdString(cs.name)).arg(QString::fromStdString(cs.category)),
            QString::fromStdString(cs.id)
        );
    }

    if (!current_shader_id_.isEmpty()) {
        int idx = existing_combo_->findData(current_shader_id_);
        if (idx >= 0) {
            existing_combo_->setCurrentIndex(idx);
        } else {
            existing_combo_->setCurrentIndex(0);
            current_shader_id_.clear();
        }
    } else {
        existing_combo_->setCurrentIndex(0);
    }

    delete_btn_->setEnabled(!current_shader_id_.isEmpty());
    existing_combo_->blockSignals(false);
}

void IsfConverterDialog::onExistingSelectionChanged(int index)
{
    if (index <= 0) {
        current_shader_id_.clear();
        name_edit_->clear();
        category_edit_->clear();
        code_edit_->clear();
        delete_btn_->setEnabled(false);
        status_lbl_->setText(QStringLiteral("Állapot: Készen áll egy új ISF shader kódjának beillesztésére."));
        status_lbl_->setStyleSheet(QStringLiteral("color: #7f95ac; font-weight: bold;"));
        return;
    }

    const QString id = existing_combo_->itemData(index).toString();
    loadShaderToEditor(id);
}

void IsfConverterDialog::loadShaderToEditor(const QString &shader_id)
{
    current_shader_id_ = shader_id;
    const auto &customs = CustomShaderManager::instance().customShaders();
    for (const auto &cs : customs) {
        if (QString::fromStdString(cs.id) == shader_id) {
            name_edit_->setText(QString::fromStdString(cs.name));
            category_edit_->setText(QString::fromStdString(cs.category));
            if (!cs.raw_user_code.empty()) {
                code_edit_->setPlainText(QString::fromStdString(cs.raw_user_code));
            } else {
                code_edit_->setPlainText(QString::fromStdString(cs.hlsl_code));
            }
            delete_btn_->setEnabled(true);
            status_lbl_->setText(QStringLiteral("Kiválasztva: %1. Módosíthatod a kódot vagy törölheted a shadert.")
                                     .arg(QString::fromStdString(cs.name)));
            status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold;"));
            return;
        }
    }
}

void IsfConverterDialog::onBrowseFsFileClicked()
{
    const QString file_path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("ISF Shader Fájl Betöltése"),
        QString(),
        QStringLiteral("ISF Shader fájlok (*.fs *.txt *.glsl);;Minden fájl (*.*)")
    );

    if (file_path.isEmpty()) return;

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        status_lbl_->setText(QStringLiteral("❌ Nem sikerült megnyitni a fájlt: %1").arg(file_path));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ff4d4d; font-weight: bold;"));
        return;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    std::string name, cat, code, err;
    std::vector<VjParamDef> params;
    std::vector<std::string> p_names, p_ids;

    if (CustomShaderManager::instance().transpileIsfToHlsl(content.toStdString(), name, cat, params, p_names, p_ids, code, err)) {
        name_edit_->setText(QString::fromStdString(name));
        category_edit_->setText(QString::fromStdString(cat));
        code_edit_->setPlainText(QString::fromStdString(code));

        std::string compile_err;
        if (CustomShaderManager::instance().testCompileShader(code, compile_err)) {
            status_lbl_->setText(QStringLiteral("✓ SIKERES ISF BETÖLTÉS & TESZT! \"%1\" (%2) lefordult a GPU-n. Kattints a \"Beágyazás & Mentés\" gombra!")
                                     .arg(QString::fromStdString(name))
                                     .arg(QString::fromStdString(cat)));
            status_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-weight: bold;"));
        } else {
            status_lbl_->setText(QStringLiteral("⚠️ ISF átalakítva, de a GPU teszt fordításkor hiba lépett fel:\n%1")
                                     .arg(QString::fromStdString(compile_err)));
            status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
        }
    } else {
        code_edit_->setPlainText(content);
        status_lbl_->setText(QStringLiteral("❌ ISF Átalakítási Hiba: %1").arg(QString::fromStdString(err)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ff4d4d; font-weight: bold;"));
    }
}

void IsfConverterDialog::onConvertIsfClicked()
{
    const QString text = code_edit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        status_lbl_->setText(QStringLiteral("⚠️ A kódmező üres! Illessz be egy ISF vagy GLSL kódot."));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
        return;
    }

    std::string name, cat, code, err;
    std::vector<VjParamDef> params;
    std::vector<std::string> p_names, p_ids;

    if (CustomShaderManager::instance().transpileIsfToHlsl(text.toStdString(), name, cat, params, p_names, p_ids, code, err)) {
        name_edit_->setText(QString::fromStdString(name));
        category_edit_->setText(QString::fromStdString(cat));
        code_edit_->setPlainText(QString::fromStdString(code));

        std::string compile_err;
        if (CustomShaderManager::instance().testCompileShader(code, compile_err)) {
            status_lbl_->setText(QStringLiteral("✓ SIKERES ISF ÁTALAKÍTÁS & TESZT! \"%1\" (%2) lefordult a GPU-n. Kattints a \"Beágyazás & Mentés\" gombra!")
                                     .arg(QString::fromStdString(name))
                                     .arg(QString::fromStdString(cat)));
            status_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-weight: bold;"));
        } else {
            status_lbl_->setText(QStringLiteral("⚠️ ISF átalakítva, de a GPU teszt fordításkor hiba lépett fel:\n%1")
                                     .arg(QString::fromStdString(compile_err)));
            status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
        }
    } else {
        status_lbl_->setText(QStringLiteral("❌ ISF Átalakítási Hiba: %1").arg(QString::fromStdString(err)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ff4d4d; font-weight: bold;"));
    }
}

void IsfConverterDialog::onCopyPromptClicked()
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(CustomShaderManager::getAiPromptTemplate());
        status_lbl_->setText(QStringLiteral("✓ Az AI Prompt sablon a vágólapra másolva! Add át a ChatGPT-nek, Claude-nak vagy Gemini-nek!"));
        status_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-weight: bold;"));
    }
}

void IsfConverterDialog::onPasteClipboardClicked()
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (!cb) return;

    const QString text = cb->text();
    if (text.isEmpty()) {
        status_lbl_->setText(QStringLiteral("⚠️ A vágólap üres! Másold ki először az ISF vagy GLSL kódot."));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
        return;
    }

    // Auto-parse parameters and name
    std::string name, cat, code, err;
    std::vector<VjParamDef> params;
    std::vector<std::string> p_names, p_ids;

    if (CustomShaderManager::instance().parseAiShaderText(text.toStdString(), name, cat, params, p_names, p_ids, code, err)) {
        name_edit_->setText(QString::fromStdString(name));
        category_edit_->setText(QString::fromStdString(cat));
        code_edit_->setPlainText(QString::fromStdString(code));

        std::string compile_err;
        if (CustomShaderManager::instance().testCompileShader(code, compile_err)) {
            status_lbl_->setText(QStringLiteral("✓ SIKERES ÁTALAKÍTÁS & TESZT! \"%1\" (%2) lefordult a GPU-n. Kattints a \"Beágyazás & Mentés\" gombra!")
                                     .arg(QString::fromStdString(name))
                                     .arg(QString::fromStdString(cat)));
            status_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-weight: bold;"));
        } else {
            status_lbl_->setText(QStringLiteral("⚠️ Átalakítva, de a GPU teszt fordításkor hiba lépett fel:\n%1")
                                     .arg(QString::fromStdString(compile_err)));
            status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
        }
    } else {
        code_edit_->setPlainText(text);
        status_lbl_->setText(QStringLiteral("⚠️ Kód beillesztve, de a címkék elemzésekor figyelmeztetés lépett fel: %1").arg(QString::fromStdString(err)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
    }
}

void IsfConverterDialog::onTestCompileClicked()
{
    const QString text = code_edit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        status_lbl_->setText(QStringLiteral("⚠️ A kódmező üres! Illessz be egy shadert a teszteléshez."));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
        return;
    }

    std::string name, cat, code, err;
    std::vector<VjParamDef> params;
    std::vector<std::string> p_names, p_ids;

    if (!CustomShaderManager::instance().parseAiShaderText(text.toStdString(), name, cat, params, p_names, p_ids, code, err)) {
        status_lbl_->setText(QStringLiteral("❌ Elemzési Hiba: %1").arg(QString::fromStdString(err)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ff4d4d; font-weight: bold;"));
        return;
    }

    // Update UI fields with parsed name, category, and transpiled HLSL code
    if (name_edit_->text().trimmed().isEmpty() || name_edit_->text().trimmed() == QStringLiteral("Új ISF Shader")) {
        name_edit_->setText(QString::fromStdString(name));
    }
    if (category_edit_->text().trimmed().isEmpty() || category_edit_->text().trimmed() == QStringLiteral("ISF / Procedural")) {
        category_edit_->setText(QString::fromStdString(cat));
    }
    code_edit_->setPlainText(QString::fromStdString(code));

    status_lbl_->setText(QStringLiteral("⚡ Fordítás tesztelése a GPU Direct3D 11 motorján..."));
    status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold;"));

    std::string compile_err;
    if (CustomShaderManager::instance().testCompileShader(code, compile_err)) {
        status_lbl_->setText(QStringLiteral("✓ SIKERES TESZT! A(z) \"%1\" hibátlanul lefordult a Direct3D 11 GPU-n. Kattints a \"Beágyazás & Mentés\" gombra!")
                                 .arg(name_edit_->text().trimmed()));
        status_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-weight: bold;"));
    } else {
        status_lbl_->setText(QStringLiteral("❌ FORDÍTÁSI HIBA:\n%1").arg(QString::fromStdString(compile_err)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ff4d4d; font-weight: bold;"));
    }
}

void IsfConverterDialog::onSaveShaderClicked()
{
    const QString text = code_edit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        status_lbl_->setText(QStringLiteral("⚠️ Nem menthető üres kód!"));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ffb703; font-weight: bold;"));
        return;
    }

    std::string name, cat, code, err;
    std::vector<VjParamDef> params;
    std::vector<std::string> p_names, p_ids;

    if (!CustomShaderManager::instance().parseAiShaderText(text.toStdString(), name, cat, params, p_names, p_ids, code, err)) {
        status_lbl_->setText(QStringLiteral("❌ Elemzési Hiba: %1").arg(QString::fromStdString(err)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ff4d4d; font-weight: bold;"));
        return;
    }

    // Allow user override from QLineEdit fields
    const QString custom_name = name_edit_->text().trimmed();
    if (!custom_name.isEmpty()) {
        name = custom_name.toStdString();
    } else {
        name_edit_->setText(QString::fromStdString(name));
    }

    const QString custom_cat = category_edit_->text().trimmed();
    if (!custom_cat.isEmpty()) {
        cat = custom_cat.toStdString();
    } else {
        category_edit_->setText(QString::fromStdString(cat));
    }

    code_edit_->setPlainText(QString::fromStdString(code));

    std::string out_id, out_err;
    const bool ok = CustomShaderManager::instance().addOrUpdateCustomShader(
        current_shader_id_.toStdString(),
        name, cat, params, p_names, p_ids, code, text.toStdString(),
        out_id, out_err
    );

    if (ok) {
        current_shader_id_ = QString::fromStdString(out_id);
        refreshExistingList();
        if (vj_dialog_) {
            vj_dialog_->reloadShaderList();
        }
        status_lbl_->setText(QStringLiteral("✓ SIKERES BEÁGYAZÁS & MENTÉS! \"%1\" azonnal bekerült a választható VJ Shaderek közé.")
                                 .arg(QString::fromStdString(name)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #00ffaa; font-weight: bold;"));
    } else {
        status_lbl_->setText(QStringLiteral("❌ Mentési hiba: %1").arg(QString::fromStdString(out_err)));
        status_lbl_->setStyleSheet(QStringLiteral("color: #ff4d4d; font-weight: bold;"));
    }
}

void IsfConverterDialog::onDeleteShaderClicked()
{
    if (current_shader_id_.isEmpty()) return;

    auto res = QMessageBox::question(
        this,
        QStringLiteral("Shader Törlése"),
        QStringLiteral("Biztosan törölni szeretnéd ezt a beágyazott ISF shadert?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (res == QMessageBox::Yes) {
        CustomShaderManager::instance().deleteCustomShader(current_shader_id_.toStdString());
        current_shader_id_.clear();
        name_edit_->clear();
        category_edit_->clear();
        code_edit_->clear();
        refreshExistingList();
        if (vj_dialog_) {
            vj_dialog_->reloadShaderList();
        }
        status_lbl_->setText(QStringLiteral("✓ Shader sikeresen törölve."));
        status_lbl_->setStyleSheet(QStringLiteral("color: #00d2ff; font-weight: bold;"));
    }
}

void IsfConverterDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect geom = screen->availableGeometry();
        // Center dialog on screen
        const int x_pos = geom.left() + (geom.width() - width()) / 2;
        const int y_pos = geom.top() + (geom.height() - height()) / 2;
        move(std::max(0, x_pos), std::max(0, y_pos));
    }
}

void IsfConverterDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        // Allow newline in QPlainTextEdit
        if (code_edit_ && code_edit_->hasFocus()) {
            QDialog::keyPressEvent(event);
            return;
        }
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void vdj_open_isf_converter_dialog(VjFxDialog *vj_dialog)
{
    if (!g_isf_dialog) {
        QWidget *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
        g_isf_dialog = new IsfConverterDialog(vj_dialog, main_window);
    }
    g_isf_dialog->refreshExistingList();
    g_isf_dialog->show();
    g_isf_dialog->raise();
    g_isf_dialog->activateWindow();
}

void vdj_free_isf_converter_dialog()
{
    if (g_isf_dialog) {
        g_isf_dialog->close();
        delete g_isf_dialog;
        g_isf_dialog = nullptr;
    }
}
