#include "snippets/SnippetStore.h"
#include "snippets/GoogleSheetsService.h"
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QCryptographicHash>
#include <QDateTime>
#include <QUuid>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QSet>
#include <algorithm>
#include <functional>
#include "utils/Logger.h"

namespace {

QString nowString() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

QString typeFor(const SnippetItem& it) {
    bool hasText = !QString::fromStdString(it.text).trimmed().isEmpty();
    bool hasImg = !QString::fromStdString(it.imagePath).trimmed().isEmpty();
    return (hasText && hasImg) ? "combo" : (hasImg ? "image" : "text");
}

bool findFolderRec(const std::string& id, FolderList& list, FolderNode* parent,
                   FolderNode** n, FolderNode** p, FolderList** l, int* idx) {
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
        FolderNode* f = list[i].get();
        if (f->id == id) {
            if (n) *n = f;
            if (p) *p = parent;
            if (l) *l = &list;
            if (idx) *idx = i;
            return true;
        }
        if (findFolderRec(id, f->children, f, n, p, l, idx)) return true;
    }
    return false;
}

bool findItemRec(const std::string& id, FolderNode& f,
                 SnippetItem** it, FolderNode** folder, int* idx) {
    for (int i = 0; i < static_cast<int>(f.items.size()); ++i) {
        if (f.items[i].id == id) {
            if (it) *it = &f.items[i];
            if (folder) *folder = &f;
            if (idx) *idx = i;
            return true;
        }
    }
    for (auto& c : f.children) {
        if (findItemRec(id, *c, it, folder, idx)) return true;
    }
    return false;
}

int pruneRec(FolderList& list, const QSet<QString>& ids) {
    int count = 0;
    for (size_t i = 0; i < list.size();) {
        if (ids.contains(QString::fromStdString(list[i]->id))) {
            list.erase(list.begin() + i);
            ++count;
            continue;
        }
        FolderNode& f = *list[i];
        for (size_t j = 0; j < f.items.size();) {
            if (ids.contains(QString::fromStdString(f.items[j].id))) {
                f.items.erase(f.items.begin() + j);
                ++count;
            } else {
                ++j;
            }
        }
        count += pruneRec(f.children, ids);
        ++i;
    }
    return count;
}

bool trailRec(const FolderList& list, const std::string& id, QStringList cur, QStringList& result) {
    for (const auto& f : list) {
        for (const auto& it : f->items) {
            if (it.id == id) { result = cur << QString::fromStdString(f->title); return true; }
        }
        if (f->id == id) { result = cur; return true; }
        QStringList next = cur;
        next << QString::fromStdString(f->title);
        if (trailRec(f->children, id, next, result)) return true;
    }
    return false;
}

} // namespace

SnippetStore::SnippetStore(const QString& dataFilePath)
    : m_dataFilePath(QFileInfo(dataFilePath).absoluteFilePath()) {
    m_dataDir = QFileInfo(m_dataFilePath).absolutePath();
    m_baseDir = QFileInfo(m_dataDir).dir().absolutePath();   // 不用 QDir::cdUp：目录尚不存在时它会失败
    m_imagesDir = m_dataDir + "/images";

    // 展开/收起、使用次数、设置微调都会 save()，合并成一次写盘
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    QObject::connect(&m_saveTimer, &QTimer::timeout, &m_saveTimer, [this]() { flush(); });
}

SnippetStore::~SnippetStore() {
    flush();
}

void SnippetStore::ensureDirs() const {
    QDir().mkpath(m_dataDir);
    QDir().mkpath(m_imagesDir);
}

QString SnippetStore::newUuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString SnippetStore::resolveImagePath(const QString& relOrAbs) const {
    if (relOrAbs.isEmpty()) return QString();
    if (relOrAbs.startsWith("http://") || relOrAbs.startsWith("https://")) return relOrAbs;
    if (QDir::isAbsolutePath(relOrAbs)) return relOrAbs;
    return m_baseDir + "/" + relOrAbs;
}

