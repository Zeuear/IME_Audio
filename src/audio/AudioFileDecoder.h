#pragma once
#include <QByteArray>
#include <QString>
#include <atomic>
#include <vector>

// @brief 用 FFmpeg（avformat + avcodec + swresample）把音视频文件解成内部统一的 Int16 / 单声道 PCM。
// QAudioDecoder 逐帧经事件循环交付，1 小时 m4a 要解约 5 分钟；这里在调用线程同步解完（不到 5 秒），调用方放进 QtConcurrent::run。
// 复用 Qt 自带的 FFmpeg（见 cmake/FFmpeg.cmake），找不到时 isAvailable() 为 false，调用方回退 QAudioDecoder。
namespace AudioFileDecoder {

struct Result {
    QByteArray pcm;
    QString error;
};

// 双声道浮点（-1..1），人声分离模型的输入格式
struct StereoResult {
    std::vector<float> left;
    std::vector<float> right;
    QString error;
};

bool isAvailable();

Result decode(const QString& path, int targetSampleRate, const std::atomic_bool* cancel = nullptr);

// 解成指定采样率的双声道 float。单声道源两个声道相同
StereoResult decodeStereo(const QString& path, int sampleRate, const std::atomic_bool* cancel = nullptr);

}
