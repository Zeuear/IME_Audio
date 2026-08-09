#include "AppConfig.h"

// 默认设备名常量：与 AppConfig 同生命周期，独立 TU 便于测试单独链接
// （原先定义在 RecorderService.cpp，导致脱离录音层时链接不到）。
const QString AudioConfig::kDefaultDeviceName = QStringLiteral("系统默认录音设备");
