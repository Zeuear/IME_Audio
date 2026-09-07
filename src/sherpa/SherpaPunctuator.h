#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <QDir>


struct SherpaOnnxOfflinePunctuation;

class SherpaPunctuator : public QObject {
    Q_OBJECT
public:
    explicit SherpaPunctuator(QObject* parent = nullptr);
    ~SherpaPunctuator() override;

    SherpaPunctuator(const SherpaPunctuator&) = delete;
    SherpaPunctuator& operator=(const SherpaPunctuator&) = delete;

    bool load(const QString& modelDir, bool forceReload = false);
    bool isLoaded() const;
    QString currentModelDir() const;

    QString punctuate(const QString& text);
    void unload();

private:
    mutable QMutex m_mutex;
    const SherpaOnnxOfflinePunctuation* m_punct = nullptr;
    QString m_loadedModelDir;
};