// SnippetStore 的 Google 表格部分：配置记录 + 异步拉取合并。与树的增删改分开放，避免主文件过长。
#include "snippets/SnippetStore.h"
#include "snippets/GoogleSheetsService.h"
#include <QDateTime>

namespace {

QString nowString() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

} // namespace


bool SnippetStore::addGoogleSheet(const GoogleSheetConfig& gs) {
    m_googleSheets.push_back(gs);
    return save();
}

bool SnippetStore::updateGoogleSheet(const QString& treeId, const GoogleSheetConfig& gs) {
    if (GoogleSheetConfig* g = findGoogleSheetByTreeId(treeId)) {
        *g = gs;
        return save();
    }
    return addGoogleSheet(gs);
}

bool SnippetStore::removeGoogleSheet(const QString& treeId) {
    for (int i = 0; i < m_googleSheets.size(); ++i) {
        if (m_googleSheets[i].treeId == treeId) {
            m_googleSheets.removeAt(i);
            return save();
        }
    }
    return false;
}

SnippetStore::GoogleSheetConfig* SnippetStore::findGoogleSheetByTreeId(const QString& treeId) {
    for (auto& g : m_googleSheets)
        if (g.treeId == treeId) return &g;
    return nullptr;
}

SnippetStore::GoogleSheetConfig* SnippetStore::findGoogleSheetByUrl(const QString& url) {
    for (auto& g : m_googleSheets)
        if (g.url == url) return &g;
    return nullptr;
}

QString SnippetStore::syncGoogleSheet(const QString& url,
                                      const QString& customAlias,
                                      const QString& existingTreeId,
                                      const QStringList& selectedSheets,
                                      const QStringList& allSheetNames,
                                      const QString& fetchedDocTitle,
                                      FolderList newSheets) {
    QString sheetId = GoogleSheetsService::extractSpreadsheetId(url);

    GoogleSheetConfig* gsEntry = nullptr;
    for (auto& g : m_googleSheets) {
        if ((!existingTreeId.isEmpty() && g.treeId == existingTreeId) ||
            (!sheetId.isEmpty() && g.spreadsheetId == sheetId)) {
            gsEntry = &g;
            break;
        }
    }

    QStringList sel = selectedSheets;
    if (sel.isEmpty() && gsEntry) sel = gsEntry->selectedSheets;

    QStringList allNames = allSheetNames;
    if (allNames.isEmpty()) {
        for (const auto& s : newSheets) allNames << s->title;
    }
    if (sel.isEmpty()) sel = allNames;

    QString docTitle = customAlias.trimmed();
    if (docTitle.isEmpty() && gsEntry) docTitle = gsEntry->title;
    if (docTitle.isEmpty()) docTitle = fetchedDocTitle;
    if (docTitle.isEmpty()) docTitle = QString("Google表格 (%1)").arg(sheetId.left(6));

    QString targetTreeId = gsEntry ? gsEntry->treeId
                                   : (!existingTreeId.isEmpty() ? existingTreeId : newUuid());

    auto newRoot = std::make_unique<FolderNode>();
    newRoot->id = targetTreeId;
    newRoot->level = 1;
    newRoot->title = docTitle;
    newRoot->sourceUrl = url.trimmed();
    newRoot->spreadsheetId = sheetId;
    newRoot->collapsed = false;
    for (auto& s : newSheets) newRoot->children.push_back(std::move(s));

    bool replaced = false;
    for (auto& r : m_roots) {
        bool match = (r->id == newRoot->id) ||
                     (!newRoot->spreadsheetId.isEmpty() && r->spreadsheetId == newRoot->spreadsheetId);
        if (match) {
            newRoot->id = r->id;
            targetTreeId = r->id;
            r = std::move(newRoot);
            replaced = true;
            break;
        }
    }
    if (!replaced) m_roots.push_back(std::move(newRoot));

    QString now = nowString();
    if (gsEntry) {
        gsEntry->url = url.trimmed();
        gsEntry->title = docTitle;
        gsEntry->spreadsheetId = sheetId;
        gsEntry->treeId = targetTreeId;
        gsEntry->selectedSheets = sel;
        gsEntry->allSheets = allNames;
        gsEntry->lastSync = now;
    } else {
        GoogleSheetConfig g;
        g.id = newUuid();
        g.treeId = targetTreeId;
        g.spreadsheetId = sheetId;
        g.title = docTitle;
        g.url = url.trimmed();
        g.selectedSheets = sel;
        g.allSheets = allNames;
        g.lastSync = now;
        m_googleSheets.push_back(g);
    }

    save();
    return targetTreeId;
}

struct SnippetStore::SheetFetch {
    QString url, alias, existingTreeId, sheetId;
    QStringList sel, allNames;
    QString docTitle, error;
    bool ok = true;
    QVector<QPair<QString, QString>> todo;   // 要拉的 (gid, 名称)
    int next = 0;
    FolderList fetched;
};

