#include "video-converter.hpp"

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

VideoConverter::VideoConverter(QObject *parent)
    : QObject(parent)
{
    process_ = new QProcess(this);
    connect(process_, &QProcess::readyReadStandardError, this, &VideoConverter::onProcessReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoConverter::onProcessFinished);
}

VideoConverter::~VideoConverter()
{
    cancel();
}

QString VideoConverter::findFfmpegBinary()
{
    // 1. Check OBS plugin config directory (where high-speed NVENC build is deployed)
    const QString appdata = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString candidate1 = QDir(appdata).filePath(QStringLiteral("obs-studio/plugin_config/obs-vj-mixer/ffmpeg.exe"));
    if (QFileInfo::exists(candidate1)) {
        return candidate1;
    }

    // 2. Also check AppConfigLocation directly
    const QString roaming = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString candidate2 = QDir(roaming).filePath(QStringLiteral("obs-studio/plugin_config/obs-vj-mixer/ffmpeg.exe"));
    if (QFileInfo::exists(candidate2)) {
        return candidate2;
    }

    // 3. Check system PATH
    const QString path_exec = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!path_exec.isEmpty()) {
        return path_exec;
    }

    // 4. Typical Windows installation paths
    const QStringList common_paths = {
        QStringLiteral("C:/ffmpeg/bin/ffmpeg.exe"),
        QStringLiteral("C:/Program Files/ffmpeg/bin/ffmpeg.exe"),
        QStringLiteral("C:/Program Files/obs-studio/bin/64bit/ffmpeg.exe")
    };
    for (const auto &p : common_paths) {
        if (QFileInfo::exists(p)) {
            return p;
        }
    }

    return QString();
}

bool VideoConverter::isFfmpegAvailable()
{
    return !findFfmpegBinary().isEmpty();
}

bool VideoConverter::convertTo1080p(const QString &input_file, const QString &output_dir)
{
    if (is_converting_) {
        return false;
    }

    const QString ffmpeg = findFfmpegBinary();
    if (ffmpeg.isEmpty() || !QFileInfo::exists(input_file)) {
        return false;
    }

    QDir().mkpath(output_dir);

    const QFileInfo fi(input_file);
    QString out_name = fi.completeBaseName() + QStringLiteral("_vj1080.mp4");
    QString out_path = QDir(output_dir).filePath(out_name);

    if (QFileInfo(out_path).absoluteFilePath() == fi.absoluteFilePath()) {
        out_name = fi.completeBaseName() + QStringLiteral("_vj_conv.mp4");
        out_path = QDir(output_dir).filePath(out_name);
    }

    current_input_path_ = input_file;
    current_output_path_ = out_path;
    total_duration_sec_ = 0.0;
    is_converting_ = true;
    is_trying_nvenc_ = true;

    // Fast GPU accelerated conversion (RTX 3060 NVENC)
    // 1920x1080 aspect ratio preserving scale + black bar padding + 100% All-Intra (-g 1)
    QStringList args;
    args << QStringLiteral("-y");
    args << QStringLiteral("-hwaccel") << QStringLiteral("cuda");
    args << QStringLiteral("-i") << input_file;
    args << QStringLiteral("-vf") << QStringLiteral("scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2:black");
    args << QStringLiteral("-c:v") << QStringLiteral("h264_nvenc");
    args << QStringLiteral("-preset") << QStringLiteral("p7");
    args << QStringLiteral("-tune") << QStringLiteral("hq");
    args << QStringLiteral("-g") << QStringLiteral("1");
    args << QStringLiteral("-c:a") << QStringLiteral("aac");
    args << QStringLiteral("-b:a") << QStringLiteral("320k");
    args << QStringLiteral("-ar") << QStringLiteral("48000");
    args << out_path;

    startWithArguments(args);
    return true;
}

void VideoConverter::startWithArguments(const QStringList &args)
{
    const QString ffmpeg = findFfmpegBinary();
    process_->start(ffmpeg, args);
}

void VideoConverter::cancel()
{
    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(1000);
    }
    is_converting_ = false;
}

void VideoConverter::onProcessReadyRead()
{
    const QString output = QString::fromUtf8(process_->readAllStandardError());

    // 1. Parse Duration: 00:04:12.34
    if (total_duration_sec_ <= 0.0) {
        static const QRegularExpression dur_regex(QStringLiteral("Duration:\\s*(\\d+):(\\d+):(\\d+\\.?\\d*)"));
        const auto match = dur_regex.match(output);
        if (match.hasMatch()) {
            const double h = match.captured(1).toDouble();
            const double m = match.captured(2).toDouble();
            const double s = match.captured(3).toDouble();
            total_duration_sec_ = (h * 3600.0) + (m * 60.0) + s;
        }
    }

    // 2. Parse current time=00:01:23.45
    static const QRegularExpression time_regex(QStringLiteral("time=(\\d+):(\\d+):(\\d+\\.?\\d*)"));
    const auto match = time_regex.match(output);
    if (match.hasMatch() && total_duration_sec_ > 0.0) {
        const double h = match.captured(1).toDouble();
        const double m = match.captured(2).toDouble();
        const double s = match.captured(3).toDouble();
        const double cur_sec = (h * 3600.0) + (m * 60.0) + s;
        const int pct = std::clamp(static_cast<int>((cur_sec / total_duration_sec_) * 100.0), 0, 100);
        emit progressChanged(pct);
    }
}

void VideoConverter::onProcessFinished(int exit_code, QProcess::ExitStatus exit_status)
{
    if (exit_code != 0 && is_trying_nvenc_) {
        // Fallback to CPU libx264 if NVENC fails
        is_trying_nvenc_ = false;
        QStringList args;
        args << QStringLiteral("-y");
        args << QStringLiteral("-i") << current_input_path_;
        args << QStringLiteral("-vf") << QStringLiteral("scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2:black");
        args << QStringLiteral("-c:v") << QStringLiteral("libx264");
        args << QStringLiteral("-preset") << QStringLiteral("veryfast");
        args << QStringLiteral("-crf") << QStringLiteral("18");
        args << QStringLiteral("-g") << QStringLiteral("1");
        args << QStringLiteral("-c:a") << QStringLiteral("aac");
        args << QStringLiteral("-b:a") << QStringLiteral("320k");
        args << QStringLiteral("-ar") << QStringLiteral("48000");
        args << current_output_path_;

        startWithArguments(args);
        return;
    }

    is_converting_ = false;
    const bool success = (exit_code == 0 && exit_status == QProcess::NormalExit);
    emit progressChanged(success ? 100 : 0);
    emit conversionFinished(current_output_path_, success, success ? QString() : QStringLiteral("Conversion error (code %1)").arg(exit_code));
}
