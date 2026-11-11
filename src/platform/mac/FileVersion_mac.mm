#include "utils/FileVersion.h"

// macOS 没有 CUDA，用不到运行库版本比对
QString fileVersion(const QString&)
{
    return QString();
}
