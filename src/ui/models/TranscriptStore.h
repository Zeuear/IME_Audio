#pragma once
#include <QString>

// @brief 实时转录稿子的本地保存与导出 txt：只留一份上次稿子（涉及隐私），先写临时文件再改名防写坏。
class TranscriptStore
{
public:
    explicit TranscriptStore(const QString& sessionFile);

    // 覆盖保存；缺少的父目录会自动创建。失败返回 false，由调用方决定是否通知用户
    bool save(const QString& text);
    QString load() const;
    void remove();

    // 导出为不带 BOM 的 UTF-8 文本，不会自动创建目录
    static bool exportTxt(const QString& path, const QString& text);

private:
    QString m_path;
};
