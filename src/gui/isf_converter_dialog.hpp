#pragma once

#include <QDialog>
#include <QString>
#include <vector>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class VjFxDialog;

class IsfConverterDialog final : public QDialog {
    Q_OBJECT

public:
    explicit IsfConverterDialog(VjFxDialog *vj_dialog, QWidget *parent = nullptr);
    ~IsfConverterDialog() override = default;

    void refreshExistingList();

protected:
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onCopyPromptClicked();
    void onPasteClipboardClicked();
    void onTestCompileClicked();
    void onSaveShaderClicked();
    void onDeleteShaderClicked();
    void onExistingSelectionChanged(int index);

private:
    void setupUi();
    void loadShaderToEditor(const QString &shader_id);

    VjFxDialog *vj_dialog_ = nullptr;

    QComboBox *existing_combo_ = nullptr;
    QLineEdit *name_edit_ = nullptr;
    QLineEdit *category_edit_ = nullptr;
    QPlainTextEdit *code_edit_ = nullptr;
    QLabel *status_lbl_ = nullptr;

    QPushButton *copy_prompt_btn_ = nullptr;
    QPushButton *paste_btn_ = nullptr;
    QPushButton *test_btn_ = nullptr;
    QPushButton *save_btn_ = nullptr;
    QPushButton *delete_btn_ = nullptr;
    QPushButton *close_btn_ = nullptr;

    QString current_shader_id_;
};

void vdj_open_isf_converter_dialog(VjFxDialog *vj_dialog);
void vdj_free_isf_converter_dialog();
