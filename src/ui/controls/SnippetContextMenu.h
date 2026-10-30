#pragma once

#include "snippets/SnippetTypes.h"
#include "snippets/SnippetStore.h"
#include "ui/models/SnippetTreeModel.h"
#include <QMenu>
#include <QTreeView>
#include <functional>

// 对应原 ModernContextMenu。clickedIndex 必须是 SnippetTreeModel（源模型）的索引。
// 所有改动完成后会调用 model->refresh() 并发出 changed()，由窗口恢复展开状态。
class SnippetContextMenu : public QMenu {
    Q_OBJECT
public:
    explicit SnippetContextMenu(SnippetStore* store, SnippetTreeModel* model, QWidget* parent = nullptr);
    ~SnippetContextMenu() override;

    void showAt(const QPoint& globalPos, QTreeView* treeView, const QModelIndex& clickedIndex = QModelIndex());

signals:
    void errorOccurred(const QString& title, const QString& cause = {});
    void changed();
    void requestGoogleSheetsDialog(const QString& treeId);
    void requestSelectAll();
    void requestInvertSelection();

private:
    void buildEmptyMenu();
    void buildFolderMenu();
    void buildItemMenu();
    QAction* add(const QString& text, std::function<void()> fn);

    void onAddLevel1Folder();
    void onAddSubfolder(const QString& parentId, int targetLevel);
    void onAddItem(const QString& parentId);
    void onEditTitle(const QString& nodeId);
    void onEditText(const QString& nodeId);
    void onEditKeyword(const QString& nodeId);
    void onChooseImage(const QString& itemId);
    void onPasteClipboardImage(const QString& itemId);
    void onClearImage(const QString& itemId);
    void onMoveNode(const QString& nodeId, int direction);
    void onDeleteNode(const QString& nodeId);

    // 结构变化（增删移动）整体 refresh；传入 editedNodeId 表示只改了该节点内容，走增量更新。最后都发 changed
    void finish(const QString& editedNodeId = QString());

    SnippetStore* m_store;
    SnippetTreeModel* m_model;
    QWidget* m_dialogParent = nullptr;
    QString m_currentNodeId;
    int m_currentLevel = 0;
};
