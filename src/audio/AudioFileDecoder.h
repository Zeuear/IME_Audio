#pragma once
#include <QByteArray>
#include <QString>
#include <atomic>
#include <vector>

// @brief 用 FFmpeg（avformat + avcodec + swresample）把音视频文件解成内部统一的 Int16 / 单声道 PCM。
//
// QAudioDecoder 把每一帧都经事件循环交付，1 小时的 m4a 要解约 5 分钟；这里在调用线程里一口气解完，
// 同一文件不到 5 秒。同步阻塞，调用方放进 QtConcurrent::run。
// 复用 Qt 自带的 FFmpeg 库（Windows 是 DLL，macOS / Linux 在 Qt 的 lib 目录，见 cmake/FFmpeg.cmake），
// 找不到时 isAvailable() 返回 false，由调用方回退到 QAudioDecoder。
namespace AudioFileDecoder {

struct Result {
    QByteArray pcm;
    QString error;   // 非空表示失败；中文，可直接展示给用户
};

// 双声道浮点（-1..1），人声分离模型的输入格式
struct StereoResult {
    std::vector<float> left;
    std::vector<float> right;
    QString error;
};

bool isAvailable();

// cancel 被置 true 时尽快返回（pcm 为空、error 为"已取消"）
Result decode(const QString& path, int targetSampleRate, const std::atomic_bool* cancel = nullptr);

// 解成指定采样率的双声道 float。单声道源两个声道相同
StereoResult decodeStereo(const QString& path, int sampleRate, const std::atomic_bool* cancel = nullptr);

}