QString SnippetStore::remoteImageCachePath(const QString& url) const {
    return m_imagesDir + "/url_"
        + QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex()) + ".png";
}

QString SnippetStore::copyImageToStore(const QString& srcPath) {
    if (srcPath.isEmpty() || !QFileInfo::exists(srcPath)) return QString();
    ensureDirs();
    QString ext = QFileInfo(srcPath).suffix().toLower();
    static const QStringList valid = { "png", "jpg", "jpeg", "bmp", "gif", "webp" };
    if (!valid.contains(ext)) ext = "png";

    QString newName = "img_" + newUuid().remove('-').left(10) + "." + ext;
    if (!QFile::copy(srcPath, m_imagesDir + "/" + newName)) return QString();
    return "data/images/" + newName;
}

QString SnippetStore::savePixmapToStore(const QPixmap& pixmap) {
    ensureDirs();
    QString newName = "clip_" + newUuid().remove('-').left(10) + ".png";
    if (!pixmap.save(m_imagesDir + "/" + newName, "PNG")) return QString();
    return "data/images/" + newName;
}

// ------------------------------------------------------------------
// JSON serialization
// ------------------------------------------------------------------
QJsonObject SnippetStore::folderToJson(const FolderNode* folder) const {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(folder->id);
    obj["node_type"] = "folder";
    obj["level"] = folder->level;
    obj["title"] = QString::fromStdString(folder->title);
    obj["collapsed"] = folder->collapsed;
    if (folder->level == 1 || !folder->sourceUrl.empty())
        obj["source_url"] = QString::fromStdString(folder->sourceUrl);
    if (!folder->spreadsheetId.empty())
        obj["spreadsheet_id"] = QString::fromStdString(folder->spreadsheetId);

    QJsonArray childrenArr;
    for (const auto& child : folder->children)
        childrenArr.append(folderToJson(child.get()));
    for (const auto& item : folder->items)
        childrenArr.append(itemToJson(&item));
    obj["children"] = childrenArr;
    return obj;
}

QJsonObject SnippetStore::itemToJson(const SnippetItem* item) const {
    QJsonObject obj;
    obj["id"] = QString::fromStdString(item->id);
    obj["node_type"] = "item";
    obj["keyword"] = QString::fromStdString(item->keyword);
    obj["title"] = QString::fromStdString(item->title);
    obj["text"] = QString::fromStdString(item->text);
    obj["image_path"] = QString::fromStdString(item->imagePath);
    obj["type"] = QString::fromStdString(item->type);
    obj["use_count"] = item->useCount;
    return obj;
}

std::unique_ptr<FolderNode> SnippetStore::jsonToFolder(const QJsonObject& obj, int level) const {
    auto folder = std::make_unique<FolderNode>();
    folder->id = obj["id"].toString(newUuid()).toStdString();
    folder->level = obj["level"].toInt(level);
    folder->title = obj["title"].toString().toStdString();
    folder->collapsed = obj["collapsed"].toBool(false);
    folder->sourceUrl = obj["source_url"].toString().toStdString();
    folder->spreadsheetId = obj["spreadsheet_id"].toString().toStdString();

    const QJsonArray children = obj["children"].toArray();
    for (const QJsonValue& val : children) {
        QJsonObject childObj = val.toObject();
        if (childObj["node_type"].toString("folder") == "folder")
            folder->children.push_back(jsonToFolder(childObj, folder->level + 1));
        else
            folder->items.push_back(jsonToItem(childObj));
    }
    return folder;
}

SnippetItem SnippetStore::jsonToItem(const QJsonObject& obj) const {
    SnippetItem item;
    item.id = obj["id"].toString(newUuid()).toStdString();
    item.keyword = obj["keyword"].toString().toStdString();
    item.title = obj["title"].toString().toStdString();
    item.text = obj["text"].toString().toStdString();
    item.imagePath = obj["image_path"].toString().toStdString();
    item.type = obj["type"].toString("text").toStdString();
    item.useCount = obj["use_count"].toInt(0);
    return item;
}

