#include "SherpaPunctuator.h"

#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>

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

    sherpa_onnx::cxx::OfflinePunctuationConfig config;
    config.model.ct_transformer = modelFile.toStdString();
    config.model.num_threads = 1;
    config.model.provider = "cpu";

    auto newPunct = std::make_unique<sherpa_onnx::cxx::OfflinePunctuation>(
        sherpa_onnx::cxx::OfflinePunctuation::Create(config));
    if (!newPunct) {
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_punct = std::move(newPunct);   
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
    const std::string in = text.toUtf8().toStdString();
    const std::string out = m_punct->AddPunctuation(in);
    return QString::fromUtf8(out.c_str(), static_cast<int>(out.size()));
}

void SherpaPunctuator::unload()
{
    QMutexLocker locker(&m_mutex);
    m_punct.reset();
    m_loadedModelDir.clear();
}