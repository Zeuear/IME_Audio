#pragma once
#include <QAudioFormat>
#include <QByteArray>

// @brief 把采集设备给出的任意格式音频，转成项目内部统一的 Int16 / 单声道 / 指定采样率 PCM。
// 设备不支持请求的格式时 QAudioSource 会回退到 preferredFormat（macOS 内置麦克风常见 48 kHz / Float / 立体声），直接当 16 kHz Int16 用只会得到乱码。
namespace AudioFormatConverter {

bool isInternalFormat(const QAudioFormat& format, int targetSampleRate);

QByteArray toInternalPcm(const QByteArray& chunk, const QAudioFormat& sourceFormat, int targetSampleRate);

}
