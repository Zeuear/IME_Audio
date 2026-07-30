#include "Logger.h"

Logger& Logger::instance() {
    static Logger _instance;
    return _instance;
}

void Logger::setLogPath(const QString& path) {
    QMutexLocker locker(&m_mutex);
    m_logPath = path;
}

void Logger::log(const QString& level, const QString& message) {
    QMutexLocker locker(&m_mutex); 

    if (level == "INFO") {
        emit newLogEntry(QString("%1 | %2").arg(level, message));
        return;
    }

    if (m_logPath.isEmpty()) {
        qDebug() << QString("[%1] %2 | %3").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"), level, message);
        return;
    }

    QFile file(m_logPath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        // 2026-07-20 10:00:00.123 | INFO | Message
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        out << timestamp << " | " << level << " | " << message << "\n";

        file.close();
        emit newLogEntry(QString("[%1] %2 | %3").arg(timestamp, level, message));

    } else {
        qWarning() << "Failed to open log file:" << m_logPath;
    }
    
}