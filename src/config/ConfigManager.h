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

    // 原子写文件：先写临时文件，成功后整体替换目标，写一半崩溃不会留下残缺文件。
    // 不会创建目录，目标目录不存在时返回 false，需要的话调用方先 mkpath
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

};