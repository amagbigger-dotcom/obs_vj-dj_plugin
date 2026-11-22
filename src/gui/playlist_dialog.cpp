#include "gui/playlist_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>

PlaylistDialog::PlaylistDialog(QWidget *parent,
                               const QString &initial_name,
                               const QString &initial_genre,
                               const QString &initial_keyword)
    : QDialog(parent)
{
    const bool is_edit = !initial_name.isEmpty();
    setWindowTitle(is_edit ? QStringLiteral("Lejátszási Lista Szerkesztése")
                           : QStringLiteral("Új Lejátszási Lista Létrehozása"));
    setFixedSize(360, 240);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #12151c; color: #e0e4ec; font-family: 'Segoe UI', Arial, sans-serif; }"
        "QLabel { color: #cfd6e4; font-size: 11px; font-weight: bold; }"
        "QLineEdit { background: #181d26; color: #ffffff; border: 1px solid #2e3e54; border-radius: 4px; padding: 6px 10px; font-size: 12px; }"
        "QLineEdit:focus { border: 1px solid #00d2ff; }"
        "QPushButton { background: #222834; border: 1px solid #384255; border-radius: 4px; color: #ffffff; padding: 6px 14px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background: #2f384a; border-color: #00d2ff; }"
        "QPushButton:pressed { background: #181d26; }"
    ));

    setupUi(initial_name, initial_genre, initial_keyword);
}

void PlaylistDialog::setupUi(const QString &name, const QString &genre, const QString &keyword)
{
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(10);

    // Name input
    auto *lbl_name = new QLabel(QStringLiteral("Lista Neve (kötelező):"), this);
    main_layout->addWidget(lbl_name);

    name_edit_ = new QLineEdit(name, this);
    name_edit_->setPlaceholderText(QStringLiteral("pl. Warmup Melodic, Festival Hits, 90s..."));
    main_layout->addWidget(name_edit_);

    // Genre input
    auto *lbl_genre = new QLabel(QStringLiteral("Műfaji / Hangulati Besorolás (opcionális):"), this);
    main_layout->addWidget(lbl_genre);

    genre_edit_ = new QLineEdit(genre, this);
    genre_edit_->setPlaceholderText(QStringLiteral("pl. Tech House, Trance, DnB, Klipek..."));
    main_layout->addWidget(genre_edit_);

    // Smart filter keyword
    auto *lbl_kw = new QLabel(QStringLiteral("Okos Szűrő Kulcsszó vagy BPM (opcionális):"), this);
    main_layout->addWidget(lbl_kw);

    keyword_edit_ = new QLineEdit(keyword, this);
    keyword_edit_->setPlaceholderText(QStringLiteral("pl. remix, vocal, vagy '128' BPM..."));
    keyword_edit_->setToolTip(QStringLiteral("Ha megadod, a lista automatikusan tartalmazni fog minden olyan dalt is, aminek a címében/útvonalában szerepel a szó."));
    main_layout->addWidget(keyword_edit_);

    main_layout->addSpacing(6);

    // Bottom action buttons
    auto *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(10);

    btn_cancel_ = new QPushButton(QStringLiteral("Mégse"), this);
    connect(btn_cancel_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(btn_cancel_);

    btn_ok_ = new QPushButton(QStringLiteral("💾 Mentés"), this);
    btn_ok_->setStyleSheet(QStringLiteral(
        "QPushButton { background: #0088cc; border: 1px solid #00aaff; color: #ffffff; padding: 6px 14px; font-weight: bold; border-radius: 4px; }"
        "QPushButton:hover { background: #00aaff; }"
    ));
    connect(btn_ok_, &QPushButton::clicked, this, [this]() {
        if (name_edit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Hiányzó Név"), QStringLiteral("Kérlek adj meg egy nevet a listának!"));
            name_edit_->setFocus();
            return;
        }
        accept();
    });
    btn_layout->addWidget(btn_ok_);

    main_layout->addLayout(btn_layout);
}

QString PlaylistDialog::playlistName() const
{
    return name_edit_ ? name_edit_->text().trimmed() : QString();
}

QString PlaylistDialog::playlistGenre() const
{
    return genre_edit_ ? genre_edit_->text().trimmed() : QString();
}

QString PlaylistDialog::filterKeyword() const
{
    return keyword_edit_ ? keyword_edit_->text().trimmed() : QString();
}
