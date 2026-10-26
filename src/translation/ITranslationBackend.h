#pragma once
#include <QObject>
#include <QString>

// @brief 本地翻译后端接口
class ITranslationBackend : public QObject {
    Q_OBJECT
public:
    explicit ITranslationBackend(QObject* parent = nullptr) : QObject(parent) {}

    // 异步翻译一句；id 由调用方分配，必须原样带回
    virtual void translate(qint64 id, const QString& text, const QString& from, const QString& to) = 0;
    virtual bool isAvailable(const QString& from, const QString& to) const = 0;
    virtual bool isUsable() const { return true; }

signals:
    void translated(qint64 id, bool ok, const QString& text, const QString& error);
};
