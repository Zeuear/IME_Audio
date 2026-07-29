#include "miniaudio.h"

#include "AudioStreamRecorder.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <vector>

namespace {

    std::string deviceIdToHex(const ma_device_id& id) {
        std::ostringstream oss;
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&id);
        for (size_t i = 0; i < sizeof(ma_device_id); ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
        }
        return oss.str();
    }

    bool hexToDeviceId(const std::string& hex, ma_device_id& outId) {
        if (hex.size() != sizeof(ma_device_id) * 2) return false;
        unsigned char* bytes = reinterpret_cast<unsigned char*>(&outId);
        for (size_t i = 0; i < sizeof(ma_device_id); ++i) {
            std::string byteStr = hex.substr(i * 2, 2);
            bytes[i] = static_cast<unsigned char>(std::stoi(byteStr, nullptr, 16));
        }
        return true;
    }

}  // namespace

// -----------------------------------------------------------------------
// 关键设计变化：
// 1. 设备采集格式不再强制成 s16/16000/mono，而是让 miniaudio 使用设备
//    "原生格式"采集（就像你验证过能正常工作的那份 demo 代码一样），
//    这样完全绕开 WASAPI 层隐式的实时格式转换/重采样链路，拿到最干净的原始信号。
// 2. 拿到原生格式的音频后，自己用 ma_data_converter 做一次性、可控的
//    格式转换（原生格式 -> f32 -> 重采样到目标采样率 -> 声道混合到目标声道数
//    -> 转回 s16），转换质量和参数完全由我们自己掌控，而不是依赖 WASAPI 的
//    隐式行为（不同驱动/设备表现可能不一致）。
// -----------------------------------------------------------------------

struct AudioStreamRecorder::Impl {
    ma_context context{};
    ma_device device{};
    bool contextInited = false;
    bool deviceInited = false;
    bool running = false;
    std::atomic<bool> paused{ false };
    AudioChunkCallback callback;
    std::string lastError;

    // 设备原生格式（采集时实际生效的参数）
    ma_format nativeFormat = ma_format_unknown;
    uint32_t nativeSampleRate = 0;
    uint32_t nativeChannels = 0;

    // 目标输出格式（回调给业务层的格式，即 config 里请求的参数）
    uint32_t targetSampleRate = 16000;
    uint32_t targetChannels = 1;

    // 转换器：原生格式 -> 目标格式(s16)
    ma_data_converter converter{};
    bool converterInited = false;

    // 转换缓冲区，避免每次回调都重新分配内存
    std::vector<int16_t> convertedBuffer;
};

static void data_callback(ma_device* pDevice, void* /*pOutput*/, const void* pInput, ma_uint32 frameCount) {
    auto* impl = reinterpret_cast<AudioStreamRecorder::Impl*>(pDevice->pUserData);
    if (!impl || impl->paused.load() || !impl->callback || !pInput) return;

    if (!impl->converterInited) {
        return;
    }

    ma_uint64 frameCountOut = 0;
    ma_data_converter_get_expected_output_frame_count(&impl->converter, frameCount, &frameCountOut);

    size_t neededSamples = static_cast<size_t>(frameCountOut) * impl->targetChannels;
    if (impl->convertedBuffer.size() < neededSamples) {
        impl->convertedBuffer.resize(neededSamples);
    }

    ma_uint64 framesIn = frameCount;
    ma_uint64 framesOut = frameCountOut;
    ma_result r = ma_data_converter_process_pcm_frames(
        &impl->converter,
        pInput, &framesIn,
        impl->convertedBuffer.data(), &framesOut);

    if (r != MA_SUCCESS || framesOut == 0) {
        return;
    }

    impl->callback(impl->convertedBuffer.data(),
        static_cast<size_t>(framesOut),
        impl->targetSampleRate,
        impl->targetChannels);
}

AudioStreamRecorder::AudioStreamRecorder() : m_impl(std::make_unique<Impl>()) {}

AudioStreamRecorder::~AudioStreamRecorder() {
    stop();
}

std::vector<AudioDeviceInfo> AudioStreamRecorder::enumerateInputDevices() {
    std::vector<AudioDeviceInfo> result;

    ma_context context{};
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
        return result;
    }

    ma_device_info* pCaptureInfos = nullptr;
    ma_uint32 captureCount = 0;
    ma_result r = ma_context_get_devices(&context, nullptr, nullptr, &pCaptureInfos, &captureCount);
    if (r == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < captureCount; ++i) {
            AudioDeviceInfo info;
            info.name = pCaptureInfos[i].name;
            info.isDefault = pCaptureInfos[i].isDefault != 0;
            info.idHex = deviceIdToHex(pCaptureInfos[i].id);
            result.push_back(std::move(info));
        }
    }

    ma_context_uninit(&context);
    return result;
}

