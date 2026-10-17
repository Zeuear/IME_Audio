#pragma once
#include <QByteArray>
#include <QString>
#include <atomic>

// @brief 用 FFmpeg（avformat + avcodec + swresample）把音视频文件解成内部统一的 Int16 / 单声道 PCM。
//
// QAudioDecoder 把每一帧都经事件循环交付，1 小时的 m4a 要解约 5 分钟；这里在调用线程里一口气解完，
// 同一文件不到 5 秒。同步阻塞，调用方放进 QtConcurrent::run。
// 目前只在 Windows 启用（复用 Qt 自带的 FFmpeg DLL，见 third_party/ffmpeg/README.md），
// 其它平台 isAvailable() 返回 false，由调用方回退到 QAudioDecoder。
namespace AudioFileDecoder {

struct Result {
    QByteArray pcm;
    QString error;   // 非空表示失败；中文，可直接展示给用户
};

bool isAvailable();

// cancel 被置 true 时尽快返回（pcm 为空、error 为"已取消"）
Result decode(const QString& path, int targetSampleRate, const std::atomic_bool* cancel = nullptr);

}
