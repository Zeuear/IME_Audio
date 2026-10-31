#ifndef SNIPPETSEARCHPOPUP_H
#define SNIPPETSEARCHPOPUP_H

#include <QHash>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPoint>
#include <QTimer>
#include <QWidget>

#include "services/ForegroundWindow.h"

class QLabel;
class QLineEdit;
class QListView;
class QStandardItemModel;
class SnippetPaster;
class SnippetStore;

// @brief 全局热键呼出的话术搜索框：搜索框 + 结果列表 + 右侧预览（鼠标悬停或方向键切换）。
//        ↑/↓ 选择，Enter 或单击粘贴，拖动结果行可放到其它窗口，Esc 关闭，失焦自动关闭。
//        呼出前先记住用户正在输入的窗口，选定后把焦点还回去再粘贴。
class SnippetSearchPopup : public QWidget {
    Q_OBJECT

public:
    SnippetSearchPopup(SnippetStore* store, SnippetPaster* paster, QWidget *parent = nullptr);

    void showPopup();
    void hidePopup();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum { IdRole = Qt::UserRole + 5, TrailRole = Qt::UserRole + 6 };

    void setupUi();
    void retranslateUi();
    void refreshList();
    void updatePreview(const QModelIndex& index);
    void moveSelection(int delta);
    void triggerPaste(const QModelIndex& index);
    void startDrag(const QModelIndex& index);

    SnippetStore* m_store = nullptr;
    SnippetPaster* m_paster = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QListView* m_listView = nullptr;
    QStandardItemModel* m_model = nullptr;
    QLabel* m_previewPath = nullptr;
    QLabel* m_previewTitle = nullptr;
    QLabel* m_previewImage = nullptr;
    QLabel* m_previewText = nullptr;
    QLabel* m_hintLabel = nullptr;

    ForegroundWindow::Handle m_target = 0;   // 呼出前的前台窗口
    QTimer m_hideTimer;
    QHash<QString, QPixmap> m_previewCache;   // 路径 -> 已缩放的预览图
    QPoint m_dragStartPos;
    QPersistentModelIndex m_dragIndex;        // 按下时的结果行，移动超过阈值才开始拖动
    bool m_dragged = false;                   // 这次按下已发起拖动，松开不能再算点击
    bool m_dragging = false;                  // 拖放循环进行中，失焦不能关闭弹窗
};

#endif // SNIPPETSEARCHPOPUP_H
