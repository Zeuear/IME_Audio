#include "MdxDsp.h"

#include <cmath>
#include <cstring>
#include "kiss_fftr.h"

namespace MdxDsp {

namespace {

// 反射补边：与 torch 的 pad_mode="reflect" 一致，边缘样本不重复
int reflectIndex(int i, int n)
{
    if (n <= 1) return 0;
    if (i < 0) i = -i;
    if (i >= n) i = 2 * (n - 1) - i;
    return i < 0 ? 0 : i;
}

}

Stft::Stft(int nFft, int hop) : m_nFft(nFft), m_hop(hop)
{
    m_fwd = kiss_fftr_alloc(nFft, 0, nullptr, nullptr);
    m_inv = kiss_fftr_alloc(nFft, 1, nullptr, nullptr);
    m_window.resize(nFft);
    // 周期 Hann（分母是 nFft 而不是 nFft-1），torch.hann_window 的默认形式
    for (int i = 0; i < nFft; ++i)
        m_window[i] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265358979f * float(i) / float(nFft));
    m_frame.resize(nFft);
}

Stft::~Stft()
{
    kiss_fftr_free(static_cast<kiss_fftr_cfg>(m_fwd));
    kiss_fftr_free(static_cast<kiss_fftr_cfg>(m_inv));
}

void Stft::forward(const float* x, int n, std::vector<std::complex<float>>& out)
{
    const int frames = frameCount(n);
    const int nb = bins();
    const int pad = m_nFft / 2;
    out.resize(size_t(frames) * nb);

    std::vector<kiss_fft_cpx> freq(nb);
    for (int t = 0; t < frames; ++t) {
        const int start = t * m_hop - pad;
        for (int i = 0; i < m_nFft; ++i)
            m_frame[i] = x[reflectIndex(start + i, n)] * m_window[i];
        kiss_fftr(static_cast<kiss_fftr_cfg>(m_fwd), m_frame.data(), freq.data());
        std::complex<float>* dst = out.data() + size_t(t) * nb;
        for (int k = 0; k < nb; ++k) dst[k] = {freq[k].r, freq[k].i};
    }
}

void Stft::inverse(const std::complex<float>* spec, int frames, int length, float* out)
{
    const int nb = bins();
    const int pad = m_nFft / 2;
    const size_t total = size_t(frames - 1) * m_hop + m_nFft;
    m_ola.assign(total, 0.0f);
    m_norm.assign(total, 0.0f);

    std::vector<kiss_fft_cpx> freq(nb);
    const float scale = 1.0f / float(m_nFft);   // kissfft 的反变换不做归一化
    for (int t = 0; t < frames; ++t) {
        const std::complex<float>* src = spec + size_t(t) * nb;
        for (int k = 0; k < nb; ++k) { freq[k].r = src[k].real(); freq[k].i = src[k].imag(); }
        kiss_fftri(static_cast<kiss_fftr_cfg>(m_inv), freq.data(), m_frame.data());
        float* ola = m_ola.data() + size_t(t) * m_hop;
        float* norm = m_norm.data() + size_t(t) * m_hop;
        for (int i = 0; i < m_nFft; ++i) {
            ola[i] += m_frame[i] * scale * m_window[i];
            norm[i] += m_window[i] * m_window[i];
        }
    }

    // 窗口平方和接近 0 的位置只出现在最两端，除以下限避免放大噪声
    for (int i = 0; i < length; ++i) {
        const size_t p = size_t(i) + pad;
        out[i] = p < total ? m_ola[p] / std::fmax(m_norm[p], 1e-8f) : 0.0f;
    }
}

}