bool AudioStreamRecorder::start(const AudioStreamConfig& config, AudioChunkCallback onChunk) {
    if (m_impl->running) return true;

    m_impl->callback = std::move(onChunk);
    m_impl->lastError.clear();
    m_impl->targetSampleRate = config.sampleRate;
    m_impl->targetChannels = config.channels;

    ma_context_config ctxConfig = ma_context_config_init();
    if (ma_context_init(nullptr, 0, &ctxConfig, &m_impl->context) != MA_SUCCESS) {
        m_impl->lastError = "ma_context_init failed";
        return false;
    }
    m_impl->contextInited = true;

    ma_device_config devConfig = ma_device_config_init(ma_device_type_capture);
    devConfig.capture.format = ma_format_unknown;   // 0 = 使用设备原生格式
    devConfig.capture.channels = 0;                 // 0 = 使用设备原生声道数
    devConfig.sampleRate = 0;                       // 0 = 使用设备原生采样率
    devConfig.dataCallback = data_callback;
    devConfig.pUserData = m_impl.get();

    //if (config.requestRawMode) {
    //    devConfig.wasapi.usage = ma_wasapi_usage_pro_audio;
    //}

    ma_device_id selectedId{};
    if (!config.deviceIdHex.empty() && hexToDeviceId(config.deviceIdHex, selectedId)) {
        devConfig.capture.pDeviceID = &selectedId;
    }

    if (ma_device_init(&m_impl->context, &devConfig, &m_impl->device) != MA_SUCCESS) {
        m_impl->lastError = "ma_device_init failed (设备可能被占用或不支持该配置)";
        ma_context_uninit(&m_impl->context);
        m_impl->contextInited = false;
        return false;
    }
    m_impl->deviceInited = true;
    m_impl->nativeFormat = m_impl->device.capture.format;
    m_impl->nativeSampleRate = m_impl->device.capture.internalSampleRate;
    m_impl->nativeChannels = m_impl->device.capture.internalChannels;

    ma_data_converter_config converterConfig = ma_data_converter_config_init(
        m_impl->nativeFormat, ma_format_s16,
        m_impl->nativeChannels, m_impl->targetChannels,
        m_impl->nativeSampleRate, m_impl->targetSampleRate);

    // 使用高质量的重采样算法（默认线性插值质量一般，Linear 也可以调阶数；
    // 这里用 ma_resample_algorithm_linear 并提高 lpfOrder，或者可选择
    // ma_resample_algorithm_custom。miniaudio 默认线性重采样对语音场景通常够用，
    // 如果对音质有更高要求，可以研究 speex resampler 集成。
    converterConfig.resampling.linear.lpfOrder = MA_MAX_FILTER_ORDER;

    if (ma_data_converter_init(&converterConfig, nullptr, &m_impl->converter) != MA_SUCCESS) {
        m_impl->lastError = "ma_data_converter_init failed";
        ma_device_uninit(&m_impl->device);
        ma_context_uninit(&m_impl->context);
        m_impl->deviceInited = false;
        m_impl->contextInited = false;
        return false;
    }
    m_impl->converterInited = true;

    if (ma_device_start(&m_impl->device) != MA_SUCCESS) {
        m_impl->lastError = "ma_device_start failed";
        ma_data_converter_uninit(&m_impl->converter, nullptr);
        m_impl->converterInited = false;
        ma_device_uninit(&m_impl->device);
        ma_context_uninit(&m_impl->context);
        m_impl->deviceInited = false;
        m_impl->contextInited = false;
        return false;
    }

    m_impl->running = true;
    m_impl->paused = false;
    return true;
}

void AudioStreamRecorder::stop() {
    if (!m_impl->running && !m_impl->deviceInited && !m_impl->contextInited) return;

    if (m_impl->deviceInited) {
        ma_device_uninit(&m_impl->device);
        m_impl->deviceInited = false;
    }
    if (m_impl->converterInited) {
        ma_data_converter_uninit(&m_impl->converter, nullptr);
        m_impl->converterInited = false;
    }
    if (m_impl->contextInited) {
        ma_context_uninit(&m_impl->context);
        m_impl->contextInited = false;
    }
    m_impl->running = false;
    m_impl->paused = false;
    m_impl->callback = nullptr;
}

void AudioStreamRecorder::pause() {
    if (m_impl->running) m_impl->paused = true;
}

void AudioStreamRecorder::resume() {
    if (m_impl->running) m_impl->paused = false;
}

bool AudioStreamRecorder::isRunning() const { return m_impl->running; }
bool AudioStreamRecorder::isPaused() const { return m_impl->paused.load(); }

// 注意：这两个接口现在返回的是"目标格式"参数（业务层实际拿到的数据格式），
// 而不是设备原生参数。如果你需要知道设备原生参数用于调试，
// 可以另外加 nativeSampleRate()/nativeChannels() 接口暴露 Impl 里记录的值。
uint32_t AudioStreamRecorder::actualSampleRate() const { return m_impl->targetSampleRate; }
uint32_t AudioStreamRecorder::actualChannels() const { return m_impl->targetChannels; }

std::string AudioStreamRecorder::lastError() const { return m_impl->lastError; }