#include "services/PrefixWatcher.h"

// macOS 暂未实现全局打字前缀监听：需要 CGEventTap 和输入监控权限。
// isSupported() 为 false，界面据此隐藏相关选项；其余接口为空操作。
struct PrefixWatcher::Impl {};

PrefixWatcher::PrefixWatcher(QObject* parent) : QObject(parent), m_impl(std::make_unique<Impl>()) {}
PrefixWatcher::~PrefixWatcher() = default;

bool PrefixWatcher::isSupported() { return false; }
void PrefixWatcher::setPrefixes(const QStringList&) {}
void PrefixWatcher::setEnabled(bool) {}
void PrefixWatcher::endQuery() {}
void PrefixWatcher::eraseTyped(int) {}
