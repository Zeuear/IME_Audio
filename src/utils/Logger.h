#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QDebug>

class Logger :public QObject {
    Q_OBJECT

public:
    // 获取单例实例
    static Logger& instance();

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 设置日志文件路径
    void setLogPath(const QString& path);
    void log(const QString& level, const QString& message);

signals:
    void newLogEntry(const QString &entry);

private:
    Logger() = default; 
    ~Logger() = default;


    QString m_logPath;
    QMutex m_mutex; 
};



#define LOG_INFO(msg)        Logger::instance().log("INFO",  msg)
#define LOG_ERROR(msg)       Logger::instance().log("ERROR", msg)
#define LOG_WARN(msg)        Logger::instance().log("WARN",  msg)
#define LOG_DEBUG(msg)       Logger::instance().log("DEBUG", msg)


#endif // LOGGER_H