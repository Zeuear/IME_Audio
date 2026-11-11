#include "utils/Firewall.h"

// macOS 不管理防火墙
bool Firewall::isSupported() { return false; }
Firewall::State Firewall::state(const QString&) { return State::Missing; }