// ------------------------------------------------------------------
// load / save
// ------------------------------------------------------------------
// 返回 true 表示发生了旧格式迁移，需要回写
bool SnippetStore::loadFromObject(QJsonObject root) {
    bool migrated = false;

    // settings（缺失的键保留默认值）
    m_settings = AppSettings();
    const QJsonObject s = root["settings"].toObject();
    if (s.contains("hotkey")) m_settings.hotkey = s["hotkey"].toString("").toStdString();
    if (s.contains("trigger_prefixes")) {
        m_settings.triggerPrefixes.clear();
        for (const QJsonValue& v : s["trigger_prefixes"].toArray())
            m_settings.triggerPrefixes.push_back(v.toString().toStdString());
    }
    m_settings.enablePrefixTrigger = s["enable_prefix_trigger"].toBool(true);
    m_settings.autoDeletePrefix = s["auto_delete_prefix"].toBool(true);
    m_settings.comboImageFirst = s["combo_image_first"].toBool(false);
    m_settings.multilineEnabled = s["multiline_enabled"].toBool(true);
    m_settings.maxDisplayLines = s["max_display_lines"].toInt(3);
    m_settings.voiceTrigger = s["voice_trigger"].toBool(true);

    // 旧版扁平 snippets -> trees 迁移
    if (!root.contains("trees")) {
        QJsonArray trees = getDefaultData()["trees"].toArray();
        const QJsonArray old = root["snippets"].toArray();
        if (!old.isEmpty() && !trees.isEmpty()) {
            QJsonArray migratedChildren;
            for (const QJsonValue& v : old) {
                QJsonObject o = v.toObject();
                o["node_type"] = "item";
                if (!o.contains("id")) o["id"] = newUuid();
                migratedChildren.append(o);
            }
            QJsonObject preset;
            preset["id"] = newUuid();
            preset["node_type"] = "folder";
            preset["level"] = 2;
            preset["title"] = "原有预设";
            preset["collapsed"] = false;
            preset["children"] = migratedChildren;

            QJsonObject l1 = trees[0].toObject();
            QJsonArray ch = l1["children"].toArray();
            ch.prepend(preset);
            l1["children"] = ch;
            trees[0] = l1;
        }
        root["trees"] = trees;
        migrated = true;
    }

    m_roots.clear();
    for (const QJsonValue& val : root["trees"].toArray()) {
        QJsonObject obj = val.toObject();
        if (obj["node_type"].toString("folder") == "folder")
            m_roots.push_back(jsonToFolder(obj, 1));
    }

    m_googleSheets.clear();
    for (const QJsonValue& v : root["google_sheets"].toArray()) {
        QJsonObject o = v.toObject();
        GoogleSheetConfig gs;
        gs.id = o["id"].toString();
        gs.treeId = o["tree_id"].toString();
        gs.spreadsheetId = o["spreadsheet_id"].toString();
        gs.title = o["title"].toString();
        gs.url = o["url"].toString();
        for (const QJsonValue& x : o["selected_sheets"].toArray()) gs.selectedSheets << x.toString();
        for (const QJsonValue& x : o["all_sheets"].toArray()) gs.allSheets << x.toString();
        gs.lastSync = o["last_sync"].toString();
        m_googleSheets.push_back(gs);
    }
    return migrated;
}

bool SnippetStore::load() {
    ensureDirs();
    QFile file(m_dataFilePath);
    if (!file.exists()) {
        loadFromObject(getDefaultData());
        m_dirty = true;
        m_indexValid = false;
        return flush();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        loadFromObject(getDefaultData());
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        loadFromObject(getDefaultData());
        return false;
    }
    m_indexValid = false;
    if (loadFromObject(doc.object())) { m_dirty = true; flush(); }
    return true;
}

bool SnippetStore::save() {
    m_indexValid = false;
    m_dirty = true;
    m_saveTimer.start();
    return true;
}

