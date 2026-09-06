#include "../../utils/SystemAudioEndpointController.h"

// macOS 不切换系统默认音频端点。
//
// Windows 侧之所以需要这个能力，是为了兼容 VoiceMeeter 创建的虚拟声卡——必须把虚拟
// 声卡设为系统默认设备，录音链路才能工作。macOS 没有对应生态（BlackHole/Loopback 的
// 模型不同），录音直接由 QAudioSource 指定设备即可，无需改动用户的系统设置。
bool SystemAudioEndpointController::setDefaultOutput(const std::string&) { return true; }
bool SystemAudioEndpointController::setDefaultInput(const std::string&) { return true; }
std::string SystemAudioEndpointController::getDefaultOutputId() const { return {}; }
std::string SystemAudioEndpointController::getDefaultInputId() const { return {}; }
void SystemAudioEndpointController::ensureSaved() {}
void SystemAudioEndpointController::restore() {}
