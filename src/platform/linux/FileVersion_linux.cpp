#include "utils/FileVersion.h"

// .so 没有统一的版本资源，GPU 运行库也由发行版包管理器负责，这里不做判断
QString fileVersion(const QString&)
{
    return QString();
}
