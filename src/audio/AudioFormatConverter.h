#pragma once
#include <QAudioFormat>
#include <QByteArray>

// @brief 把采集设备给出的任意格式音频，转成项目内部统一的 Int16 / 单声道 / 指定采样率 PCM。
//
// 设备不支持我们请求的格式时，QAudioSource 会回退到设备的 preferredFormat
// （macOS 内置麦克风常见 48 kHz / Float / 立体声）。把这些字节直接当 16 kHz Int16 单声道
// 交给 VAD 和识别器，得到的只会是乱码。
namespace AudioFormatConverter {

// 设备实际格式已经是内部格式时返回 true，调用方可以跳过转换
bool isInternalFormat(const QAudioFormat& format, int targetSampleRate);

// 解码 → 混成单声道 → 重采样到 targetSampleRate → Int16
QByteArray toInternalPcm(const QByteArray& chunk, const QAudioFormat& sourceFormat, int targetSampleRate);

}
