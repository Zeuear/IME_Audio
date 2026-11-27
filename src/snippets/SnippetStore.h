#pragma once

#include "snippets/SnippetTypes.h"
#include "snippets/SnippetSearchIndex.h"
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QPixmap>
#include <QColor>
#include <memory>
#include <QObject>
#include <QTimer>
#include <functional>

class GoogleSheetsService;

class SnippetStore : public QObject {
public:
    struct GoogleSheetConfig {
        QString id;
        QString treeId;
        QString spreadsheetId;
        QString title;
        QString url;
        QStringList selectedSheets;
        QStringList allSheets;
        QString lastSync;
    };

    using SearchHit = SnippetSearchIndex::Hit;

    explicit SnippetStore(const QString& dataFilePath);
    ~SnippetStore() override;

    bool load();
    bool save();
    bool flush();

    FolderList& roots() { return m_roots; }
    const FolderList& roots() const { return m_roots; }

    AppSettings& settings() { return m_settings; }
    const AppSettings& settings() const { return m_settings; }

    QVector<GoogleSheetConfig>& googleSheets() { return m_googleSheets; }
    const QVector<GoogleSheetConfig>& googleSheets() const { return m_googleSheets; }

    const QString& baseDir() const { return m_baseDir; }
    const QString& imagesDir() const { return m_imagesDir; }
    QString resolveImagePath(const QString& relOrAbs) const;
    // 远程图缓存位置 images/url_<md5>.png，同一个链接只下载一次；粘贴和列表缩略图共用
    QString remoteImageCachePath(const QString& url) const;
    QString copyImageToStore(const QString& srcPath);
    QString savePixmapToStore(const QPixmap& pixmap);

    // level<=0 时按父级自动推算（max 3）；parentId 为空建 L1。返回树内指针（下次增删前有效）
    FolderNode* addFolder(const QString& parentId, const QString& title, int level = 0);

    bool toggleFolderCollapsed(const QString& folderId);
    bool setAllCollapsed(bool collapsed, int maxLevel = 3);

    // parentFolderId 可为文件夹或条目 id（条目则取其所在文件夹）；找不到时回退到第一个可用文件夹
    SnippetItem* addItem(const QString& parentFolderId, const SnippetItem& itemData);

    // field: title / text / keyword / image_path / file_path；文件夹仅支持 title
    bool updateNode(const QString& nodeId, const QString& field, const QString& value);

    // 删除文件夹（含子节点）或条目；删除 L1 时同步移除 google_sheets 配置
    bool deleteNode(const QString& nodeId);
    int deleteNodes(const QStringList& nodeIds);

    bool moveNode(const QString& nodeId, int direction);
    bool incrementUseCount(const QString& itemId);

    FolderNode* findFolderById(const QString& id);
    SnippetItem* findItemById(const QString& id);

    bool findFolder(const QString& id,
                    FolderNode** outNode = nullptr,
                    FolderNode** outParent = nullptr,
                    FolderList** outList = nullptr,
                    int* outIndex = nullptr);
    bool findItem(const QString& id,
                  SnippetItem** outItem = nullptr,
                  FolderNode** outFolder = nullptr,
                  int* outIndex = nullptr);

    bool addGoogleSheet(const GoogleSheetConfig& gs);
    bool updateGoogleSheet(const QString& treeId, const GoogleSheetConfig& gs);
    bool removeGoogleSheet(const QString& treeId);
    GoogleSheetConfig* findGoogleSheetByTreeId(const QString& treeId);
    GoogleSheetConfig* findGoogleSheetByUrl(const QString& url);

    // 对应原 sync_google_sheet 的合并部分。selectedSheets 为空时沿用已记住的勾选，仍为空则全选
    QString syncGoogleSheet(const QString& url,
                            const QString& customAlias,
                            const QString& existingTreeId,
                            const QStringList& selectedSheets,
                            const QStringList& allSheetNames,
                            const QString& fetchedDocTitle,
                            FolderList newSheets);

    // 联网拉取并合并（异步，不嵌套事件循环）。selectedNames 为空沿用记住的勾选。
    // done(treeId, error)：失败时 treeId 为空。拉取阶段不动树，全部拉完才在同一个调用栈里替换并回调，
    // 所以回调里立刻刷新模型即可，期间界面不会拿着旧指针重绘
    using ImportCallback = std::function<void(const QString& treeId, const QString& error)>;
    void importGoogleSheet(GoogleSheetsService* service,
                           const QString& url,
                           const QString& customAlias,
                           const QString& existingTreeId,
                           const QStringList& selectedNames,
                           ImportCallback done);

    using RefreshCallback = std::function<void(int okCount, const QStringList& errors)>;
    void refreshAllGoogleSheets(GoogleSheetsService* service, RefreshCallback done);

    QVector<SearchHit> search(const QString& query, bool allWhenEmpty = false);

    QVector<SnippetItem*> matchKeywords(const QString& spokenText, int limit = 4);

    int countItemsInFolder(const FolderNode& folder) const;
    QStringList getNodePath(const QString& targetId) const;

private:
    struct SheetFetch;
    struct RefreshJob;
    using FetchDone = std::function<void(std::shared_ptr<SheetFetch>)>;
    void fetchGoogleSheet(GoogleSheetsService* service, std::shared_ptr<SheetFetch> job, FetchDone done);
    void fetchNextSheet(GoogleSheetsService* service, std::shared_ptr<SheetFetch> job, FetchDone done);
    void refreshNext(GoogleSheetsService* service, std::shared_ptr<RefreshJob> job);
    QString applyFetched(SheetFetch& job);

    SnippetSearchIndex m_searchIndex;
    bool m_dirty = false;
    QTimer m_saveTimer;

    QString m_dataFilePath;
    QString m_baseDir;
    QString m_dataDir;
    QString m_imagesDir;

    FolderList m_roots;
    AppSettings m_settings;
    QVector<GoogleSheetConfig> m_googleSheets;

    QJsonObject folderToJson(const FolderNode* folder) const;
    QJsonObject itemToJson(const SnippetItem* item) const;
    std::unique_ptr<FolderNode> jsonToFolder(const QJsonObject& obj, int level) const;
    SnippetItem jsonToItem(const QJsonObject& obj) const;

    QJsonObject getDefaultData() const;
    QString createSampleImage(const QString& filename, const QString& title,
                              const QString& subtitle, const QColor& bg) const;
    bool loadFromObject(QJsonObject root);
    void ensureDirs() const;
    static QString newUuid();
};
