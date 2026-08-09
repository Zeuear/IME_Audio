#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <QDir>
#include <memory>


namespace sherpa_onnx::cxx {
    class OfflinePunctuation;
}

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
    std::unique_ptr<sherpa_onnx::cxx::OfflinePunctuation> m_punct;
    QString m_loadedModelDir;  
};