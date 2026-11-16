#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>

class VideoConverter : public QObject {
    Q_OBJECT

public:
    explicit VideoConverter(QObject *parent = nullptr);
    ~VideoConverter() override;

    enum class TargetResolution {
        Auto,
        Res720p,
        Res1080p,
        Res1440p,
        Res4K
    };

    static QString findFfmpegBinary();
    static bool isFfmpegAvailable();

    bool convertVideo(const QString &input_file, const QString &output_dir, TargetResolution target_res = TargetResolution::Res1080p);
    bool convertTo1080p(const QString &input_file, const QString &output_dir) {
        return convertVideo(input_file, output_dir, TargetResolution::Res1080p);
    }
    void cancel();
    bool isConverting() const { return is_converting_; }
    QString currentOutputPath() const { return current_output_path_; }
    QString currentInputPath() const { return current_input_path_; }

signals:
    void progressChanged(int percent);
    void conversionFinished(const QString &output_path, bool success, const QString &error_msg);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exit_code, QProcess::ExitStatus exit_status);
    void checkWatchdog();

private:
    void startWithArguments(const QStringList &args);
    void startCpuFallback();

    QProcess *process_ = nullptr;
    QTimer *watchdog_timer_ = nullptr;
    bool is_converting_ = false;
    bool is_trying_nvenc_ = true;
    QString current_input_path_{};
    QString current_output_path_{};
    QString current_filter_{};
    double total_duration_sec_ = 0.0;
    qint64 last_activity_ms_ = 0;
};
