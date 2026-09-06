#include "../../utils/SystemAudioEndpointController.h"

// Linux 不切换系统默认音频端点：录音由 QAudioSource 直接指定设备。
bool SystemAudioEndpointController::setDefaultOutput(const std::string&) { return true; }
bool SystemAudioEndpointController::setDefaultInput(const std::string&) { return true; }
std::string SystemAudioEndpointController::getDefaultOutputId() const { return {}; }
std::string SystemAudioEndpointController::getDefaultInputId() const { return {}; }
void SystemAudioEndpointController::ensureSaved() {}
void SystemAudioEndpointController::restore() {}
