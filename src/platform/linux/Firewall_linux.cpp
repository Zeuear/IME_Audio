#include "utils/Firewall.h"

// Linux 不管理防火墙
bool Firewall::isSupported() { return false; }
Firewall::State Firewall::state(const QString&) { return State::Missing; }
