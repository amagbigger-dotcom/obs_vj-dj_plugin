#include "video-converter.hpp"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <obs.h>
#include <algorithm>
#include <cmath>

VideoConverter::VideoConverter(QObject *parent)
    : QObject(parent)
{
    process_ = new QProcess(this);
    connect(process_, &QProcess::readyReadStandardError, this, &VideoConverter::onProcessReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoConverter::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        blog(LOG_WARNING, "[vj_converter] QProcess error: %d", static_cast<int>(err));
        if (is_trying_nvenc_) {
            is_trying_nvenc_ = false;
            startCpuFallback();
        } else {
            is_converting_ = false;
            emit progressChanged(0);
            emit conversionFinished(current_output_path_, false,
                QStringLiteral("Process error (code %1)").arg(static_cast<int>(err)));
        }
    });
}

VideoConverter::~VideoConverter()
{
    cancel();
}

QString VideoConverter::findFfmpegBinary()
{
    // 1. Check OBS Studio bin directory (where obs64.exe runs)
    const QString app_dir = QCoreApplication::applicationDirPath();
    if (!app_dir.isEmpty()) {
        const QString candidate0 = QDir(app_dir).filePath(QStringLiteral("ffmpeg.exe"));
        if (QFileInfo::exists(candidate0)) {
            return candidate0;
        }
    }

    // 2. Check OBS plugin config directory (%APPDATA%/obs-studio/plugin_config/obs-vj-mixer/ffmpeg.exe)
    const QString appdata = qEnvironmentVariable("APPDATA");
    if (!appdata.isEmpty()) {
        const QString candidate1 = QDir(appdata).filePath(QStringLiteral("obs-studio/plugin_config/obs-vj-mixer/ffmpeg.exe"));
        if (QFileInfo::exists(candidate1)) {
            return candidate1;
        }
    }

    // 3. Check Program Files 64-bit paths
    const QStringList common_paths = {
        QStringLiteral("C:/Program Files/obs-studio/bin/64bit/ffmpeg.exe"),
        QStringLiteral("C:/Program Files/obs-studio/obs-plugins/64bit/ffmpeg.exe"),
        QStringLiteral("C:/ffmpeg/bin/ffmpeg.exe"),
        QStringLiteral("C:/Program Files/ffmpeg/bin/ffmpeg.exe")
    };
    for (const auto &p : common_paths) {
        if (QFileInfo::exists(p)) {
            return p;
        }
    }

    // 4. Check system PATH
    const QString path_exec = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!path_exec.isEmpty()) {
        return path_exec;
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
    if (ffmpeg.isEmpty()) {
        blog(LOG_WARNING, "[vj_converter] Cannot start conversion: ffmpeg.exe not found!");
        emit conversionFinished(QString(), false, QStringLiteral("ffmpeg.exe nem található!"));
        return false;
    }
    if (!QFileInfo::exists(input_file)) {
        blog(LOG_WARNING, "[vj_converter] Input file does not exist: %s", input_file.toUtf8().constData());
        emit conversionFinished(QString(), false, QStringLiteral("Bemeneti fájl nem található!"));
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
    // 1920x1080 aspect ratio preserving scale + black bar padding + 100% All-Intra (-g 2 -bf 0)
    QStringList args;
    args << QStringLiteral("-y");
    args << QStringLiteral("-hwaccel") << QStringLiteral("cuda");
    args << QStringLiteral("-i") << input_file;
    args << QStringLiteral("-vf") << QStringLiteral("scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2:black");
    args << QStringLiteral("-c:v") << QStringLiteral("h264_nvenc");
    args << QStringLiteral("-preset") << QStringLiteral("p7");
    args << QStringLiteral("-tune") << QStringLiteral("hq");
    args << QStringLiteral("-rc") << QStringLiteral("vbr");
    args << QStringLiteral("-cq") << QStringLiteral("18");
    args << QStringLiteral("-b:v") << QStringLiteral("0");
    args << QStringLiteral("-g") << QStringLiteral("2");
    args << QStringLiteral("-bf") << QStringLiteral("0");
    args << QStringLiteral("-c:a") << QStringLiteral("aac");
    args << QStringLiteral("-b:a") << QStringLiteral("320k");
    args << QStringLiteral("-ar") << QStringLiteral("48000");
    args << out_path;

    startWithArguments(args);
    return true;
}

void VideoConverter::startCpuFallback()
{
    blog(LOG_INFO, "[vj_converter] Starting CPU libx264 fallback conversion");
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
}

void VideoConverter::startWithArguments(const QStringList &args)
{
    const QString ffmpeg = findFfmpegBinary();
    blog(LOG_INFO, "[vj_converter] Launching ffmpeg: %s with %d args", ffmpeg.toUtf8().constData(), args.size());
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
        startCpuFallback();
        return;
    }

    is_converting_ = false;
    const bool success = (exit_code == 0 && exit_status == QProcess::NormalExit);
    emit progressChanged(success ? 100 : 0);
    emit conversionFinished(current_output_path_, success, success ? QString() : QStringLiteral("Conversion error (code %1)").arg(exit_code));
}
