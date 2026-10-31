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

class GoogleSheetsService : public QObject {
    Q_OBJECT
public:
    explicit GoogleSheetsService(QObject* parent = nullptr);
    ~GoogleSheetsService() override;

    // 提取 spreadsheet ID
    static QString extractSpreadsheetId(const QString& urlOrId);

    // 解析 CSV 行
    static QVector<QStringList> parseCsv(const QString& csvText);

    static void parseRowsIntoFolder(FolderNode* l2Folder, const QVector<QStringList>& rows);

    // callback: ok, docTitle, spreadsheetId, sheets(gid, name), error
    using ListSheetsCallback = std::function<void(bool, QString, QString, QVector<QPair<QString, QString>>, QString)>;
    void listSheetNames(const QString& url, ListSheetsCallback callback, int timeoutMs = 20000);

    using FetchSheetCallback = std::function<void(bool, FolderList, QString)>;
    void fetchSheetItems(const QString& spreadsheetId, const QString& sheetGid,
                         const QString& sheetName, const QString& imagesDir,
                         FetchSheetCallback callback, int timeoutMs = 20000);

private:
    QNetworkAccessManager* m_nam;

    using GetCallback = std::function<void(bool, QByteArray, QString)>;
    void httpGet(const QUrl& url, int timeoutMs, GetCallback callback);
    QString buildCsvUrl(const QString& spreadsheetId, const QString& gid) const;
};
