#pragma once
#include <QObject>
#include <QStringList>
#include <memory>

// @brief 全局键盘钩子监听“打字前缀”（如 // 或 ;;），命中发出 prefixTyped 呼出话术搜索框；默认关闭，setEnabled(true) 才装钩子和后台线程。
//        只认物理按键、不保存内容，缓冲只留最近 12 个字符且 2.5 秒后清空；Windows/macOS（需辅助功能权限）有实现，Linux 的 isSupported() 为 false。
class PrefixWatcher : public QObject {
    Q_OBJECT

signals:
    void prefixTyped(const QString& prefix);
    // 命中前缀后进入“跟随打字”：之后敲的字符会作为搜索词转发，方向键、回车、Tab、Esc 被拦下当导航键
    // （key 为 Qt::Key_Up / Key_Down / Key_Return；Key_Escape 表示放弃：Esc、退格删到前缀、光标移动等）
    void queryChanged(const QString& query);
    void navKey(int key);

public:
    explicit PrefixWatcher(QObject* parent = nullptr);
    ~PrefixWatcher() override;

    static bool isSupported();

    void setPrefixes(const QStringList& prefixes);
    void setEnabled(bool enabled);
    void endQuery();

    static void eraseTyped(int count);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
