#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// AdaptiveSpectrumController
// -----------------------------------------------------------------------
// 分频段自适应量程控制器：每个频段独立追踪自己的 ceil/floor，
// 并叠加基于真实物理频率的心理声学权重（模拟语音可懂度分布），
// 让可视化结果呈现"人声核心频段更突出、两端更弱"的效果。
//
// 输出 = pow(归一化形状, gamma) * 心理声学权重 * 全局响度增益
// 三者分别负责：频段相对强弱 / 频率重要性塑形 / 绝对音量感知
// -----------------------------------------------------------------------
class AdaptiveSpectrumController {
public:
    struct BandConfig {
        float attackMs;
        float releaseMs;
        float sensitivity;
        float gamma;
    };

    struct BandState {
        float ceilDb = -20.0f;
        float floorDb = -60.0f;
        float runningMean = -40.0f;
        float runningVar = 10.0f;
        int warmUpFrames = 0;
    };

    // bandCount:     频段数量
    // frameRateHz:   实际调用频率 = sampleRate / hopSize（不是采样率本身！）
    // bandCenterHzs: 每个频段对应的真实物理中心频率(Hz)，用于心理声学权重计算
    AdaptiveSpectrumController(int bandCount, float frameRateHz,
        const std::vector<float>& bandCenterHzs)
        : m_bandCount(bandCount), m_frameRateHz(frameRateHz)
    {
        m_states.resize(bandCount);
        m_configs.resize(bandCount);
        m_outputBuffer.resize(bandCount);
        m_weightMask.resize(bandCount);

        setupDefaultConfigs();
        setupPsychoacousticMask(bandCenterHzs);
    }

    const std::vector<float>& process(const std::vector<float>& inputDbs, float globalRmsLevel)
    {
        static constexpr float kAbsoluteFloorDb = -70.0f; // 绝对静音门限
        static constexpr float kAbsoluteFloorClamp = -90.0f; // floorDb 的硬下限保护

        for (int i = 0; i < m_bandCount; ++i) {
            float currentDb = inputDbs[i];
            const auto& cfg = m_configs[i];
            auto& state = m_states[i];

            // 1. 活跃判定：完全独立于视觉权重，只看统计学特征
            bool statisticallyActive = (state.warmUpFrames >= 30) &&
                (currentDb > (state.runningMean + cfg.sensitivity * std::sqrt(state.runningVar)));
            bool physicallyActive = (currentDb > kAbsoluteFloorDb);
            bool isActive = statisticallyActive && physicallyActive;

            // 2. 只在"不活跃"（背景/静音）时更新统计基准，
            //    避免 runningMean 被说话内容自己带偏，导致小声说话逐渐"消失"
            if (!isActive) {
                updateStatistics(state, currentDb);
                if (state.warmUpFrames < 30) state.warmUpFrames++;
            }

            // 3. 分频段包络追踪
            if (isActive) {
                float alphaAttack = calculateAlpha(cfg.attackMs);
                state.ceilDb += alphaAttack * (currentDb - state.ceilDb);

                // 地板追随长期背景均值，而不是瞬时值，
                // 避免持续说话把地板拖高、压缩内部动态范围
                float alphaFloor = calculateAlpha(cfg.releaseMs * 2.0f);
                state.floorDb += alphaFloor * (state.runningMean - state.floorDb);
            }
            else {
                float alphaCeilFall = calculateAlpha(cfg.releaseMs * 5.0f);
                state.ceilDb += alphaCeilFall * (currentDb - state.ceilDb);

                float alphaFloorFall = calculateAlpha(cfg.releaseMs);
                state.floorDb += alphaFloorFall * (currentDb - state.floorDb);
            }

            // 4. 安全保护：最小动态范围 + 绝对下限，防止反转/失控下探
            const float minRange = 20.0f;
            if (state.ceilDb - state.floorDb < minRange) {
                state.ceilDb = state.floorDb + minRange;
            }
            state.floorDb = std::max(kAbsoluteFloorClamp, state.floorDb);

            // 5. 归一化 + Gamma 校正
            float range = state.ceilDb - state.floorDb;
            float normalized = (currentDb - state.floorDb) / std::max(range, 1.0f);
            normalized = std::clamp(normalized, 0.0f, 1.0f);

            // 6. 叠加心理声学权重(塑形) + 全局响度(保留绝对音量感)
            m_outputBuffer[i] = std::pow(normalized, cfg.gamma) * m_weightMask[i] * globalRmsLevel;
        }

        return m_outputBuffer;
    }

private:
    void updateStatistics(BandState& state, float val)
    {
        const float learningRate = 0.001f;
        state.runningMean = (1.0f - learningRate) * state.runningMean + learningRate * val;
        float diff = val - state.runningMean;
        state.runningVar = (1.0f - learningRate) * state.runningVar + learningRate * (diff * diff);
    }

    float calculateAlpha(float ms) const
    {
        if (ms <= 0.0f) return 1.0f;
        float tau = ms / 1000.0f;
        float interval = 1.0f / m_frameRateHz;
        return 1.0f - std::exp(-interval / tau);
    }

    // 基于真实物理频率(Hz)的心理声学权重曲线。
    // peakHz 必须落在你实际频段覆盖范围之内，否则整条曲线会
    // 退化成单调递增/递减，起不到"中间凸起两边低"的效果——
    // 这要求调用方的 voiceMinHz/voiceMaxHz 范围要把 peakHz 包含在内
    // (建议范围覆盖到至少 3000~4000Hz，参考语音可懂度 SII 核心区)。
    void setupPsychoacousticMask(const std::vector<float>& bandCenterHzs)
    {
        const float peakHz = 1500.0f;   // 语音可懂度贡献最大的中心频率附近
        const float widthHz = 1200.0f;  // 控制钟形曲线宽度

        for (int i = 0; i < m_bandCount; ++i) {
            float hz = std::max(bandCenterHzs[i], 1.0f); // 防止 log2(0)
            float logDist = std::log2(hz / peakHz);
            float sigma = std::log2((peakHz + widthHz) / peakHz);

            m_weightMask[i] = std::exp(-(logDist * logDist) / (2.0f * sigma * sigma));
            m_weightMask[i] = std::max(m_weightMask[i], 0.15f); 
        }
    }

    void setupDefaultConfigs()
    {
        for (int i = 0; i < m_bandCount; ++i) {
            if (i < m_bandCount / 3) {
                m_configs[i] = { 15.0f, 400.0f, 1.8f, 1.2f }; // 低频：较迟钝，防止低频抖动
            }
            else if (i < (2 * m_bandCount) / 3) {
                m_configs[i] = { 10.0f, 150.0f, 0.6f, 1.4f }; // 中频（人声核心）：灵敏、快速响应
            }
            else {
                m_configs[i] = { 30.0f, 80.0f, 1.2f, 1.8f };  // 高频：灵动
            }
        }
    }

    int m_bandCount;
    float m_frameRateHz;
    std::vector<BandConfig> m_configs;
    std::vector<BandState> m_states;
    std::vector<float> m_outputBuffer;
    std::vector<float> m_weightMask;
};
