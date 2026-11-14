#pragma once

#include <QObject>
#include <QString>
#include <QProcess>

class VideoConverter : public QObject {
    Q_OBJECT

public:
    explicit VideoConverter(QObject *parent = nullptr);
    ~VideoConverter() override;

    static QString findFfmpegBinary();
    static bool isFfmpegAvailable();

    bool convertTo1080p(const QString &input_file, const QString &output_dir);
    void cancel();
    bool isConverting() const { return is_converting_; }
    QString currentOutputPath() const { return current_output_path_; }

signals:
    void progressChanged(int percent);
    void conversionFinished(const QString &output_path, bool success, const QString &error_msg);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exit_code, QProcess::ExitStatus exit_status);

private:
    void startWithArguments(const QStringList &args);

    QProcess *process_ = nullptr;
    bool is_converting_ = false;
    bool is_trying_nvenc_ = true;
    QString current_input_path_{};
    QString current_output_path_{};
    double total_duration_sec_ = 0.0;
};
