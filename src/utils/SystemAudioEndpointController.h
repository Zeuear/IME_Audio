#pragma once
#include <string>

// 系统级音频端点控制器：在开始监听时把【系统默认播放设备】和/或【系统默认输入（麦克风）设备】
// 切到指定端点，停止监听时 restore() 还原。平台相关实现完全隔离在本类的 .cpp 中，
// 本头不依赖 Qt，也不暴露任何平台 API，便于跨平台编译与解耦。
//
// 设备端点 id 使用 std::string（Windows 上为 MMDevice endpoint ID）。调用方负责把各自
// 平台的设备标识（如 QAudioDevice::id()）转换为 std::string 传入。
class SystemAudioEndpointController {
public:
    SystemAudioEndpointController() = default;
    ~SystemAudioEndpointController() = default;

    SystemAudioEndpointController(const SystemAudioEndpointController&) = delete;
    SystemAudioEndpointController& operator=(const SystemAudioEndpointController&) = delete;

    // 把系统默认【播放】设备切换到 endpointId；成功返回 true，内部记录原默认值以便 restore。
    bool setDefaultOutput(const std::string& endpointId);

    // 把系统默认【输入/麦克风】设备切换到 endpointId；成功返回 true，内部记录原默认值。
    bool setDefaultInput(const std::string& endpointId);

    // 返回当前系统默认播放/输入端点 id（空串表示获取失败或平台不支持）。
    std::string getDefaultOutputId() const;
    std::string getDefaultInputId() const;

    // 还原本控制器记录的所有原默认端点（输出+输入）。重复调用安全。
    void restore();

private:
    // 首次切换前记录当前系统默认端点（输出+输入），仅记录一次。
    void ensureSaved();

    std::string m_savedOutputId;
    std::string m_savedInputId;
    bool m_hasSaved = false;
};
