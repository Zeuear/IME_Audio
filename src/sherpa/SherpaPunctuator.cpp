#include "SherpaPunctuator.h"

#include <cstring>

#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include "c-api.h"

SherpaPunctuator::SherpaPunctuator(QObject* parent)
    : QObject(parent)
{
}

SherpaPunctuator::~SherpaPunctuator()
{
    unload();
}

bool SherpaPunctuator::load(const QString& modelDir, bool forceReload)
{
    const QString normalizedDir = QDir(modelDir).absolutePath();
    {
        QMutexLocker locker(&m_mutex);
        if (!forceReload && m_punct && m_loadedModelDir == normalizedDir) {
            return true;
        }
    }

    const QString modelFile = QDir(modelDir).filePath("model.onnx");
    if (!QFileInfo::exists(modelFile)) {
        return false;
    }

    const QByteArray modelFileUtf8 = modelFile.toUtf8();

    SherpaOnnxOfflinePunctuationConfig config;
    memset(&config, 0, sizeof(config));
    config.model.ct_transformer = modelFileUtf8.constData();
    config.model.num_threads = 1;
    config.model.provider = "cpu";

    const SherpaOnnxOfflinePunctuation* newPunct = SherpaOnnxCreateOfflinePunctuation(&config);
    if (!newPunct) {
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_punct) {
            SherpaOnnxDestroyOfflinePunctuation(m_punct);
        }
        m_punct = newPunct;
        m_loadedModelDir = normalizedDir;
    }
    return true;
}

bool SherpaPunctuator::isLoaded() const
{
    QMutexLocker locker(&m_mutex);
    return m_punct != nullptr;
}

QString SherpaPunctuator::currentModelDir() const
{
    QMutexLocker locker(&m_mutex);
    return m_loadedModelDir;
}

QString SherpaPunctuator::punctuate(const QString& text)
{
    if (text.isEmpty()) {
        return text;
    }

    QMutexLocker locker(&m_mutex);
    if (!m_punct) {
        return text;
    }
    const QByteArray in = text.toUtf8();
    const char* result = SherpaOfflinePunctuationAddPunct(m_punct, in.constData());
    if (!result) {
        return text;
    }
    const QString out = QString::fromUtf8(result);
    SherpaOfflinePunctuationFreeText(result);
    return out;
}

void SherpaPunctuator::unload()
{
    QMutexLocker locker(&m_mutex);
    if (m_punct) {
        SherpaOnnxDestroyOfflinePunctuation(m_punct);
        m_punct = nullptr;
    }
    m_loadedModelDir.clear();
}
