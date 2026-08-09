#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "cxx-api.h"

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

    bool load(const QString& modelDir);
    bool isLoaded() const;

    QString punctuate(const QString& text);
    void unload();

private:
    std::unique_ptr<sherpa_onnx::cxx::OfflinePunctuation> m_punct;
};
