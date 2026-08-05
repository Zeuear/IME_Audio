#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

#include "TextPolishService.h" // 复用 RequestParams 定义

class QNetworkAccessManager;

class TextPolishProvider {
public:
    using ResultCallback = std::function<void(bool success, QString text, QString error)>;
    using TestCallback = std::function<void(bool success, QString message)>;
    using ModelsCallback = std::function<void(bool success, QStringList models, QString error)>;

    virtual ~TextPolishProvider() = default;

    virtual bool supports(const TextPolishService::RequestParams& params) const = 0;
    virtual void polish(const QString& inputText,
                        const TextPolishService::RequestParams& params,
                        QNetworkAccessManager* mgr,
                        ResultCallback done) = 0;

    // 是否支持连接测试（默认不支持）
    virtual bool canTest() const { return false; }
    virtual void test(const TextPolishService::RequestParams& /*params*/,
                      QNetworkAccessManager* /*mgr*/,
                      TestCallback done) { done(false, QStringLiteral("该后端不支持连接测试")); }

    // 是否支持拉取模型列表（默认不支持）
    virtual bool canFetchModels() const { return false; }
    virtual void fetchModels(const QString& /*baseUrl*/,
                             QNetworkAccessManager* /*mgr*/,
                             ModelsCallback done) { done(false, {}, QStringLiteral("该后端不支持拉取模型列表")); }
};

// 按 params 选择对应 provider（轻量工厂，不引入注册表）
std::unique_ptr<TextPolishProvider> createTextPolishProvider(const TextPolishService::RequestParams& params);
