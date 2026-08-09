#include "SherpaPunctuator.h"

#include <QDir>
#include <QFileInfo>

#include "cxx-api.h"

SherpaPunctuator::SherpaPunctuator(QObject* parent)
    : QObject(parent)
{
}

SherpaPunctuator::~SherpaPunctuator()
{
    unload();
}

bool SherpaPunctuator::load(const QString& modelDir)
{
    unload(); // 先清理旧模型

    const QString modelFile = QDir(modelDir).filePath("model.onnx");
    if (!QFileInfo::exists(modelFile)) {
        return false;
    }

    sherpa_onnx::cxx::OfflinePunctuationConfig config;
    config.model.ct_transformer = modelFile.toStdString();
    config.model.num_threads = 1;
    config.model.provider = "cpu";

    m_punct = std::make_unique<sherpa_onnx::cxx::OfflinePunctuation>(
        sherpa_onnx::cxx::OfflinePunctuation::Create(config));

    return isLoaded();
}

bool SherpaPunctuator::isLoaded() const
{
    return m_punct != nullptr;
}

QString SherpaPunctuator::punctuate(const QString& text)
{
    if (!isLoaded() || text.isEmpty()) {
        return text; // 未加载：原样返回，交给上层启发式兜底
    }
    // sherpa 标点模型按 UTF-8 文本处理
    const std::string in = text.toUtf8().toStdString();
    const std::string out = m_punct->AddPunctuation(in);
    return QString::fromUtf8(out.c_str(), static_cast<int>(out.size()));
}

void SherpaPunctuator::unload()
{
    m_punct.reset();
}
