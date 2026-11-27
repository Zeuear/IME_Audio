#pragma once
#include <complex>
#include <vector>

// @brief
// MDX-Net 用的 STFT / ISTFT，与 torch.stft(center=True, 周期 Hann) / istft 等价（模型按此训练，窗口或补边变了会失真）。
// 内部缓冲复用，非线程安全：每线程一个实例。
namespace MdxDsp {

class Stft {
public:
    Stft(int nFft, int hop);
    ~Stft();
    Stft(const Stft&) = delete;
    Stft& operator=(const Stft&) = delete;

    int bins() const { return m_nFft / 2 + 1; }
    int frameCount(int samples) const { return 1 + samples / m_hop; }

    void forward(const float* x, int n, std::vector<std::complex<float>>& out);
    void inverse(const std::complex<float>* spec, int frames, int length, float* out);

private:
    int m_nFft;
    int m_hop;
    void* m_fwd = nullptr;
    void* m_inv = nullptr;
    std::vector<float> m_window;
    std::vector<float> m_frame;
    std::vector<float> m_ola;
    std::vector<float> m_norm;
};

}
