#include "snippets/GoogleSheetsService.h"
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>
#include <QSet>
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <private/qzipreader_p.h>
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

// ---------------------------------------------------------------------------
// XLSX 解析：只取 A、B 两列和嵌入的图片。CSV 导出拿不到直接插入单元格的图片，所以优先走 XLSX
// ---------------------------------------------------------------------------

const QString kRelNs = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");

struct XlsxRel {
    QString id;
    QString type;
    QString target;   // 已换算成压缩包内的绝对路径
};

// 关系文件里的 Target 相对于“被描述文件所在目录”，或以 / 开头表示从包根算起
QVector<XlsxRel> parseRels(const QByteArray& data, const QString& baseDir) {
    QVector<XlsxRel> rels;
    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QLatin1String("Relationship")) continue;
        XlsxRel r;
        r.id = xml.attributes().value("Id").toString();
        r.type = xml.attributes().value("Type").toString();
        const QString t = xml.attributes().value("Target").toString();
        r.target = t.startsWith('/') ? t.mid(1) : QDir::cleanPath(baseDir + "/" + t);
        rels.push_back(r);
    }
    return rels;
}

QString relTarget(const QVector<XlsxRel>& rels, const QString& id) {
    for (const XlsxRel& r : rels) if (r.id == id) return r.target;
    return QString();
}

int colLettersToIndex(const QString& ref) {
    int idx = 0;
    for (const QChar c : ref) {
        if (c < 'A' || c > 'Z') break;
        idx = idx * 26 + (c.unicode() - 'A' + 1);
    }
    return idx - 1;
}

QStringList parseSharedStrings(const QByteArray& data) {
    QStringList out;
    QXmlStreamReader xml(data);
    QString cur;
    bool inSi = false;
    while (!xml.atEnd()) {
        const auto tok = xml.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            if (xml.name() == QLatin1String("si")) { inSi = true; cur.clear(); }
            else if (inSi && xml.name() == QLatin1String("t")) cur += xml.readElementText();
        } else if (tok == QXmlStreamReader::EndElement && xml.name() == QLatin1String("si")) {
            inSi = false;
            out << cur;
        }
    }
    return out;
}

// 工作表里的 A、B 列，行号从 0 开始，缺的行补空串
QVector<QStringList> parseSheetRows(const QByteArray& data, const QStringList& shared, int minRows) {
    QHash<int, QStringList> rows;
    int maxRow = -1;
    int curRow = -1;
    int col = 0;
    QString type, val;
    bool inCell = false, inInline = false;

    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        const auto tok = xml.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            const auto name = xml.name();
            if (name == QLatin1String("row")) {
                const QString r = xml.attributes().value("r").toString();
                curRow = r.isEmpty() ? curRow + 1 : r.toInt() - 1;
                maxRow = qMax(maxRow, curRow);
            } else if (name == QLatin1String("c")) {
                col = colLettersToIndex(xml.attributes().value("r").toString());
                type = xml.attributes().value("t").toString();
                val.clear();
                inCell = true;
            } else if (inCell && name == QLatin1String("is")) {
                inInline = true;
            } else if (inCell && inInline && name == QLatin1String("t")) {
                val += xml.readElementText();
            } else if (inCell && !inInline && name == QLatin1String("v")) {
                val = xml.readElementText();
                if (type == QLatin1String("s")) {
                    const int i = val.toInt();
                    val = (i >= 0 && i < shared.size()) ? shared[i] : QString();
                }
            }
        } else if (tok == QXmlStreamReader::EndElement) {
            if (xml.name() == QLatin1String("is")) {
                inInline = false;
            } else if (xml.name() == QLatin1String("c")) {
                if (inCell && curRow >= 0 && col >= 0 && col < 2) {
                    QStringList& row = rows[curRow];
                    if (row.isEmpty()) row = QStringList{ QString(), QString() };
                    row[col] = val;
                }
                inCell = false;
            }
        }
    }

    const int total = qMax(maxRow + 1, minRows);
    QVector<QStringList> out;
    out.reserve(total);
    for (int i = 0; i < total; ++i) {
        QStringList row = rows.value(i);
        if (row.isEmpty()) row = QStringList{ QString(), QString() };
        out.push_back(row);
    }
    return out;
}

