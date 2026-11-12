#pragma once
#include <QString>
#include <atomic>
#include <functional>
#include <vector>

// @brief
// 用 MDX-Net 模型（UVR-MDX-NET-Voc_FT）从混音里分离人声。同步阻塞，调用方放进 QtConcurrent::run。
// 只走 CPU。模型要求 44.1 kHz 双声道输入。
namespace MdxSeparator {

constexpr int kSampleRate = 44100;

struct Stereo {
    std::vector<float> left;
    std::vector<float> right;
};

struct Result {
    Stereo vocals;
    QString error;   // 非空表示失败；中文，可直接展示给用户
};

// progress(已完成块数, 总块数)，在调用线程里同步回调
using Progress = std::function<void(int done, int total)>;

Result separateVocals(const QString& modelPath, const Stereo& mix, int threads,
                      const std::atomic_bool* cancel = nullptr, const Progress& progress = {});

// 写 16-bit 双声道 WAV
bool writeStereoWav(const QString& path, const Stereo& audio, int sampleRate);

}
