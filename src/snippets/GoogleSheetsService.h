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
#include <QHash>
#include <QByteArray>
#include <functional>

class GoogleSheetsService : public QObject {
    Q_OBJECT
public:
    explicit GoogleSheetsService(QObject* parent = nullptr);
    ~GoogleSheetsService() override;

    // 提取 spreadsheet ID
    static QString extractSpreadsheetId(const QString& urlOrId);

    // 解析 CSV 行
    static QVector<QStringList> parseCsv(const QString& csvText);

    // (行, 列) 均从 0 开始；表格里直接插入的图片按所在单元格对上，优先于 B 列的 =IMAGE() 公式
    using CellImages = QHash<QPair<int, int>, QString>;
    static void parseRowsIntoFolder(FolderNode* l2Folder, const QVector<QStringList>& rows,
                                    const CellImages& cellImages = {});

    // callback: ok, docTitle, spreadsheetId, sheets(gid, name), error
    using ListSheetsCallback = std::function<void(bool, QString, QString, QVector<QPair<QString, QString>>, QString)>;
    void listSheetNames(const QString& url, ListSheetsCallback callback, int timeoutMs = 20000);

    using FetchSheetCallback = std::function<void(bool, FolderList, QString)>;
    void fetchSheetItems(const QString& spreadsheetId, const QString& sheetGid,
                         const QString& sheetName, const QString& imagesDir,
                         FetchSheetCallback callback, int timeoutMs = 20000);

private:
    QNetworkAccessManager* m_nam;

    // 整本 XLSX 一次下载，同一次导入里的多张 Sheet 共用；空数据表示导出失败，后续直接走 CSV
    struct XlsxCache {
        qint64 savedAt = 0;
        QByteArray data;
    };
    QHash<QString, XlsxCache> m_xlsxCache;
    using XlsxCallback = std::function<void(const QByteArray&)>;
    void loadXlsx(const QString& spreadsheetId, int timeoutMs, XlsxCallback callback);
    void fetchSheetCsv(const QString& spreadsheetId, const QString& sheetGid, const QString& sheetName,
                       FetchSheetCallback callback, int timeoutMs);

    using GetCallback = std::function<void(bool, QByteArray, QString)>;
    void httpGet(const QUrl& url, int timeoutMs, GetCallback callback);
    QString buildCsvUrl(const QString& spreadsheetId, const QString& gid) const;
};
