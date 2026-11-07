#pragma once
#include <QByteArray>

// @brief 按整句峰值把偏小的语音放大到统一电平，只放大不衰减。
namespace AudioNormalizer {

// 目标峰值（占满量程的比例）和增益上限（倍数，20 倍约 26 dB）
inline constexpr double kTargetPeak = 0.7;
inline constexpr double kMaxGain = 20.0;
// RMS 低于此值（约 -56 dBFS）视为底噪，放大只会放大噪声并诱发幻觉文字
inline constexpr double kNoiseFloorRms = 50.0;

// 输入为 16-bit 单声道 PCM
QByteArray normalizePcm16(const QByteArray& pcm, double* gainOut = nullptr);

}
