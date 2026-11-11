#pragma once
#include <QString>

// @brief 读取可执行文件 / 动态库的版本号（Windows 版本资源里的 FileVersion）。
// 不支持的平台或文件没有版本资源时返回空串，调用方应把空串当作"无法判断"。
QString fileVersion(const QString& path);
