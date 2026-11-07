#pragma once
#include <QString>

// @brief 实时转录稿子的本地保存：崩溃或误关后能找回，也能导出 txt。
//        只保留一份"上次的稿子"，每次覆盖，不累积历史（稿子内容可能涉及隐私）。
//        写入先落到同目录的临时文件再改名，写到一半失败时旧内容不会被破坏。
class TranscriptStore
{
public:
    explicit TranscriptStore(const QString& sessionFile);

    // 覆盖保存；缺少的父目录会自动创建。失败返回 false，由调用方决定是否通知用户
    bool save(const QString& text);
    // 文件不存在或读不了时返回空串
    QString load() const;
    // 删除会话文件；文件本来就不存在也不算错
    void remove();

    // 导出为不带 BOM 的 UTF-8 文本，不会自动创建目录
    static bool exportTxt(const QString& path, const QString& text);

private:
    QString m_path;
};