// 把工作表绘图里的图片抽成文件，返回 (行, 列) -> 相对路径。路径写成 <数据目录名>/images/<文件>，
// 和手动添加的图片一样相对于话术库根目录，换电脑也能找到
GoogleSheetsService::CellImages extractSheetImages(QZipReader& zip, const QString& sheetPath, const QString& imagesDir) {
    GoogleSheetsService::CellImages result;
    if (imagesDir.isEmpty()) return result;

    const QString sheetDir = QFileInfo(sheetPath).path();
    const QByteArray sheetRelsData = zip.fileData(sheetDir + "/_rels/" + QFileInfo(sheetPath).fileName() + ".rels");
    if (sheetRelsData.isEmpty()) return result;

    const QFileInfo imgInfo(imagesDir);
    const QString relPrefix = imgInfo.dir().dirName() + "/" + imgInfo.fileName() + "/";

    for (const XlsxRel& rel : parseRels(sheetRelsData, sheetDir)) {
        if (!rel.type.contains("drawing")) continue;
        const QByteArray drawing = zip.fileData(rel.target);
        if (drawing.isEmpty()) continue;

        const QString drawDir = QFileInfo(rel.target).path();
        const QVector<XlsxRel> drawRels =
            parseRels(zip.fileData(drawDir + "/_rels/" + QFileInfo(rel.target).fileName() + ".rels"), drawDir);

        QXmlStreamReader xml(drawing);
        bool inAnchor = false, inFrom = false;
        int row = -1, col = -1;
        QString embed;
        while (!xml.atEnd()) {
            const auto tok = xml.readNext();
            if (tok == QXmlStreamReader::StartElement) {
                const auto name = xml.name();
                if (name == QLatin1String("twoCellAnchor") || name == QLatin1String("oneCellAnchor")) {
                    inAnchor = true; row = col = -1; embed.clear();
                } else if (inAnchor && name == QLatin1String("from")) {
                    inFrom = true;
                } else if (inAnchor && inFrom && name == QLatin1String("col")) {
                    col = xml.readElementText().toInt();
                } else if (inAnchor && inFrom && name == QLatin1String("row")) {
                    row = xml.readElementText().toInt();
                } else if (inAnchor && name == QLatin1String("blip") && embed.isEmpty()) {
                    embed = xml.attributes().value(kRelNs, "embed").toString();
                }
            } else if (tok == QXmlStreamReader::EndElement) {
                if (xml.name() == QLatin1String("from")) {
                    inFrom = false;
                } else if (inAnchor && (xml.name() == QLatin1String("twoCellAnchor") || xml.name() == QLatin1String("oneCellAnchor"))) {
                    inAnchor = false;
                    const QString media = relTarget(drawRels, embed);
                    const QByteArray bytes = (row >= 0 && col >= 0 && !media.isEmpty()) ? zip.fileData(media) : QByteArray();
                    if (bytes.isEmpty()) continue;
                    QString ext = QFileInfo(media).suffix().toLower();
                    if (ext.isEmpty()) ext = "png";
                    const QString fname = "gsheet_" + newId().remove('-').left(10) + "." + ext;
                    QDir().mkpath(imagesDir);
                    QFile f(imagesDir + "/" + fname);
                    if (f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size())
                        result.insert({ row, col }, relPrefix + fname);
                }
            }
        }
    }
    return result;
}

// 解析整本 XLSX 里名为 sheetName 的工作表；压缩包不可读或找不到该表返回 false
bool parseXlsxSheet(const QByteArray& xlsx, const QString& sheetName, const QString& imagesDir,
                    QVector<QStringList>& rowsOut, GoogleSheetsService::CellImages& imagesOut) {
    QBuffer buffer;
    buffer.setData(xlsx);
    buffer.open(QIODevice::ReadOnly);
    QZipReader zip(&buffer);
    if (!zip.isReadable()) return false;

    const QByteArray workbook = zip.fileData("xl/workbook.xml");
    if (workbook.isEmpty()) return false;
    const QVector<XlsxRel> wbRels = parseRels(zip.fileData("xl/_rels/workbook.xml.rels"), "xl");

    QString sheetPath;
    QXmlStreamReader xml(workbook);
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QLatin1String("sheet")) continue;
        if (xml.attributes().value("name").toString() == sheetName) {
            sheetPath = relTarget(wbRels, xml.attributes().value(kRelNs, "id").toString());
            break;
        }
    }
    if (sheetPath.isEmpty()) return false;
    const QByteArray sheetData = zip.fileData(sheetPath);
    if (sheetData.isEmpty()) return false;

    imagesOut = extractSheetImages(zip, sheetPath, imagesDir);
    int minRows = 0;
    for (auto it = imagesOut.cbegin(); it != imagesOut.cend(); ++it) minRows = qMax(minRows, it.key().first + 1);
    rowsOut = parseSheetRows(sheetData, parseSharedStrings(zip.fileData("xl/sharedStrings.xml")), minRows);
    return true;
}

FolderList buildSheetFolder(const QString& sheetName, const QVector<QStringList>& rows,
                            const GoogleSheetsService::CellImages& images) {
    auto l2 = std::make_unique<FolderNode>();
    l2->id = newId();
    l2->level = 2;
    l2->title = sheetName;
    l2->collapsed = false;
    GoogleSheetsService::parseRowsIntoFolder(l2.get(), rows, images);

    FolderList result;
    result.push_back(std::move(l2));
    return result;
}

