#pragma once
#include <complex>
#include <vector>

// @brief
// MDX-Net 用的 STFT / ISTFT，与 torch.stft(center=True, 周期 Hann) 及 torch.istft 等价，
// 因为模型是按这套频谱训练的，窗口、补边方式变了分离结果就会失真。
// 内部缓冲可复用，不是线程安全的：每个线程各用一个实例。
namespace MdxDsp {

class Stft {
public:
    Stft(int nFft, int hop);
    ~Stft();
    Stft(const Stft&) = delete;
    Stft& operator=(const Stft&) = delete;

    int bins() const { return m_nFft / 2 + 1; }
    int frameCount(int samples) const { return 1 + samples / m_hop; }

    // out 按 [frame][bin] 排列，大小 frameCount(n) * bins()
    void forward(const float* x, int n, std::vector<std::complex<float>>& out);
    // spec 按 [frame][bin] 排列；重建 length 个样本
    void inverse(const std::complex<float>* spec, int frames, int length, float* out);

private:
    int m_nFft;
    int m_hop;
    void* m_fwd = nullptr;   // kiss_fftr_cfg
    void* m_inv = nullptr;
    std::vector<float> m_window;
    std::vector<float> m_frame;
    std::vector<float> m_ola;
    std::vector<float> m_norm;
};

}
