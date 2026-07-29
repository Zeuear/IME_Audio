#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// 单个输入设备的描述信息
struct AudioDeviceInfo {
    std::string name;       // 设备显示名称，UI 下拉框直接用这个
    std::string idHex;       // 设备的内部标识（十六进制字符串），用于精确选中该设备
    bool isDefault = false;  // 是否为系统默认输入设备
};

// 录音会话配置
struct AudioStreamConfig {
    uint32_t sampleRate = 16000;   // ASR 模型常用 16000
    uint32_t channels = 1;         // 单声道，ASR 常用配置
    std::string deviceIdHex;       // 空 = 使用系统默认设备；否则填 AudioDeviceInfo::idHex
    bool requestRawMode = true;    // true: 尝试跳过系统麦克风增强效果链（推荐开启）
};

// 每收到一段 PCM 数据就会回调一次
// data: int16 PCM 采样数据（交织格式，如果 channels>1）
// frameCount: 本次回调包含多少个采样帧（不是字节数）
// sampleRate / channels: 实际生效的采样率/声道数（可能与请求值不同，务必以此为准）
using AudioChunkCallback = std::function<void(const int16_t* data,
                                               size_t frameCount,
                                               uint32_t sampleRate,
                                               uint32_t channels)>;

class AudioStreamRecorder {
public:
    AudioStreamRecorder();
    ~AudioStreamRecorder();

    AudioStreamRecorder(const AudioStreamRecorder&) = delete;
    AudioStreamRecorder& operator=(const AudioStreamRecorder&) = delete;

    static std::vector<AudioDeviceInfo> enumerateInputDevices();

    bool start(const AudioStreamConfig& config, AudioChunkCallback onChunk);
    void stop();

    void pause();
    void resume();

    bool isRunning() const;
    bool isPaused() const;

    uint32_t actualSampleRate() const;
    uint32_t actualChannels() const;

    std::string lastError() const;
    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
};
