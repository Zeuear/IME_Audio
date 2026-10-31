#include "SnippetPaster.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <functional>
#include <memory>

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
    QString filePath = item->filePath.trimmed();
    const bool imageFirst = m_store->settings().comboImageFirst;
    m_store->incrementUseCount(itemId);

    // 只记了路径，原文件可能已被移走或删除；其余内容仍然可用
    if (!filePath.isEmpty() && !QFileInfo::exists(filePath)) {
        emit errorOccurred(tr("文件粘贴失败"), tr("找不到文件：%1").arg(filePath));
        filePath.clear();
    }

    if (imagePath.isEmpty()) {
        pasteLoaded(text, QImage(), filePath, imageFirst);
        return;
    }
    loadImage(imagePath, [this, text, filePath, imageFirst](const QImage& image) {
        if (image.isNull()) {
            emit errorOccurred(tr("图片粘贴失败"), tr("无法读取图片文件或下载远程图片"));
            // 图片失败时文字和文件仍然可用
            pasteLoaded(text, QImage(), filePath, imageFirst);
            return;
        }
        pasteLoaded(text, flattenOnWhite(image), filePath, imageFirst);
    });
}

// 一次拖放只能落下一种内容，目标应用看到文字和文件并存时常常只取文字，
// 所以带附件的条目只拖附件；文字要和附件一起发时用点击粘贴
QMimeData* SnippetPaster::createDragMime(const QString& itemId) const {
    const SnippetItem* item = m_store->findItemById(itemId);
    if (!item) return nullptr;

    const QString imagePath = item->imagePath.trimmed();
    const bool remote = imagePath.startsWith(QLatin1String("http://")) || imagePath.startsWith(QLatin1String("https://"));
    const QString imageFile = imagePath.isEmpty() ? QString()
                            : remote ? m_store->remoteImageCachePath(imagePath)
                                     : m_store->resolveImagePath(imagePath);
    const bool hasImage = !imageFile.isEmpty() && QFileInfo::exists(imageFile);
    const QString filePath = item->filePath.trimmed();
    const bool hasFile = !filePath.isEmpty() && QFileInfo::exists(filePath);

    auto mime = std::make_unique<QMimeData>();
    QList<QUrl> urls;
    if (hasImage) urls << QUrl::fromLocalFile(imageFile);
    if (hasFile) urls << QUrl::fromLocalFile(filePath);
    if (!urls.isEmpty()) {
        mime->setUrls(urls);
        if (hasImage && !hasFile) mime->setImageData(flattenOnWhite(QImage(imageFile)));
    } else if (!item->text.trimmed().isEmpty()) {
        mime->setText(item->text);
    } else {
        return nullptr;
    }
    return mime.release();
}

void SnippetPaster::pasteLoaded(const QString& text, const QImage& image, const QString& filePath, bool imageFirst) {
    const bool hasText = !text.trimmed().isEmpty();
    const bool hasImage = !image.isNull();
    const bool hasFile = !filePath.isEmpty();

    QVector<std::function<bool()>> steps;
    if (hasImage && imageFirst) steps << [image]() { return InputInjector::pasteImage(image); };
    if (hasText) steps << [text]() { return InputInjector::pasteText(text); };
    if (hasImage && !imageFirst) steps << [image]() { return InputInjector::pasteImage(image); };
    if (hasFile) steps << [filePath]() { return InputInjector::pasteFile(filePath); };

    bool ok = true;
    for (int i = 0; i < steps.size(); ++i) {
        if (i == 0) {
            ok = steps[0]();
            continue;
        }
        QTimer::singleShot(kComboGapMs * i, this, [step = steps[i]]() {
            if (!step()) LOG_WARN("Snippet combo: later paste step failed");
        });
    }

    if (!ok) {
        LOG_WARN("Snippet paste failed");
#ifdef Q_OS_WIN
        emit errorOccurred(tr("粘贴失败"), tr("无法向当前窗口发送粘贴操作，内容已留在剪贴板，可手动 Ctrl+V"));
#else
        // Linux 尚未实现模拟按键，macOS 可能缺辅助功能权限；文字路径不会写剪贴板，这里补上才能手动粘贴
        if (!hasImage && !hasFile && hasText) QGuiApplication::clipboard()->setText(text);
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
