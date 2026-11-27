#pragma once

#include <QString>

// macOS .app bundle 内不可写（写入还会破坏签名）：可写数据落 Application Support，只读资源在 Contents/Resources；
// Windows/Linux 两者同在可执行文件目录。新增文件务必按可写/只读归类，放错只在 macOS 运行时暴露。
namespace AppPaths {

QString dataDir();
// 只读、随应用分发：内置 VAD 模型
QString resourceDir();

QString logFile();
QString configFile();
QString termsFile();
QString dictionariesDir();
// SnippetStore 约定文件位于 <base>/data/snippets.json
QString snippetsFile();
QString promptsDir();
// 实时转录稿子的自动保存文件（可写，只留上次一份；属隐私内容，放在程序数据目录，随卸载清理）
QString transcriptFile();

QString toolsDir();
QString linkDownloadsDir();
// 人声分离输出的临时 WAV；程序启动和退出时清空
QString separatedDir();

QString sherpaRoot();
QString sherpaModelsDir();
QString translationModelsDir();
QString vadModelFile();

}
