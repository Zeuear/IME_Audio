#include "SnippetPaster.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <functional>

#include "services/InputInjector.h"
#include "snippets/SnippetStore.h"
#include "utils/Logger.h"

namespace {

// 图文组合：两次粘贴之间留出时间，让目标应用先消化第一次剪贴板内容
constexpr int kComboGapMs = 300;

// 透明背景转白底，否则 PNG 贴到聊天软件里常是一块黑
QImage flattenOnWhite(const QImage& src) {
    if (!src.hasAlphaChannel()) return src;
    QImage out(src.size(), QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.drawImage(0, 0, src);
    return out;
}

} // namespace

SnippetPaster::SnippetPaster(SnippetStore* store, QNetworkAccessManager* network, QObject* parent)
    : QObject(parent), m_store(store), m_network(network) {}

void SnippetPaster::paste(const QString& itemId) {
    SnippetItem* item = m_store->findItemById(itemId);
    if (!item) return;

    const QString text = item->text;
    const QString imagePath = item->imagePath.trimmed();
    const bool imageFirst = m_store->settings().comboImageFirst;
    m_store->incrementUseCount(itemId);

    if (imagePath.isEmpty()) {
        pasteLoaded(text, QImage(), imageFirst);
        return;
    }
    loadImage(imagePath, [this, text, imageFirst](const QImage& image) {
        if (image.isNull()) {
            emit errorOccurred(tr("图片粘贴失败"), tr("无法读取图片文件或下载远程图片"));
            // 图片失败时文字仍然可用
            if (!text.trimmed().isEmpty()) pasteLoaded(text, QImage(), imageFirst);
            return;
        }
        pasteLoaded(text, flattenOnWhite(image), imageFirst);
    });
}

void SnippetPaster::pasteLoaded(const QString& text, const QImage& image, bool imageFirst) {
    const bool hasText = !text.trimmed().isEmpty();
    const bool hasImage = !image.isNull();

    auto pasteTextStep = [text]() { return InputInjector::pasteText(text); };
    auto pasteImageStep = [image]() { return InputInjector::pasteImage(image); };

    bool ok = true;
    if (hasText && hasImage) {
        auto first = imageFirst ? std::function<bool()>(pasteImageStep) : std::function<bool()>(pasteTextStep);
        auto second = imageFirst ? std::function<bool()>(pasteTextStep) : std::function<bool()>(pasteImageStep);
        ok = first();
        QPointer<SnippetPaster> self(this);
        QTimer::singleShot(kComboGapMs, this, [self, second]() {
            if (self && !second()) LOG_WARN("Snippet combo: second paste step failed");
        });
    } else if (hasImage) {
        ok = pasteImageStep();
    } else if (hasText) {
        ok = pasteTextStep();
    }

    if (!ok) {
        LOG_WARN("Snippet paste failed");
#ifdef Q_OS_WIN
        emit errorOccurred(tr("粘贴失败"), tr("无法向当前窗口发送粘贴操作，内容已留在剪贴板，可手动 Ctrl+V"));
#else
        // Linux 尚未实现模拟按键，macOS 可能缺辅助功能权限；文字路径不会写剪贴板，这里补上才能手动粘贴
        if (!hasImage && hasText) QGuiApplication::clipboard()->setText(text);
        emit errorOccurred(tr("粘贴失败"), tr("当前系统暂不支持自动粘贴（或缺少辅助功能权限），内容已复制到剪贴板，请手动粘贴"));
#endif
    }
}

void SnippetPaster::loadImage(const QString& imagePath, std::function<void(const QImage&)> done) {
    const bool remote = imagePath.startsWith(QLatin1String("http://")) || imagePath.startsWith(QLatin1String("https://"));
    if (!remote) {
        done(QImage(m_store->resolveImagePath(imagePath)));
        return;
    }

    // 远程图缓存为 images/url_<md5>.png，同一个链接只下载一次
    const QString cachePath = m_store->remoteImageCachePath(imagePath);
    if (QFileInfo::exists(cachePath)) {
        done(QImage(cachePath));
        return;
    }

    QNetworkReply* reply = m_network->get(QNetworkRequest(QUrl(imagePath)));
    QPointer<SnippetPaster> self(this);
    connect(reply, &QNetworkReply::finished, this, [self, reply, cachePath, done]() {
        reply->deleteLater();
        if (!self) return;
        QImage image;
        if (reply->error() == QNetworkReply::NoError) image.loadFromData(reply->readAll());
        else LOG_WARN(QString("Snippet image download failed: %1").arg(reply->errorString()));
        if (!image.isNull()) image.save(cachePath, "PNG");
        done(image);
    });
}
