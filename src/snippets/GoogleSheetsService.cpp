#include "snippets/GoogleSheetsService.h"
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>
#include <QSet>
#include <memory>

namespace {

const char kUserAgent[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString htmlUnescape(QString s) {
    static const QRegularExpression numRe("&#(x?)([0-9a-fA-F]+);");
    QString out;
    int last = 0;
    auto it = numRe.globalMatch(s);
    while (it.hasNext()) {
        auto m = it.next();
        out += s.mid(last, m.capturedStart() - last);
        bool ok = false;
        uint cp = m.captured(2).toUInt(&ok, m.captured(1).isEmpty() ? 10 : 16);
        out += ok ? QString::fromUcs4(reinterpret_cast<const char32_t*>(&cp), 1) : m.captured(0);
        last = m.capturedEnd();
    }
    out += s.mid(last);
    out.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"")
       .replace("&#39;", "'").replace("&nbsp;", " ").replace("&amp;", "&");
    return out;
}

} // namespace

GoogleSheetsService::GoogleSheetsService(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

GoogleSheetsService::~GoogleSheetsService() = default;

QString GoogleSheetsService::extractSpreadsheetId(const QString& urlOrId) {
    QString s = urlOrId.trimmed();
    static const QRegularExpression re(R"(/spreadsheets/d/([a-zA-Z0-9\-_]+))");
    QRegularExpressionMatch m = re.match(s);
    if (m.hasMatch()) return m.captured(1);

    static const QRegularExpression idRe(R"(^[a-zA-Z0-9\-_]{15,}$)");
    if (idRe.match(s).hasMatch()) return s;
    return QString();
}

void GoogleSheetsService::httpGet(const QUrl& url, int timeoutMs, GetCallback callback) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", kUserAgent);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_nam->get(request);
    // 计时器挂在 reply 上，服务销毁时请求连同回调一起作废，不会回调到已销毁的调用方
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    auto timedOut = std::make_shared<bool>(false);
    connect(timer, &QTimer::timeout, reply, [reply, timedOut]() {
        *timedOut = true;
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [reply, timer, timedOut, callback = std::move(callback)]() {
        timer->stop();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body;
        QString error;
        bool ok = false;
        if (*timedOut) {
            error = QString::fromUtf8("网络请求超时");
        } else if (reply->error() != QNetworkReply::NoError) {
            error = reply->errorString();
        } else if (code != 200) {
            error = QString::fromUtf8("HTTP 错误: %1").arg(code);
        } else {
            body = reply->readAll();
            ok = true;
        }
        reply->deleteLater();
        if (callback) callback(ok, body, error);
    });
    timer->start(timeoutMs);
}

void GoogleSheetsService::listSheetNames(const QString& url, ListSheetsCallback callback, int timeoutMs) {
    const QString sheetId = extractSpreadsheetId(url);
    if (sheetId.isEmpty()) {
        if (callback) callback(false, QString(), QString(), {},
                               QString::fromUtf8("无效的 Google 表格链接，请检查是否包含 /spreadsheets/d/..."));
        return;
    }

    // 1) /edit：取标题（失败不致命，下一步 pubhtml 还能拿到标签列表）
    httpGet(QUrl(QString("https://docs.google.com/spreadsheets/d/%1/edit").arg(sheetId)), timeoutMs,
        [this, sheetId, timeoutMs, callback](bool editOk, QByteArray editBody, QString err1) {
            QString docTitle;
            QString htmlText;
            if (editOk) {
                htmlText = QString::fromUtf8(editBody);
                static const QRegularExpression titleRe("<title>(.*?)</title>", QRegularExpression::DotMatchesEverythingOption);
                auto tm = titleRe.match(htmlText);
                if (tm.hasMatch()) {
                    QString rawT = htmlUnescape(tm.captured(1)).trimmed();
                    static const QRegularExpression suffixRe(
                        QString::fromUtf8(R"(\s*-\s*Google\s*(表格|Sheets|雲端硬碟|Drive).*$)"),
                        QRegularExpression::CaseInsensitiveOption);
                    rawT.remove(suffixRe);
                    if (!rawT.isEmpty() && !rawT.contains("Google")) docTitle = rawT;
                }
            }

            // 2) pubhtml：干净的 Sheet 标签列表
            httpGet(QUrl(QString("https://docs.google.com/spreadsheets/d/%1/pubhtml").arg(sheetId)), timeoutMs,
                [sheetId, callback, editOk, err1, docTitle, htmlText](bool pubOk, QByteArray pubBody, QString err2) {
                    QVector<QPair<QString, QString>> sheets;
                    if (pubOk) {
                        const QString pubHtml = QString::fromUtf8(pubBody);
                        static const QRegularExpression sheetRe(R"re(id="sheet-button-(\d+)"[^>]*>.*?<a[^>]*>(.*?)</a>)re",
                                                                QRegularExpression::DotMatchesEverythingOption);
                        auto it = sheetRe.globalMatch(pubHtml);
                        while (it.hasNext()) {
                            auto m = it.next();
                            sheets.append({ m.captured(1), htmlUnescape(m.captured(2)).trimmed() });
                        }
                    }

                    // 3) 回退：/edit 引导数据
                    if (sheets.isEmpty() && !htmlText.isEmpty()) {
                        static const QRegularExpression re2(R"re(\[(\d+),\s*\[\["([^"]+)")re");
                        QSet<QString> seen;
                        auto it = re2.globalMatch(htmlText);
                        while (it.hasNext()) {
                            auto m = it.next();
                            if (seen.contains(m.captured(1))) continue;
                            seen.insert(m.captured(1));
                            sheets.append({ m.captured(1), m.captured(2) });
                        }
                    }

                    if (!callback) return;
                    if (!editOk && !pubOk) {
                        callback(false, QString(), sheetId, {}, err1.isEmpty() ? err2 : err1);
                        return;
                    }
                    if (sheets.isEmpty()) sheets.append(qMakePair(QString("0"), QString("Sheet1")));
                    callback(true, docTitle.isEmpty() ? QString("Google表格 (%1)").arg(sheetId.left(6)) : docTitle,
                             sheetId, sheets, QString());
                });
        });
}

QString GoogleSheetsService::buildCsvUrl(const QString& spreadsheetId, const QString& gid) const {
    return QString("https://docs.google.com/spreadsheets/d/%1/gviz/tq?tqx=out:csv&gid=%2")
        .arg(spreadsheetId, gid);
}

void GoogleSheetsService::fetchSheetItems(const QString& spreadsheetId,
                                          const QString& sheetGid,
                                          const QString& sheetName,
                                          const QString& /*imagesDir*/,
                                          FetchSheetCallback callback,
                                          int timeoutMs) {
    httpGet(QUrl(buildCsvUrl(spreadsheetId, sheetGid)), timeoutMs,
        [sheetName, callback = std::move(callback)](bool ok, QByteArray data, QString err) {
            if (!callback) return;
            if (!ok) {
                callback(false, FolderList(), err);
                return;
            }

            QString csvText = QString::fromUtf8(data);
            if (csvText.startsWith(QChar(0xFEFF))) csvText.remove(0, 1);
            if (csvText.trimmed().startsWith('<')) {
                callback(false, FolderList(),
                    QString::fromUtf8("无法读取该表格内容，请确认共享权限已设为「知道链接的任何人均可查看」"));
                return;
            }

            auto l2 = std::make_unique<FolderNode>();
            l2->id = newId();
            l2->level = 2;
            l2->title = sheetName;
            l2->collapsed = false;
            parseRowsIntoFolder(l2.get(), parseCsv(csvText));

            FolderList result;
            result.push_back(std::move(l2));
            callback(true, std::move(result), QString());
        });
}

QVector<QStringList> GoogleSheetsService::parseCsv(const QString& text) {
    QVector<QStringList> rows;
    QStringList row;
    QString cell;
    bool inQuotes = false;
    bool any = false;   // 当前行是否出现过字符

    auto endCell = [&]() { row << cell; cell.clear(); };
    auto endRow = [&]() {
        if (any || !row.isEmpty()) {
            endCell();
            rows.push_back(row);
        }
        row.clear();
        any = false;
    };

    for (int i = 0; i < text.size(); ++i) {
        QChar c = text[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') { cell += '"'; ++i; }
                else inQuotes = false;
            } else {
                cell += c;
            }
            continue;
        }
        if (c == '"') { inQuotes = true; any = true; }
        else if (c == ',') { endCell(); any = true; }
        else if (c == '\r') { if (i + 1 < text.size() && text[i + 1] == '\n') ++i; endRow(); }
        else if (c == '\n') { endRow(); }
        else { cell += c; any = true; }
    }
    endRow();
    return rows;
}

void GoogleSheetsService::parseRowsIntoFolder(FolderNode* l2Folder, const QVector<QStringList>& rows) {
    FolderNode* currentL3 = nullptr;   // 所有权在 l2Folder->children
    static const QRegularExpression imgRe(R"(^=IMAGE\(["']([^"']+)["'])", QRegularExpression::CaseInsensitiveOption);

    for (int r = 0; r < rows.size(); ++r) {
        const QStringList& cells = rows[r];
        QString colA = cells.size() > 0 ? cells[0].trimmed() : QString();
        QString colB = cells.size() > 1 ? cells[1].trimmed() : QString();
        QString imgPath;

        if (!colB.isEmpty()) {
            auto m = imgRe.match(colB);
            if (m.hasMatch()) {
                imgPath = m.captured(1);
                colB.clear();          // 公式本身不作为外文文本
            }
        }

        if (colA.isEmpty() && colB.isEmpty() && imgPath.isEmpty()) continue;

        if (colA.contains("@@")) {
            QString folderTitle = QString(colA).replace("@@", "").trimmed();
            if (folderTitle.isEmpty()) folderTitle = colB.isEmpty() ? QString::fromUtf8("未命名分组") : colB;

            auto folder = std::make_unique<FolderNode>();
            folder->id = newId();
            folder->level = 3;
            folder->title = folderTitle;
            folder->collapsed = false;
            currentL3 = folder.get();
            l2Folder->children.push_back(std::move(folder));
            continue;
        }

        if (r == 0 &&
            (colA == QString::fromUtf8("中文") || colA == QString::fromUtf8("中文说明") || colA == QString::fromUtf8("标题")) &&
            (colB == QString::fromUtf8("外文") || colB == QString::fromUtf8("外语") ||
             colB == QString::fromUtf8("内容") || colB == QString::fromUtf8("话术")))
            continue;

        SnippetItem item;
        item.id = newId();
        item.title = (colA.isEmpty() ? (colB.isEmpty() ? QString::fromUtf8("图片消息") : colB.left(24)) : colA);
        item.text = colB;
        item.imagePath = imgPath;
        item.type = (!imgPath.isEmpty() && !colB.isEmpty()) ? "combo" : (!imgPath.isEmpty() ? "image" : "text");
        item.useCount = 0;

        (currentL3 ? currentL3 : l2Folder)->items.push_back(std::move(item));
    }
}
