#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// 分频段自适应量程：每频段独立追踪 ceil/floor，叠加心理声学权重，突出人声核心频段。
// 输出 = pow(归一化形状, gamma) * 心理声学权重 * 全局响度增益
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

    // frameRateHz:   实际调用频率 = sampleRate / hopSize（不是采样率本身！）
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
        static constexpr float kAbsoluteFloorDb = -70.0f;
        static constexpr float kAbsoluteFloorClamp = -90.0f; // floorDb 的硬下限保护

        for (int i = 0; i < m_bandCount; ++i) {
            float currentDb = inputDbs[i];
            const auto& cfg = m_configs[i];
            auto& state = m_states[i];

            bool statisticallyActive = (state.warmUpFrames >= 30) &&
                (currentDb > (state.runningMean + cfg.sensitivity * std::sqrt(state.runningVar)));
            bool physicallyActive = (currentDb > kAbsoluteFloorDb);
            bool isActive = statisticallyActive && physicallyActive;

            // 只在不活跃（背景/静音）时更新统计基准，避免 runningMean 被说话内容带偏、小声说话逐渐消失
            if (!isActive) {
                updateStatistics(state, currentDb);
                if (state.warmUpFrames < 30) state.warmUpFrames++;
            }

            if (isActive) {
                float alphaAttack = calculateAlpha(cfg.attackMs);
                state.ceilDb += alphaAttack * (currentDb - state.ceilDb);

                // 地板追随长期背景均值而非瞬时值，避免持续说话把地板拖高
                float alphaFloor = calculateAlpha(cfg.releaseMs * 2.0f);
                state.floorDb += alphaFloor * (state.runningMean - state.floorDb);
            }
            else {
                float alphaCeilFall = calculateAlpha(cfg.releaseMs * 5.0f);
                state.ceilDb += alphaCeilFall * (currentDb - state.ceilDb);

                float alphaFloorFall = calculateAlpha(cfg.releaseMs);
                state.floorDb += alphaFloorFall * (currentDb - state.floorDb);
            }

            const float minRange = 20.0f;
            if (state.ceilDb - state.floorDb < minRange) {
                state.ceilDb = state.floorDb + minRange;
            }
            state.floorDb = std::max(kAbsoluteFloorClamp, state.floorDb);

            float range = state.ceilDb - state.floorDb;
            float normalized = (currentDb - state.floorDb) / std::max(range, 1.0f);
            normalized = std::clamp(normalized, 0.0f, 1.0f);

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

    // peakHz 必须落在频段覆盖范围内（建议覆盖到 3000~4000Hz），否则曲线退化为单调，没有中间凸起。
    void setupPsychoacousticMask(const std::vector<float>& bandCenterHzs)
    {
        const float peakHz = 1500.0f;
        const float widthHz = 1200.0f;

        for (int i = 0; i < m_bandCount; ++i) {
            float hz = std::max(bandCenterHzs[i], 1.0f);
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
                m_configs[i] = { 30.0f, 80.0f, 1.2f, 1.8f };
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
