#include "ui/controls/SnippetContextMenu.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QImage>
#include <QPixmap>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>

namespace {
const QStringList kImageSuffixes = { "png", "jpg", "jpeg", "bmp", "gif", "webp" };
}

SnippetContextMenu::SnippetContextMenu(SnippetStore* store, SnippetTreeModel* model, QWidget* parent)
    : QMenu(parent), m_store(store), m_model(model), m_dialogParent(parent) {}

SnippetContextMenu::~SnippetContextMenu() = default;

QAction* SnippetContextMenu::add(const QString& text, std::function<void()> fn) {
    QAction* act = addAction(text);
    connect(act, &QAction::triggered, this, [fn]() { fn(); });
    return act;
}

void SnippetContextMenu::finish() {
    m_model->refresh();
    emit changed();
}

void SnippetContextMenu::showAt(const QPoint& globalPos, QTreeView* /*treeView*/, const QModelIndex& clickedIndex) {
    clear();
    m_currentNodeId.clear();
    m_currentLevel = 0;

    if (!clickedIndex.isValid()) {
        buildEmptyMenu();
    } else {
        m_currentNodeId = m_model->nodeIdFromIndex(clickedIndex);
        m_currentLevel = m_model->nodeLevel(clickedIndex);
        if (m_model->isFolder(clickedIndex)) buildFolderMenu();
        else buildItemMenu();
    }
    exec(globalPos);
}

void SnippetContextMenu::buildEmptyMenu() {
    add(tr("New Table"), [this]() { onAddLevel1Folder(); });
    addSeparator();
    add(tr("Select All"), [this]() { emit requestSelectAll(); });
    add(tr("Invert Selection"), [this]() { emit requestInvertSelection(); });
}

void SnippetContextMenu::buildFolderMenu() {
    const QString id = m_currentNodeId;
    FolderNode* folder = m_store->findFolderById(id);
    const QString title = folder ? QString::fromStdString(folder->title) : id;

    if (m_currentLevel == 1) {
        add(tr("Add Sheet Folder (level 2)"), [this, id]() { onAddSubfolder(id, 2); });
        add(tr("Add Snippet"), [this, id]() { onAddItem(id); });
        if (folder && !folder->sourceUrl.empty()) {
            addSeparator();
            add(tr("Choose Sheets to Sync..."), [this, id]() { emit requestGoogleSheetsDialog(id); });
        }
    } else if (m_currentLevel == 2) {
        add(tr("Add Group Folder (level 3, @@)"), [this, id]() { onAddSubfolder(id, 3); });
        add(tr("Add Snippet"), [this, id]() { onAddItem(id); });
    } else {
        add(tr("Add Snippet"), [this, id]() { onAddItem(id); });
    }

    addSeparator();
    add(tr("Rename"), [this, id]() { onEditTitle(id); });

    addSeparator();
    add(tr("Move Up"), [this, id]() { onMoveNode(id, -1); });
    add(tr("Move Down"), [this, id]() { onMoveNode(id, 1); });

    addSeparator();
    add(tr("Delete \"%1\"").arg(title.left(14)), [this, id]() { onDeleteNode(id); });
}

void SnippetContextMenu::buildItemMenu() {
    const QString id = m_currentNodeId;
    SnippetItem* item = m_store->findItemById(id);
    const QString title = item ? QString::fromStdString(item->title) : id;
    const bool hasImage = item && !item->imagePath.empty();

    add(tr("Edit Title"), [this, id]() { onEditTitle(id); });
    add(tr("Edit Text"), [this, id]() { onEditText(id); });
    add(tr("Set Keyword"), [this, id]() { onEditKeyword(id); });

    addSeparator();
    add(tr("Choose Image File..."), [this, id]() { onChooseImage(id); });
    add(tr("Paste Image from Clipboard"), [this, id]() { onPasteClipboardImage(id); });
    if (hasImage) {
        addSeparator();
        add(tr("Remove Image"), [this, id]() { onClearImage(id); });
    }

    addSeparator();
    add(tr("Move Up"), [this, id]() { onMoveNode(id, -1); });
    add(tr("Move Down"), [this, id]() { onMoveNode(id, 1); });

    addSeparator();
    add(tr("Delete \"%1\"").arg(title.left(14)), [this, id]() { onDeleteNode(id); });
}