bool SnippetStore::flush() {
    m_saveTimer.stop();
    if (!m_dirty) return true;
    m_dirty = false;

    QJsonObject root;

    QJsonObject settingsObj;
    settingsObj["hotkey"] = QString::fromStdString(m_settings.hotkey);
    QJsonArray prefixes;
    for (const auto& p : m_settings.triggerPrefixes)
        prefixes.append(QString::fromStdString(p));
    settingsObj["trigger_prefixes"] = prefixes;
    settingsObj["enable_prefix_trigger"] = m_settings.enablePrefixTrigger;
    settingsObj["auto_delete_prefix"] = m_settings.autoDeletePrefix;
    settingsObj["combo_image_first"] = m_settings.comboImageFirst;
    settingsObj["multiline_enabled"] = m_settings.multilineEnabled;
    settingsObj["max_display_lines"] = m_settings.maxDisplayLines;
    settingsObj["voice_trigger"] = m_settings.voiceTrigger;
    root["settings"] = settingsObj;

    QJsonArray treesArr;
    for (const auto& node : m_roots) treesArr.append(folderToJson(node.get()));
    root["trees"] = treesArr;

    QJsonArray gsArr;
    for (const auto& gs : m_googleSheets) {
        QJsonObject o;
        o["id"] = gs.id;
        o["tree_id"] = gs.treeId;
        o["spreadsheet_id"] = gs.spreadsheetId;
        o["title"] = gs.title;
        o["url"] = gs.url;
        o["selected_sheets"] = QJsonArray::fromStringList(gs.selectedSheets);
        o["all_sheets"] = QJsonArray::fromStringList(gs.allSheets);
        o["last_sync"] = gs.lastSync;
        gsArr.append(o);
    }
    root["google_sheets"] = gsArr;

    ensureDirs();
    QSaveFile file(m_dataFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Snippet store save failed: cannot open %1").arg(m_dataFilePath));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        LOG_ERROR(QString("Snippet store save failed: commit %1").arg(m_dataFilePath));
        return false;
    }
    return true;
}

// ------------------------------------------------------------------
// Default data（对应原 get_default_data）
// ------------------------------------------------------------------
QString SnippetStore::createSampleImage(const QString& filename, const QString& title,
                                        const QString& subtitle, const QColor& bg) const {
    ensureDirs();
    QString rel = "data/images/" + filename;
    QString path = m_imagesDir + "/" + filename;
    if (QFileInfo::exists(path)) return rel;

    const int w = 420, h = 240;
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(bg);
    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(12, 12, w - 24, h - 24);
    p.fillRect(QRect(24, 24, w - 48, h - 48), Qt::white);

    QFont big("Microsoft YaHei");
    big.setPixelSize(24);
    QFont small("Microsoft YaHei");
    small.setPixelSize(15);

    p.setFont(big);
    p.setPen(bg);
    p.drawText(QPoint(40, 60 + 22), title);
    p.setFont(small);
    p.setPen(QColor(80, 80, 80));
    p.drawText(QPoint(40, 110 + 14), subtitle);
    p.setPen(QColor(140, 140, 140));
    p.drawText(QPoint(40, 155 + 14), QString::fromUtf8("★ 快捷输入助手 · 图文示例 ★"));
    p.end();

    img.save(path, "PNG");
    return rel;
}

