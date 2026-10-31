#pragma once
#include <QObject>
#include <QStringList>
#include <memory>

// @brief 在任意软件里监听“打字前缀”（如 // 或 ;;），命中后发出 prefixTyped，用来呼出话术搜索框。
//        需要全局键盘钩子，所以默认关闭：setEnabled(true) 才会安装钩子和后台线程，关闭即全部释放。
//        只认键盘按下的物理按键（输入法开着也能识别），不读取、不保存按键内容，缓冲只留最近 12 个字符且 2.5 秒后清空。
//        各平台实现在 platform/<os>/PrefixWatcher_<os>.*；Windows（低级键盘钩子）和 macOS（CGEventTap，需辅助功能权限）有实现，Linux 的 isSupported() 为 false。
class PrefixWatcher : public QObject {
    Q_OBJECT

signals:
    // prefix 是命中的那个前缀（已经被打进了目标窗口）
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
    // 搜索框收起后调用：退出“跟随打字”，回到只等前缀的状态
    void endQuery();

    // 向前台窗口发 count 个退格，用来删掉已经打出去的前缀
    static void eraseTyped(int count);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
