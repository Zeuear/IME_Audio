#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QSettings>
#include <QReadWriteLock> 
#include "AppConfig.h"

class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager& instance();
    explicit ConfigManager(QObject *parent = nullptr);
    void applyDefaults();

    static void setConfigFilePath(const QString& path);
    static QString configFilePath();

    // 原子写（临时文件再替换）；不创建目录，目标目录不存在返回 false
    static bool writeFileAtomically(const QString& path, const QByteArray& data, bool ownerOnly = false);

    bool load();
    bool save();

    AppConfig &config() { return m_config; }
    const AppConfig &config() const { return m_config; }
    void updateConfig(const AppConfig& newConfig);

signals:
    void configLoaded();
    void configSaved();

private:
    AppConfig m_config;
    mutable QReadWriteLock m_lock;
    inline static QString m_configPath;
    // 本实例载入或保存过之后，内存里的配置才是权威；在此之前不能覆盖已存在的配置文件
    bool m_synced = false;

};