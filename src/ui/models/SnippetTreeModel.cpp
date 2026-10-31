#include "ui/models/SnippetTreeModel.h"
#include <QBrush>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QImageReader>
#include <QFont>
#include <QGuiApplication>
#include <QImage>

SnippetTreeModel::SnippetTreeModel(SnippetStore* store, QObject* parent)
    : QAbstractItemModel(parent), m_store(store) {
    rebuild();
}

SnippetTreeModel::~SnippetTreeModel() = default;

void SnippetTreeModel::rebuild() {
    m_info.clear();
    m_idToPtr.clear();
    if (m_thumbCache.size() > 500) m_thumbCache.clear();   
    if (!m_store) return;
    const FolderList& roots = m_store->roots();
    for (int i = 0; i < static_cast<int>(roots.size()); ++i) {
        const FolderNode* f = roots[i].get();
        m_info[f] = { true, nullptr, i };
        m_idToPtr[f->id] = f;
        m_info[f].count = rebuildFolder(f);
        m_info[f].search = f->title.toLower();
    }
}

// 返回该文件夹下（含子孙）的话术总数
int SnippetTreeModel::rebuildFolder(const FolderNode* f) {
    int row = 0;
    int total = static_cast<int>(f->items.size());
    for (const auto& c : f->children) {
        m_info[c.get()] = { true, f, row++ };
        m_idToPtr[c->id] = c.get();
        const int n = rebuildFolder(c.get());
        Info& ci = m_info[c.get()];
        ci.count = n;
        ci.search = c->title.toLower();
        total += n;
    }
    for (const auto& it : f->items) {
        m_info[&it] = { false, f, row++ };
        m_idToPtr[it.id] = &it;
        m_info[&it].search = (it.title + ' ' + it.text + ' ' +
                              it.keyword).toLower();
    }
    return total;
}

const SnippetTreeModel::Info* SnippetTreeModel::infoOf(const void* ptr) const {
    auto it = m_info.find(ptr);
    return it == m_info.end() ? nullptr : &it->second;
}

QModelIndex SnippetTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!m_store || row < 0 || column < 0 || column >= columnCount()) return QModelIndex();

    if (!parent.isValid()) {
        const FolderList& roots = m_store->roots();
        if (row < static_cast<int>(roots.size()))
            return createIndex(row, column, roots[row].get());
        return QModelIndex();
    }
    if (parent.column() != 0) return QModelIndex();

    const Info* pi = infoOf(parent.internalPointer());
    if (!pi || !pi->isFolder) return QModelIndex();
    const FolderNode* folder = static_cast<const FolderNode*>(parent.internalPointer());

    int childFolders = static_cast<int>(folder->children.size());
    if (row < childFolders)
        return createIndex(row, column, folder->children[row].get());
    int itemIdx = row - childFolders;
    if (itemIdx < static_cast<int>(folder->items.size()))
        return createIndex(row, column, const_cast<SnippetItem*>(&folder->items[itemIdx]));
    return QModelIndex();
}

QModelIndex SnippetTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid()) return QModelIndex();
    const Info* info = infoOf(index.internalPointer());
    if (!info || !info->parent) return QModelIndex();
    const Info* pinfo = infoOf(info->parent);
    if (!pinfo) return QModelIndex();
    return createIndex(pinfo->row, 0, const_cast<FolderNode*>(info->parent));
}

int SnippetTreeModel::rowCount(const QModelIndex& parent) const {
    if (!m_store) return 0;
    if (!parent.isValid()) return static_cast<int>(m_store->roots().size());
    if (parent.column() != 0) return 0;
    const Info* pi = infoOf(parent.internalPointer());
    if (!pi || !pi->isFolder) return 0;
    const FolderNode* node = static_cast<const FolderNode*>(parent.internalPointer());
    return static_cast<int>(node->children.size() + node->items.size());
}

int SnippetTreeModel::columnCount(const QModelIndex& /*parent*/) const {
    return 3; // 0: 名称/中文, 1: 外文, 2: 快捷码
}

namespace {
constexpr int kThumbW = 56;  
constexpr int kThumbH = 34;
}