QJsonObject SnippetStore::getDefaultData() const {
    auto makeItem = [](const QString& kw, const QString& title, const QString& text,
                       const QString& img, const QString& type, int count) {
        QJsonObject o;
        o["id"] = newUuid();
        o["node_type"] = "item";
        o["keyword"] = kw;
        o["title"] = title;
        o["text"] = text;
        o["image_path"] = img;
        o["type"] = type;
        o["use_count"] = count;
        return o;
    };
    auto makeFolder = [](int level, const QString& title, const QJsonArray& children) {
        QJsonObject o;
        o["id"] = newUuid();
        o["node_type"] = "folder";
        o["level"] = level;
        o["title"] = title;
        o["collapsed"] = false;
        o["children"] = children;
        return o;
    };

    QString sampleQr = createSampleImage("sample_card.png", "Official Service Card",
                                         "Scan QR or contact us anytime", QColor(16, 185, 129));
    QString samplePromo = createSampleImage("sample_promo.png", "Special Promotion Poster",
                                            "Exclusive discount for VIP customers", QColor(79, 70, 229));

    QJsonObject root;
    QJsonObject settingsObj;
    settingsObj["hotkey"] = "";
    settingsObj["trigger_prefixes"] = QJsonArray{ "//", ";;" };
    settingsObj["enable_prefix_trigger"] = true;
    settingsObj["auto_delete_prefix"] = true;
    settingsObj["combo_image_first"] = false;
    settingsObj["multiline_enabled"] = true;
    settingsObj["max_display_lines"] = 3;
    root["settings"] = settingsObj;
    root["google_sheets"] = QJsonArray();

    QJsonObject l3a = makeFolder(3, QString::fromUtf8("开场问候 (@@开场问候)"), QJsonArray{
        makeItem("hi", QString::fromUtf8("您好，很高兴为您服务"),
                 QString::fromUtf8("Hello! Glad to help you today 😊 How may I assist you?"), "", "text", 5),
        makeItem("sh", QString::fromUtf8("请稍等，正在为您查询"),
                 QString::fromUtf8("Please wait a moment while I check the details for you ⏳"), "", "text", 3) });
    QJsonObject l3b = makeFolder(3, QString::fromUtf8("名片与活动图文 (@@图文素材)"), QJsonArray{
        makeItem("mp", QString::fromUtf8("发送专属客服名片（图文组合）"),
                 "Here is my official contact card, feel free to reach out anytime:", sampleQr, "combo", 4),
        makeItem("hd", QString::fromUtf8("发送活动福利海报（图文组合）"),
                 QString::fromUtf8("🎉 Our latest promotion is live! Please check the poster below for details~"),
                 samplePromo, "combo", 2) });
    QJsonObject l3c = makeFolder(3, QString::fromUtf8("物流核对 (@@物流核对)"), QJsonArray{
        makeItem("dz", QString::fromUtf8("请确认您的收货地址和电话"),
                 QString::fromUtf8("Could you please confirm your shipping address and phone number so we can arrange dispatch immediately? 📦"),
                 "", "text", 2) });

    QJsonObject l2a = makeFolder(2, "日常接待 (Sheet1)", QJsonArray{ l3a, l3b });
    QJsonObject l2b = makeFolder(2, "订单与售后 (Sheet2)", QJsonArray{ l3c });

    QJsonObject l1 = makeFolder(1, "本地话术库 (示例)", QJsonArray{ l2a, l2b });
    l1["id"] = "local_default_root";
    l1["source_url"] = "";

    root["trees"] = QJsonArray{ l1 };
    return root;
}

// ------------------------------------------------------------------
// 查找
// ------------------------------------------------------------------
bool SnippetStore::findFolder(const QString& id, FolderNode** outNode, FolderNode** outParent,
                              FolderList** outList, int* outIndex) {
    return findFolderRec(id.toStdString(), m_roots, nullptr, outNode, outParent, outList, outIndex);
}

bool SnippetStore::findItem(const QString& id, SnippetItem** outItem, FolderNode** outFolder, int* outIndex) {
    std::string sid = id.toStdString();
    for (auto& r : m_roots) {
        if (findItemRec(sid, *r, outItem, outFolder, outIndex)) return true;
    }
    return false;
}

FolderNode* SnippetStore::findFolderById(const QString& id) {
    FolderNode* n = nullptr;
    return findFolder(id, &n) ? n : nullptr;
}

SnippetItem* SnippetStore::findItemById(const QString& id) {
    SnippetItem* it = nullptr;
    return findItem(id, &it) ? it : nullptr;
}

// ------------------------------------------------------------------
// CRUD
// ------------------------------------------------------------------
FolderNode* SnippetStore::addFolder(const QString& parentId, const QString& title, int level) {
    QString t = title.trimmed();
    if (t.isEmpty()) t = QString::fromUtf8("新建目录");

    auto folder = std::make_unique<FolderNode>();
    folder->id = newUuid().toStdString();
    folder->title = t.toStdString();
    folder->collapsed = false;

    if (parentId.isEmpty()) {
        folder->level = 1;
        m_roots.push_back(std::move(folder));
        save();
        return m_roots.back().get();
    }

    FolderNode* parent = findFolderById(parentId);
    if (!parent) return nullptr;

    folder->level = level > 0 ? level : std::min(3, parent->level + 1);
    parent->children.push_back(std::move(folder));
    parent->collapsed = false;
    save();
    return parent->children.back().get();
}

