#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

#include <memory>

class QLockFile;

class SingleInstanceGuard : public QObject {
    Q_OBJECT
public:
    explicit SingleInstanceGuard(const QString& dataDir, QObject* parent = nullptr);
    ~SingleInstanceGuard() override;
    bool acquireOrNotifyExisting();
    static QString serverNameForDataDir(const QString& dataDir);

signals:
    void anotherInstanceStarted();

private:
    QString m_dataDir;
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer* m_server = nullptr;
};
