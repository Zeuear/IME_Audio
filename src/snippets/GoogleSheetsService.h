#pragma once

#include "snippets/SnippetTypes.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>
#include <functional>

// 对应原 google_sheets_sync.py。
// 注意：当前实现用嵌套 QEventLoop 阻塞等待网络，回调在函数返回前同步触发（UI 不冻结但会重入事件循环）。
// 仅走 gviz CSV 路径；XLSX 导出解析与嵌入图片提取未移植。
class GoogleSheetsService : public QObject {
    Q_OBJECT
public:
    explicit GoogleSheetsService(QObject* parent = nullptr);
    ~GoogleSheetsService() override;

    // 从 Google 表格 URL 或裸 ID 提取 spreadsheet ID，失败返回空串
    static QString extractSpreadsheetId(const QString& urlOrId);

    // 解析 CSV 行（支持引号、转义引号、单元格内换行）
    static QVector<QStringList> parseCsv(const QString& csvText);

    // 将行 (A=中文, B=外文) 解析进 L2 文件夹：含 '@@' 的 A 列生成 L3 目录（对应 _parse_rows_into_tree）
    static void parseRowsIntoFolder(FolderNode* l2Folder, const QVector<QStringList>& rows);

    // callback: ok, docTitle, spreadsheetId, sheets(gid, name), error
    using ListSheetsCallback = std::function<void(bool, QString, QString, QVector<QPair<QString, QString>>, QString)>;
    void listSheetNames(const QString& url, ListSheetsCallback callback, int timeoutMs = 20000);

    // 下载指定 Sheet 的 CSV，解析为一个 L2 FolderNode。callback: ok, sheets(仅 1 个 L2), error
    using FetchSheetCallback = std::function<void(bool, FolderList, QString)>;
    void fetchSheetItems(const QString& spreadsheetId, const QString& sheetGid,
                         const QString& sheetName, const QString& imagesDir,
                         FetchSheetCallback callback, int timeoutMs = 20000);

private:
    QNetworkAccessManager* m_nam;

    bool blockingGet(const QUrl& url, int timeoutMs, QByteArray* body, QString* error, int* status = nullptr);
    QString buildCsvUrl(const QString& spreadsheetId, const QString& gid) const;
};