// 条目配图在磁盘上的位置；远程图只认已下载的缓存（列表里不发网络请求）。无配图返回空串。
QString SnippetTreeModel::imageFileFor(const SnippetItem& item) const {
    const QString imagePath = item.imagePath.trimmed();
    if (imagePath.isEmpty()) return QString();
    const QString file = m_store->resolveImagePath(imagePath);
    if (file.startsWith("http://") || file.startsWith("https://")) return m_store->remoteImageCachePath(file);
    return file;
}

QPixmap SnippetTreeModel::thumbnailFor(const SnippetItem& item) const {
    const QString file = imageFileFor(item);
    if (file.isEmpty()) return QPixmap();

    // 带 mtime：图片被替换后自动失效
    const QString key = file + '|' + QString::number(QFileInfo(file).lastModified().toMSecsSinceEpoch());
    auto cached = m_thumbCache.constFind(key);
    if (cached != m_thumbCache.constEnd()) return cached.value();

    QPixmap thumb;
    // 读取时直接按目标尺寸解码，不把大图整张载入内存再缩小
    const qreal dpr = qApp->devicePixelRatio();
    const QSize target = QSize(kThumbW, kThumbH) * dpr;
    QImageReader reader(file);
    reader.setAutoTransform(true);
    const QSize src = reader.size();
    if (src.isValid()) reader.setScaledSize(src.scaled(target, Qt::KeepAspectRatio));
    const QImage image = reader.read();
    if (!image.isNull()) {
        thumb = QPixmap::fromImage(image);
        thumb.setDevicePixelRatio(dpr);
    }
    m_thumbCache.insert(key, thumb);
    return thumb;
}

QVariant SnippetTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_store) return QVariant();
    const void* ptr = index.internalPointer();
    const Info* info = infoOf(ptr);
    if (!info) return QVariant();

    const bool isFol = info->isFolder;
    const FolderNode* folder = isFol ? static_cast<const FolderNode*>(ptr) : nullptr;
    const SnippetItem* item = isFol ? nullptr : static_cast<const SnippetItem*>(ptr);
    const int level = isFol ? folder->level : 4;
    const int col = index.column();

    switch (role) {
    case Qt::DisplayRole:
        if (col == 0) return isFol ? folder->title : item->title;
        if (col == 1) {
            if (isFol) return QString::number(info->count);   // 只放数字，表头已去掉，不再写“条话术”
            return item->text.simplified().left(60);
        }
        if (col == 2 && !isFol) return item->keyword;
        return QVariant();
    case Qt::DecorationRole:
        if (!isFol && col == 0) {
            const QPixmap thumb = thumbnailFor(*item);
            if (!thumb.isNull()) return thumb;
        }
        return QVariant();
    case Qt::ToolTipRole: {
        if (isFol) return QVariant();
        // 悬停预览：快捷码 + 标题 + 配图大图 + 完整内容
        auto html = [](const QString& s) { return s.toHtmlEscaped().replace('\n', "<br>"); };
        QString tip;
        if (!item->keyword.isEmpty()) tip += "<b>[" + html(item->keyword) + "]</b> ";
        tip += "<b>" + html(item->title) + "</b>";
        const QString file = imageFileFor(*item);
        if (!file.isEmpty() && QFileInfo::exists(file)) tip += "<br><img src=\"" + file.toHtmlEscaped() + "\" width=\"260\">";
        if (!item->filePath.isEmpty()) tip += "<br>" + html(item->filePath);
        tip += "<br>" + html(item->text);
        return tip;
    }
    case TextRole: {
        if (isFol) return QVariant();
        if (item->filePath.isEmpty()) return item->text;
        // 委托只画这段文字。附件名放第一行：单行显示或内容很长时，后面的行会被截掉
        const QString attachment = QCoreApplication::translate("SnippetTreeModel", "File: %1")
                                       .arg(QFileInfo(item->filePath).fileName());
        return item->text.isEmpty() ? attachment : attachment + '\n' + item->text;
    }
    case KeywordRole:
        return isFol ? QVariant() : QVariant(item->keyword);
    case CountRole:
        return isFol ? QVariant(info->count) : QVariant();
    case HasImageRole:
        return !isFol && !item->imagePath.isEmpty();
    case NodeIdRole:
        return isFol ? folder->id : item->id;
    case IsFolderRole:
        return isFol;
    case LevelRole:
        return level;
    case SearchRole:
        return info->search;
    case Qt::ForegroundRole:
        if (isFol) return QBrush(levelColor(level));
        return QVariant();
    case Qt::BackgroundRole:
        if (isFol) return QBrush(levelBg(level));
        return QVariant();
    case Qt::FontRole:
        if (isFol) {
            QFont f;
            f.setBold(true);
            return f;
        }
        return QVariant();
    default:
        return QVariant();
    }
}

