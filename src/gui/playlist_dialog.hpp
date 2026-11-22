#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class PlaylistDialog : public QDialog {
    Q_OBJECT

public:
    explicit PlaylistDialog(QWidget *parent = nullptr,
                            const QString &initial_name = QString(),
                            const QString &initial_genre = QString(),
                            const QString &initial_keyword = QString());
    ~PlaylistDialog() override = default;

    QString playlistName() const;
    QString playlistGenre() const;
    QString filterKeyword() const;

private:
    void setupUi(const QString &name, const QString &genre, const QString &keyword);

    QLineEdit *name_edit_ = nullptr;
    QLineEdit *genre_edit_ = nullptr;
    QLineEdit *keyword_edit_ = nullptr;
    QPushButton *btn_ok_ = nullptr;
    QPushButton *btn_cancel_ = nullptr;
};
