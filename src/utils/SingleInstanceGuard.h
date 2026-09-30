#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

#include <memory>

class QLockFile;

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