QVariant SnippetTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
    switch (section) {
    case 0: return tr("Title");
    case 1: return tr("Text");
    case 2: return tr("Keyword");
    default: return QVariant();
    }
}

Qt::ItemFlags SnippetTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void SnippetTreeModel::refresh() {
    beginResetModel();
    rebuild();
    endResetModel();
}

QModelIndex SnippetTreeModel::indexByNodeId(const QString& nodeId) const {
    auto it = m_idToPtr.find(nodeId);
    if (it == m_idToPtr.end()) return QModelIndex();
    const Info* info = infoOf(it.value());
    if (!info) return QModelIndex();
    return createIndex(info->row, 0, const_cast<void*>(it.value()));
}

QString SnippetTreeModel::nodeIdFromIndex(const QModelIndex& index) const {
    if (!index.isValid()) return QString();
    return data(index, NodeIdRole).toString();
}

bool SnippetTreeModel::isFolder(const QModelIndex& index) const {
    if (!index.isValid()) return false;
    return data(index, IsFolderRole).toBool();
}

int SnippetTreeModel::nodeLevel(const QModelIndex& index) const {
    if (!index.isValid()) return 0;
    return data(index, LevelRole).toInt();
}

QString SnippetTreeModel::nodeTitle(const QModelIndex& index) const {
    if (!index.isValid()) return QString();
    return data(index.siblingAtColumn(0), Qt::DisplayRole).toString();
}

QColor SnippetTreeModel::levelColor(int level) {
    switch (level) {
        case 1: return QColor("#2563EB");
        case 2: return QColor("#059669");
        case 3: return QColor("#D97706");
        default: return QColor("#475569");
    }
}

namespace {
// 半透明底色在 light / gray / dark 主题下都可读
QColor withAlpha(QColor c) { c.setAlpha(38); return c; }
}

QColor SnippetTreeModel::levelBg(int level) {
    switch (level) {
        case 1: return withAlpha(levelColor(1));
        case 2: return withAlpha(levelColor(2));
        case 3: return withAlpha(levelColor(3));
        default: return withAlpha(levelColor(0));
    }
}

QString SnippetTreeModel::levelBadge(int level) {
    switch (level) {
        case 1: return QCoreApplication::translate("SnippetTreeModel", "Level 1");
        case 2: return QCoreApplication::translate("SnippetTreeModel", "Level 2");
        case 3: return QCoreApplication::translate("SnippetTreeModel", "Level 3");
        default: return QCoreApplication::translate("SnippetTreeModel", "Folder");
    }
}

void SnippetTreeModel::nodeChanged(const QString& nodeId) {
    auto it = m_idToPtr.find(nodeId);
    auto info = it == m_idToPtr.end() ? m_info.end() : m_info.find(it.value());
    if (info == m_info.end()) {
        refresh();   // 不认识的节点，退回整体重建
        return;
    }
    const void* ptr = it.value();
    if (info->second.isFolder) {
        info->second.search = static_cast<const FolderNode*>(ptr)->title.toLower();
    } else {
        const auto* item = static_cast<const SnippetItem*>(ptr);
        info->second.search = (item->title + ' ' + item->text + ' ' + item->keyword).toLower();
    }
    const QModelIndex first = createIndex(info->second.row, 0, const_cast<void*>(ptr));
    emit dataChanged(first, first.siblingAtColumn(columnCount() - 1));
}
