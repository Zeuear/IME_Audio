#pragma once
#include <QString>
#include <QtGlobal>

// @brief 下载进度文字格式化，纯逻辑；模型行、CUDA 行、翻译卡片共用"文件名 · 第几个/共几个 · 百分比"。
namespace DownloadText {

QString formatSize(qint64 bytes);

// 单个文件名超过 maxNameChars 时从中间省略（保留开头和扩展名）；
// 只有一个文件时不显示序号；percent 为负表示未知，不显示百分比，超过 100 按 100
QString progressLine(const QString& fileName, int fileIndex, int fileCount, int percent, int maxNameChars = 36);

} // namespace DownloadText

// @brief 一组文件（模型、CUDA 包）的进度累计：文件数、完成数、当前文件、整体百分比；喂下载器信号即可。
class DownloadGroupProgress
{
public:
    void start(int totalFiles);
    void fileFinished();
    void update(const QString& fileName, int overallPercent);

    int percent() const { return m_percent; }
    int finishedFiles() const { return m_finished; }
    int totalFiles() const { return m_total; }
    // 还没收到过进度时为空串
    QString line(int maxNameChars = 36) const;

private:
    int m_total = 0;
    int m_finished = 0;
    int m_percent = 0;
    QString m_name;
    bool m_hasUpdate = false;
};