bool SnippetStore::toggleFolderCollapsed(const QString& folderId) {
    FolderNode* node = findFolderById(folderId);
    if (!node) return false;
    node->collapsed = !node->collapsed;
    save();
    return node->collapsed;
}

bool SnippetStore::setAllCollapsed(bool collapsed, int maxLevel) {
    std::function<void(FolderList&)> walk = [&](FolderList& nodes) {
        for (auto& n : nodes) {
            if (n->level <= maxLevel) n->collapsed = collapsed;
            walk(n->children);
        }
    };
    walk(m_roots);
    return save();
}

SnippetItem* SnippetStore::addItem(const QString& parentFolderId, const SnippetItem& itemData) {
    FolderNode* parent = nullptr;
    if (!parentFolderId.isEmpty()) {
        parent = findFolderById(parentFolderId);
        if (!parent) {
            FolderNode* folder = nullptr;
            if (findItem(parentFolderId, nullptr, &folder)) parent = folder;
        }
    }
    if (!parent) {
        if (m_roots.empty()) addFolder(QString(), QString::fromUtf8("默认主表"));
        parent = m_roots[0].get();
        if (!parent->children.empty()) parent = parent->children[0].get();
    }

    SnippetItem item;
    item.id = newUuid().toStdString();
    item.keyword = QString::fromStdString(itemData.keyword).trimmed().toStdString();
    QString title = QString::fromStdString(itemData.title).trimmed();
    item.title = (title.isEmpty() ? QString::fromUtf8("未命名中文标签") : title).toStdString();
    item.text = QString::fromStdString(itemData.text).trimmed().toStdString();
    item.imagePath = QString::fromStdString(itemData.imagePath).trimmed().toStdString();
    item.useCount = 0;
    item.type = typeFor(item).toStdString();

    parent->items.push_back(item);
    parent->collapsed = false;
    save();
    return &parent->items.back();
}

bool SnippetStore::updateNode(const QString& nodeId, const QString& field, const QString& value) {
    if (FolderNode* f = findFolderById(nodeId)) {
        if (field != "title") return false;
        f->title = value.toStdString();
        return save();
    }

    SnippetItem* it = findItemById(nodeId);
    if (!it) return false;

    std::string v = value.toStdString();
    if (field == "title") it->title = v;
    else if (field == "text") it->text = v;
    else if (field == "keyword") it->keyword = v;
    else if (field == "image_path") it->imagePath = v;
    else return false;

    it->type = typeFor(*it).toStdString();
    return save();
}

int SnippetStore::deleteNodes(const QStringList& nodeIds) {
    QSet<QString> ids(nodeIds.begin(), nodeIds.end());
    if (ids.isEmpty()) return 0;

    int count = pruneRec(m_roots, ids);
    for (int i = m_googleSheets.size() - 1; i >= 0; --i) {
        if (ids.contains(m_googleSheets[i].treeId)) m_googleSheets.removeAt(i);
    }
    if (count > 0) save();
    return count;
}

bool SnippetStore::deleteNode(const QString& nodeId) {
    return deleteNodes({ nodeId }) > 0;
}

bool SnippetStore::moveNode(const QString& nodeId, int direction) {
    FolderList* list = nullptr;
    int idx = -1;
    if (findFolder(nodeId, nullptr, nullptr, &list, &idx)) {
        int n = idx + direction;
        if (n < 0 || n >= static_cast<int>(list->size())) return false;
        std::swap((*list)[idx], (*list)[n]);
        return save();
    }

    FolderNode* folder = nullptr;
    if (findItem(nodeId, nullptr, &folder, &idx)) {
        int n = idx + direction;
        if (n < 0 || n >= static_cast<int>(folder->items.size())) return false;
        std::swap(folder->items[idx], folder->items[n]);
        return save();
    }
    return false;
}