struct SnippetStore::RefreshJob {
    QVector<GoogleSheetConfig> configs;
    int index = 0;
    QVector<std::shared_ptr<SheetFetch>> results;
    RefreshCallback done;
};

void SnippetStore::fetchGoogleSheet(GoogleSheetsService* service, std::shared_ptr<SheetFetch> job, FetchDone done) {
    auto fail = [&](const QString& msg) {
        job->ok = false;
        job->error = msg;
        done(job);
    };
    if (!service) return fail(QString::fromUtf8("内部错误：未提供网络服务"));

    job->sheetId = GoogleSheetsService::extractSpreadsheetId(job->url);
    if (job->sheetId.isEmpty())
        return fail(QString::fromUtf8("无效的 Google 表格链接，请检查是否包含 /spreadsheets/d/..."));

    if (job->sel.isEmpty()) {
        for (const auto& g : m_googleSheets) {
            if ((!job->existingTreeId.isEmpty() && g.treeId == job->existingTreeId) || g.spreadsheetId == job->sheetId) {
                job->sel = g.selectedSheets;
                break;
            }
        }
    }

    service->listSheetNames(job->url,
        [this, service, job, done](bool ok, QString title, QString, QVector<QPair<QString, QString>> sheets, QString err) {
            if (!ok) {
                job->ok = false;
                job->error = err;
                done(job);
                return;
            }
            job->docTitle = title;
            for (const auto& p : sheets) job->allNames << p.second;
            if (job->sel.isEmpty()) job->sel = job->allNames;
            for (const auto& p : sheets)
                if (job->sel.contains(p.second)) job->todo.push_back(p);
            fetchNextSheet(service, job, done);
        });
}

// 一张一张拉：每张在自己的回调里续下一张，不阻塞界面线程
void SnippetStore::fetchNextSheet(GoogleSheetsService* service, std::shared_ptr<SheetFetch> job, FetchDone done) {
    if (job->next >= job->todo.size()) {
        done(job);
        return;
    }
    const QPair<QString, QString> p = job->todo[job->next++];
    service->fetchSheetItems(job->sheetId, p.first, p.second, m_imagesDir,
        [this, service, job, done, name = p.second](bool ok, FolderList list, QString err) {
            if (!ok) {
                job->ok = false;
                job->error = QString::fromUtf8("读取 Sheet「%1」失败：%2").arg(name, err);
                done(job);
                return;
            }
            for (auto& f : list) job->fetched.push_back(std::move(f));
            fetchNextSheet(service, job, done);
        });
}

QString SnippetStore::applyFetched(SheetFetch& job) {
    return syncGoogleSheet(job.url, job.alias, job.existingTreeId, job.sel, job.allNames, job.docTitle,
                           std::move(job.fetched));
}

void SnippetStore::importGoogleSheet(GoogleSheetsService* service,
                                     const QString& url,
                                     const QString& customAlias,
                                     const QString& existingTreeId,
                                     const QStringList& selectedNames,
                                     ImportCallback done) {
    auto job = std::make_shared<SheetFetch>();
    job->url = url;
    job->alias = customAlias;
    job->existingTreeId = existingTreeId;
    job->sel = selectedNames;
    fetchGoogleSheet(service, job, [this, done](std::shared_ptr<SheetFetch> j) {
        if (!j->ok) {
            done(QString(), j->error);
            return;
        }
        done(applyFetched(*j), QString());
    });
}

void SnippetStore::refreshAllGoogleSheets(GoogleSheetsService* service, RefreshCallback done) {
    auto job = std::make_shared<RefreshJob>();
    job->configs = m_googleSheets;   // 快照：拉取期间配置列表不应影响遍历
    job->done = std::move(done);
    refreshNext(service, job);
}

void SnippetStore::refreshNext(GoogleSheetsService* service, std::shared_ptr<RefreshJob> job) {
    if (job->index >= job->configs.size()) {
        // 全部拉完才改树，且紧接着回调，调用方在回调里刷新模型
        int okCount = 0;
        QStringList errors;
        for (const auto& r : job->results) {
            if (!r->ok) {
                errors << QString("%1: %2").arg(r->alias.isEmpty() ? r->url : r->alias, r->error);
            } else if (!applyFetched(*r).isEmpty()) {
                ++okCount;
            }
        }
        job->done(okCount, errors);
        return;
    }

    const GoogleSheetConfig& gs = job->configs[job->index++];
    auto fetch = std::make_shared<SheetFetch>();
    fetch->url = gs.url;
    fetch->alias = gs.title;
    fetch->existingTreeId = gs.treeId;
    fetch->sel = gs.selectedSheets;
    fetchGoogleSheet(service, fetch, [this, service, job](std::shared_ptr<SheetFetch> j) {
        job->results.push_back(j);
        refreshNext(service, job);
    });
}
