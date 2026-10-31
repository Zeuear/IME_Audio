#ifndef SNIPPETPASTER_H
#define SNIPPETPASTER_H

#include <QImage>
#include <functional>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class SnippetStore;
struct SnippetItem;

// @brief 把话术条目粘贴进前台窗口：文字、图片、文件，或它们的组合（文件总在最后）。
//        远程图片先下载并缓存到话术库的 images 目录，下次直接用本地文件。
//        走 InputInjector 的剪贴板路径，不经 TranscriptionService，所以不会被当成听写结果处理。
class SnippetPaster : public QObject {
    Q_OBJECT

signals:
    void errorOccurred(const QString& title, const QString& cause = {});

public:
    SnippetPaster(SnippetStore* store, QNetworkAccessManager* network, QObject* parent = nullptr);

    void paste(const QString& itemId);

private:
    void pasteLoaded(const QString& text, const QImage& image, const QString& filePath, bool imageFirst);
    void loadImage(const QString& imagePath, std::function<void(const QImage&)> done);

    SnippetStore* m_store = nullptr;
    QNetworkAccessManager* m_network = nullptr;
};

#endif // SNIPPETPASTER_H