// 从 xlsx 的 workbook.xml 读全部标签名。未"发布到网络"的表格拿不到 pubhtml，这是唯一可靠的来源
QVector<QPair<QString, QString>> parseWorkbookSheets(const QByteArray& xlsx) {
    QVector<QPair<QString, QString>> out;
    QBuffer buffer;
    buffer.setData(xlsx);
    buffer.open(QIODevice::ReadOnly);
    QZipReader zip(&buffer);
    if (!zip.isReadable()) return out;

    QXmlStreamReader xml(zip.fileData("xl/workbook.xml"));
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QLatin1String("sheet")) continue;
        // xlsx 里的 sheetId 不是 Google 的 gid；这里的 gid 只在 CSV 兜底时用，xlsx 路径按名称匹配
        out.append({ xml.attributes().value("sheetId").toString(), xml.attributes().value("name").toString() });
    }
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
                [this, sheetId, timeoutMs, callback, editOk, err1, docTitle, htmlText](bool pubOk, QByteArray pubBody, QString err2) {
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
                    const QString title = docTitle.isEmpty() ? QString("Google表格 (%1)").arg(sheetId.left(6)) : docTitle;

                    // 4) 前三步只认到 0/1 张时（未发布到网络的表格常见），再用整本 xlsx 的标签名核对
                    if (sheets.size() <= 1) {
                        loadXlsx(sheetId, timeoutMs, [sheets, title, sheetId, callback](const QByteArray& xlsx) mutable {
                            const auto names = parseWorkbookSheets(xlsx);
                            if (names.size() > sheets.size()) sheets = names;
                            if (sheets.isEmpty()) sheets.append(qMakePair(QString("0"), QString("Sheet1")));
                            callback(true, title, sheetId, sheets, QString());
                        });
                        return;
                    }
                    callback(true, title, sheetId, sheets, QString());
                });
        });
}

QString GoogleSheetsService::buildCsvUrl(const QString& spreadsheetId, const QString& gid) const {
    return QString("https://docs.google.com/spreadsheets/d/%1/gviz/tq?tqx=out:csv&gid=%2")
        .arg(spreadsheetId, gid);
}

void GoogleSheetsService::loadXlsx(const QString& spreadsheetId, int timeoutMs, XlsxCallback callback) {
    constexpr qint64 kCacheMs = 120 * 1000;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_xlsxCache.constFind(spreadsheetId);
    if (it != m_xlsxCache.cend() && now - it->savedAt < kCacheMs) {
        callback(it->data);
        return;
    }
    const QUrl url(QString("https://docs.google.com/spreadsheets/d/%1/export?format=xlsx").arg(spreadsheetId));
    // 整本文件比单张 CSV 大得多，超时放宽
    httpGet(url, timeoutMs * 3, [this, spreadsheetId, callback = std::move(callback)](bool ok, QByteArray data, QString) {
        // 无权限时 Google 返回登录页 HTML，不是 zip（以 PK 开头）
        if (!ok || !data.startsWith("PK")) data.clear();
        m_xlsxCache.insert(spreadsheetId, { QDateTime::currentMSecsSinceEpoch(), data });
        callback(data);
    });
}

// 先试 XLSX（能带出直接插入单元格的图片），任何一步失败都退回 CSV
void GoogleSheetsService::fetchSheetItems(const QString& spreadsheetId,
                                          const QString& sheetGid,
                                          const QString& sheetName,
                                          const QString& imagesDir,
                                          FetchSheetCallback callback,
                                          int timeoutMs) {
    loadXlsx(spreadsheetId, timeoutMs,
        [=, this, callback = std::move(callback)](const QByteArray& xlsx) mutable {
            QVector<QStringList> rows;
            CellImages images;
            if (!xlsx.isEmpty() && parseXlsxSheet(xlsx, sheetName, imagesDir, rows, images)) {
                callback(true, buildSheetFolder(sheetName, rows, images), QString());
                return;
            }
            fetchSheetCsv(spreadsheetId, sheetGid, sheetName, std::move(callback), timeoutMs);
        });
}

void GoogleSheetsService::fetchSheetCsv(const QString& spreadsheetId, const QString& sheetGid,
                                        const QString& sheetName, FetchSheetCallback callback, int timeoutMs) {
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
            callback(true, buildSheetFolder(sheetName, parseCsv(csvText), {}), QString());
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

void GoogleSheetsService::parseRowsIntoFolder(FolderNode* l2Folder, const QVector<QStringList>& rows,
                                              const CellImages& cellImages) {
    FolderNode* currentL3 = nullptr;   // 所有权在 l2Folder->children
    static const QRegularExpression imgRe(R"(^=IMAGE\(["']([^"']+)["'])", QRegularExpression::CaseInsensitiveOption);

    for (int r = 0; r < rows.size(); ++r) {
        const QStringList& cells = rows[r];
        QString colA = cells.size() > 0 ? cells[0].trimmed() : QString();
        QString colB = cells.size() > 1 ? cells[1].trimmed() : QString();
        QString imgPath = cellImages.value({ r, 1 });
        if (imgPath.isEmpty()) imgPath = cellImages.value({ r, 0 });

        if (imgPath.isEmpty() && !colB.isEmpty()) {
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
