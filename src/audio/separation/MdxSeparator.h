#pragma once
#include <QString>
#include <atomic>
#include <functional>
#include <vector>

// @brief
// 用 MDX-Net 模型（UVR-MDX-NET-Voc_FT）从混音里分离人声。同步阻塞，调用方放进 QtConcurrent::run。
// useCuda 时优先用 CUDA，GPU 会话建不起来就退回 CPU。模型要求 44.1 kHz 双声道输入。
namespace MdxSeparator {

constexpr int kSampleRate = 44100;

struct Stereo {
    std::vector<float> left;
    std::vector<float> right;
};

struct Result {
    Stereo vocals;
    QString error;
};

using Progress = std::function<void(int done, int total)>;

Result separateVocals(const QString& modelPath, const Stereo& mix, int threads, bool useCuda,
                      const std::atomic_bool* cancel = nullptr, const Progress& progress = {});

bool writeStereoWav(const QString& path, const Stereo& audio, int sampleRate);

}