bool SnippetStore::incrementUseCount(const QString& itemId) {
    SnippetItem* it = findItemById(itemId);
    if (!it) return false;
    it->useCount++;
    return save();
}

// ------------------------------------------------------------------
// Google Sheets config
// ------------------------------------------------------------------
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
        for (const auto& s : newSheets) allNames << QString::fromStdString(s->title);
    }
    if (sel.isEmpty()) sel = allNames;

    QString docTitle = customAlias.trimmed();
    if (docTitle.isEmpty() && gsEntry) docTitle = gsEntry->title;
    if (docTitle.isEmpty()) docTitle = fetchedDocTitle;
    if (docTitle.isEmpty()) docTitle = QString("Google表格 (%1)").arg(sheetId.left(6));

    QString targetTreeId = gsEntry ? gsEntry->treeId
                                   : (!existingTreeId.isEmpty() ? existingTreeId : newUuid());

    auto newRoot = std::make_unique<FolderNode>();
    newRoot->id = targetTreeId.toStdString();
    newRoot->level = 1;
    newRoot->title = docTitle.toStdString();
    newRoot->sourceUrl = url.trimmed().toStdString();
    newRoot->spreadsheetId = sheetId.toStdString();
    newRoot->collapsed = false;
    for (auto& s : newSheets) newRoot->children.push_back(std::move(s));

    bool replaced = false;
    for (auto& r : m_roots) {
        bool match = (r->id == newRoot->id) ||
                     (!newRoot->spreadsheetId.empty() && r->spreadsheetId == newRoot->spreadsheetId);
        if (match) {
            newRoot->id = r->id;
            targetTreeId = QString::fromStdString(r->id);
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

QString SnippetStore::importGoogleSheet(GoogleSheetsService* service,
                                        const QString& url,
                                        const QString& customAlias,
                                        const QString& existingTreeId,
                                        const QStringList& selectedNames,
                                        QString* error) {
    auto fail = [&](const QString& msg) {
        if (error) *error = msg;
        return QString();
    };
    if (!service) return fail("内部错误：未提供网络服务");

    QString sheetId = GoogleSheetsService::extractSpreadsheetId(url);
    if (sheetId.isEmpty()) return fail(QString::fromUtf8("无效的 Google 表格链接，请检查是否包含 /spreadsheets/d/..."));

    QStringList sel = selectedNames;
    if (sel.isEmpty()) {
        for (const auto& g : m_googleSheets) {
            if ((!existingTreeId.isEmpty() && g.treeId == existingTreeId) || g.spreadsheetId == sheetId) {
                sel = g.selectedSheets;
                break;
            }
        }
    }

    bool listOk = false;
    QString docTitle, listError;
    QVector<QPair<QString, QString>> sheets;
    service->listSheetNames(url, [&](bool ok, QString title, QString, QVector<QPair<QString, QString>> s, QString err) {
        listOk = ok;
        docTitle = title;
        sheets = s;
        listError = err;
    });
    if (!listOk) return fail(listError);

    QStringList allNames;
    for (const auto& p : sheets) allNames << p.second;
    if (sel.isEmpty()) sel = allNames;

    FolderList fetched;
    for (const auto& p : sheets) {
        if (!sel.contains(p.second)) continue;
        bool ok = false;
        QString err;
        service->fetchSheetItems(sheetId, p.first, p.second, m_imagesDir,
            [&](bool o, FolderList list, QString e) {
                ok = o;
                err = e;
                for (auto& f : list) fetched.push_back(std::move(f));
            });
        if (!ok) return fail(QString::fromUtf8("读取 Sheet「%1」失败：%2").arg(p.second, err));
    }

    return syncGoogleSheet(url, customAlias, existingTreeId, sel, allNames, docTitle, std::move(fetched));
}

int SnippetStore::refreshAllGoogleSheets(GoogleSheetsService* service, QStringList* errors) {
    int okCount = 0;
    const QVector<GoogleSheetConfig> snapshot = m_googleSheets;
    for (const auto& gs : snapshot) {
        QString err;
        QString id = importGoogleSheet(service, gs.url, gs.title, gs.treeId, gs.selectedSheets, &err);
        if (id.isEmpty()) {
            if (errors) *errors << QString("%1: %2").arg(gs.title.isEmpty() ? gs.url : gs.title, err);
        } else {
            ++okCount;
        }
    }
    return okCount;
}

// ------------------------------------------------------------------
// Search / helpers
// ------------------------------------------------------------------
QVector<SnippetStore::SearchHit> SnippetStore::search(const QString& query, bool allWhenEmpty) {
    QVector<SearchHit> results;
    const QString q = query.trimmed().toLower();
    if (q.isEmpty() && !allWhenEmpty) return results;
    for (const IndexEntry& e : index()) {
        if (q.isEmpty() || e.haystack.contains(q)) results.push_back({ e.item, e.trail });
    }
    return results;
}

// 遍历顺序与树一致：先本文件夹的条目，再子文件夹
void SnippetStore::buildIndex(FolderList& list, const QStringList& trail) {
    for (auto& folder : list) {
        QStringList next = trail;
        next << QString::fromStdString(folder->title);
        const QString trailStr = next.join(" / ");
        const QString trailLower = trailStr.toLower();
        for (auto& item : folder->items) {
            IndexEntry e;
            e.item = &item;
            e.keyword = QString::fromStdString(item.keyword).trimmed();
            e.trail = trailStr;
            // 用换行隔开各字段，避免查询词横跨两个字段误命中
            e.haystack = (QString::fromStdString(item.keyword) + '\n' + QString::fromStdString(item.title) + '\n' +
                          QString::fromStdString(item.text) + '\n' + trailLower).toLower();
            m_index.push_back(std::move(e));
        }
        buildIndex(folder->children, next);
    }
}

const QVector<SnippetStore::IndexEntry>& SnippetStore::index() {
    if (!m_indexValid) {
        m_index.clear();
        buildIndex(m_roots, QStringList());
        m_indexValid = true;
    }
    return m_index;
}

int SnippetStore::countItemsInFolder(const FolderNode& folder) const {
    int total = static_cast<int>(folder.items.size());
    for (const auto& child : folder.children)
        total += countItemsInFolder(*child);
    return total;
}

QStringList SnippetStore::getNodePath(const QString& targetId) const {
    QStringList result;
    trailRec(m_roots, targetId.toStdString(), QStringList(), result);
    return result;
}

namespace {

// 英文快捷码要按整词匹配（"hi" 不应命中 "this"）；中日韩文字之间没有空格，仍按子串匹配
bool containsKeyword(const QString& text, const QString& keyword) {
    int from = 0;
    while (true) {
        const int pos = text.indexOf(keyword, from, Qt::CaseInsensitive);
        if (pos < 0) return false;
        auto isWordChar = [](QChar c) { return c.unicode() < 128 && c.isLetterOrNumber(); };
        const bool leftOk = pos == 0 || !isWordChar(keyword.front()) || !isWordChar(text[pos - 1]);
        const int end = pos + keyword.size();
        const bool rightOk = end >= text.size() || !isWordChar(keyword.back()) || !isWordChar(text[end]);
        if (leftOk && rightOk) return true;
        from = pos + 1;
    }
}


} // namespace

QVector<SnippetItem*> SnippetStore::matchKeywords(const QString& spokenText, int limit) {
    QVector<SnippetItem*> hits;
    if (spokenText.trimmed().isEmpty()) return hits;
    for (const IndexEntry& e : index()) {
        if (!e.keyword.isEmpty() && containsKeyword(spokenText, e.keyword)) hits.push_back(e.item);
    }
    std::stable_sort(hits.begin(), hits.end(),
                     [](const SnippetItem* a, const SnippetItem* b) { return a->useCount > b->useCount; });
    if (hits.size() > limit) hits.resize(limit);
    return hits;
}