void SnippetContextMenu::onAddLevel1Folder() {
    bool ok = false;
    const QString name = QInputDialog::getText(m_dialogParent, tr("New Table"), tr("Table name:"),
                                               QLineEdit::Normal, tr("New Table"), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_store->addFolder(QString(), name, 1);
        finish();
    }
}

void SnippetContextMenu::onAddSubfolder(const QString& parentId, int targetLevel) {
    const QString levelName = targetLevel == 2 ? tr("Sheet folder") : tr("Group folder");
    bool ok = false;
    QString name = QInputDialog::getText(m_dialogParent, tr("Add %1").arg(levelName), tr("Name:"),
                                         QLineEdit::Normal, tr("New %1").arg(levelName), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        name = name.remove("@@").trimmed();
        m_store->addFolder(parentId, name, targetLevel);
        finish();
    }
}

void SnippetContextMenu::onAddItem(const QString& parentId) {
    SnippetItem item;
    item.title = tr("New snippet (double-click to edit)").toStdString();
    item.text = tr("Double-click to edit the text").toStdString();
    if (m_store->addItem(parentId, item)) finish();
}

void SnippetContextMenu::onEditTitle(const QString& nodeId) {
    QString cur;
    if (FolderNode* f = m_store->findFolderById(nodeId)) cur = QString::fromStdString(f->title);
    else if (SnippetItem* it = m_store->findItemById(nodeId)) cur = QString::fromStdString(it->title);

    bool ok = false;
    const QString v = QInputDialog::getText(m_dialogParent, tr("Rename"), tr("New name:"), QLineEdit::Normal, cur, &ok);
    if (ok && !v.trimmed().isEmpty()) {
        m_store->updateNode(nodeId, "title", v.trimmed());
        finish();
    }
}

void SnippetContextMenu::onEditText(const QString& nodeId) {
    SnippetItem* it = m_store->findItemById(nodeId);
    if (!it) return;
    bool ok = false;
    const QString v = QInputDialog::getMultiLineText(m_dialogParent, tr("Edit Text"), tr("Text to paste:"),
                                                     QString::fromStdString(it->text), &ok);
    if (ok) {
        m_store->updateNode(nodeId, "text", v);
        finish();
    }
}

void SnippetContextMenu::onEditKeyword(const QString& nodeId) {
    SnippetItem* it = m_store->findItemById(nodeId);
    if (!it) return;
    bool ok = false;
    const QString v = QInputDialog::getText(m_dialogParent, tr("Set Keyword"), tr("Spoken keyword, e.g. quote:"),
                                            QLineEdit::Normal, QString::fromStdString(it->keyword), &ok);
    if (ok) {
        m_store->updateNode(nodeId, "keyword", v.trimmed());
        finish();
    }
}

void SnippetContextMenu::onChooseImage(const QString& itemId) {
    const QString path = QFileDialog::getOpenFileName(m_dialogParent, tr("Choose Image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;All files (*.*)"));
    if (path.isEmpty()) return;
    const QString rel = m_store->copyImageToStore(path);
    if (rel.isEmpty()) {
        emit errorOccurred(tr("复制图片失败"), tr("无法把图片复制到话术库的 images 目录"));
        return;
    }
    m_store->updateNode(itemId, "image_path", rel);
    finish();
}

void SnippetContextMenu::onPasteClipboardImage(const QString& itemId) {
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    QImage img;
    if (mime && mime->hasImage()) {
        img = qvariant_cast<QImage>(mime->imageData());
    } else if (mime && mime->hasUrls()) {
        // 文件管理器里复制的图片文件
        for (const QUrl& u : mime->urls()) {
            if (u.isLocalFile() && kImageSuffixes.contains(QFileInfo(u.toLocalFile()).suffix().toLower())) {
                img.load(u.toLocalFile());
                break;
            }
        }
    }
    if (img.isNull()) {
        emit errorOccurred(tr("剪贴板里没有图片"), tr("请先复制一张图片或截图"));
        return;
    }
    const QString rel = m_store->savePixmapToStore(QPixmap::fromImage(img));
    if (rel.isEmpty()) return;
    m_store->updateNode(itemId, "image_path", rel);
    finish();
}

void SnippetContextMenu::onClearImage(const QString& itemId) {
    m_store->updateNode(itemId, "image_path", QString());
    finish();
}

void SnippetContextMenu::onMoveNode(const QString& nodeId, int direction) {
    if (m_store->moveNode(nodeId, direction)) finish();
}

void SnippetContextMenu::onDeleteNode(const QString& nodeId) {
    if (m_store->deleteNode(nodeId)) finish();
}
