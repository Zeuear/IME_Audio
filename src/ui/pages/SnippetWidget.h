#ifndef SNIPPETWIDGET_H
#define SNIPPETWIDGET_H

#include <QTimer>
#include <QWidget>

#include "ui/delegates/SnippetTreeDelegate.h"

class GoogleSheetsService;
class QAction;
class AnimatedCheckBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;
class ShortcutEdit;
class QTreeView;
class SnippetContextMenu;
class SnippetStore;
class SnippetTreeModel;

// @brief 指令设置页：多级话术树（主表 → Sheet → 分组 → 条目），说到条目的快捷码时悬浮窗会提示点击粘贴。
//        支持双击编辑、多选批量删除、右键菜单、Google 表格导入。
class SnippetWidget : public QWidget {
    Q_OBJECT

signals:
    void errorOccurred(const QString& title, const QString& cause = {});
    // 话术库内容或设置有变化（悬浮窗据此刷新）
    void storeChanged();
    // 全局热键被按下，请求呼出话术搜索框
    void searchPopupRequested();

public:
    explicit SnippetWidget(QWidget *parent = nullptr);
    ~SnippetWidget() override = default;

    void setSnippetStore(SnippetStore* store);

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateIcons();   // 图标跟随当前主题文字色重新着色
    void onResizeTreeLayout();
    SnippetTreeDelegate::Zone zoneUnderCursor(const QModelIndex& proxyIndex) const;

    void refreshTree();
    void applyExpandState();
    void updateStatus();
    QModelIndex toSource(const QModelIndex& proxyIndex) const;

    void onAddLevel1Folder();
    void onRefreshAllSheets();
    void onOpenGoogleSheets(const QString& treeId = QString());
    void onExpandCollapseAll(bool expand);
    void onBatchDeleteSelected();
    void onSelectAll();
    void onInvertSelection();
    void onContextMenu(const QPoint& pos);
    void onItemDoubleClicked(const QModelIndex& proxyIndex);
    void onTreeClicked(const QModelIndex& proxyIndex);
    void onSearchTextChanged(const QString& text);
    void onTreeExpandedChanged(const QModelIndex& proxyIndex, bool expanded);

    SnippetStore* m_store = nullptr;
    SnippetTreeModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    SnippetContextMenu* m_contextMenu = nullptr;
    SnippetTreeDelegate* m_delegate = nullptr;
    bool m_applyingExpand = false;
    QTimer m_relayoutTimer;   // 缩放窗口时合并行高重算
    GoogleSheetsService* m_sheetsService = nullptr; 
    bool m_refreshing = false;

    QTreeView* m_treeView = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_linesLabel = nullptr;
    AnimatedCheckBox* m_voiceTriggerChk = nullptr;
    AnimatedCheckBox* m_imageFirstChk = nullptr;
    QSpinBox* m_maxLinesSpin = nullptr;
    QLabel* m_hotkeyLabel = nullptr;
    ShortcutEdit* m_hotkeyEdit = nullptr;
    QPushButton* m_googleBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_expandBtn = nullptr;
    QPushButton* m_collapseBtn = nullptr;
    QPushButton* m_optionsBtn = nullptr;
    QPushButton* m_moreBtn = nullptr;
    QPushButton* m_multilineBtn = nullptr;
    QMenu* m_optionsMenu = nullptr;
    QMenu* m_moreMenu = nullptr;
    QAction* m_searchAction = nullptr;
    QAction* m_selectAllAct = nullptr;
    QAction* m_invertAct = nullptr;
    QPushButton* m_deleteBtn = nullptr;
};

#endif // SNIPPETWIDGET_H
