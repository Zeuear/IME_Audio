#pragma once
#include <QObject>
#include <QString>
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

};