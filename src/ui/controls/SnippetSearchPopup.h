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

// @brief 全局热键呼出的话术搜索框
class SnippetSearchPopup : public QWidget {
    Q_OBJECT

signals:
    void closed();

public:
    SnippetSearchPopup(SnippetStore* store, SnippetPaster* paster, QWidget *parent = nullptr);

    void showPopup();
    // 打字前缀呼出
    void showPopupAfterPrefix(int prefixLength);
    void hidePopup();

    void setQuery(const QString& query);
    void navigate(int key);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum { IdRole = Qt::UserRole + 5, TrailRole = Qt::UserRole + 6 };  

    void open(bool followTyping);
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

    ForegroundWindow::Handle m_target = 0;    // 呼出前的前台窗口
    QTimer m_hideTimer;
    QHash<QString, QPixmap> m_previewCache;   // 路径 -> 已缩放的预览图
    QPoint m_dragStartPos;
    QPersistentModelIndex m_dragIndex;        // 按下时的结果行，移动超过阈值才开始拖动
    bool m_dragged = false;                   // 这次按下已发起拖动，松开不能再算点击
    bool m_dragging = false;                  // 拖放循环进行中，失焦不能关闭弹窗
    int m_erasePrefixChars = 0;               // 选定条目后要先删掉的前缀字符数
    int m_queryLength = 0;                    // 打字前缀模式下已经打进目标窗口的搜索词长度
    bool m_queryMode = false;                 // 打字前缀呼出：弹窗不拿焦点，搜索词由 PrefixWatcher 转发
    QTimer m_idleTimer;
    QTimer m_outsideClickTimer;               // 打字前缀模式下轮询全局鼠标，点到弹窗外面就收起
    bool m_mouseWasDown = false;
};

#endif 
