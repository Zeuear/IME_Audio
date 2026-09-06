#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

#include <memory>

class QLockFile;

// 用 QLockFile 判定唯一性（它记录 PID 并检查存活，能自动回收崩溃残留），
// QLocalServer 只用来通知已在运行的实例。
//
// 不用"能否连上服务端"来判定：Windows 上泄漏的命名管道即使无人监听也连得上，
// 会让程序永久拒绝启动。通知通道失效时只影响窗口不被唤起，不影响能否启动。
class SingleInstanceGuard : public QObject {
    Q_OBJECT
public:
    // dataDir 为每用户的数据目录，锁文件与通知通道均由它派生
    explicit SingleInstanceGuard(const QString& dataDir, QObject* parent = nullptr);
    ~SingleInstanceGuard() override;

    // true 表示本进程是唯一实例；false 表示已有实例在运行，并已尝试通知对方。
    bool acquireOrNotifyExisting();

    static QString serverNameForDataDir(const QString& dataDir);

signals:
    void anotherInstanceStarted();

private:
    QString m_dataDir;
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer* m_server = nullptr;
};
