#ifndef SNIPPETSEARCHPOPUP_H
#define SNIPPETSEARCHPOPUP_H

#include <QTimer>
#include <QWidget>

#include "services/ForegroundWindow.h"

class QLabel;
class QLineEdit;
class QListView;
class QStandardItemModel;
class SnippetPaster;
class SnippetStore;

// @brief 全局热键呼出的话术搜索框：搜索框 + 结果列表 + 右侧预览。
//        ↑/↓ 选择，Enter 或单击粘贴，Esc 关闭，失焦自动关闭。
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

    SnippetStore* m_store = nullptr;
    SnippetPaster* m_paster = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QListView* m_listView = nullptr;
    QStandardItemModel* m_model = nullptr;
    QLabel* m_previewBadge = nullptr;
    QLabel* m_previewPath = nullptr;
    QLabel* m_previewTitle = nullptr;
    QLabel* m_previewImage = nullptr;
    QLabel* m_previewText = nullptr;
    QLabel* m_hintLabel = nullptr;

    ForegroundWindow::Handle m_target = 0;   // 呼出前的前台窗口
    QTimer m_hideTimer;
};

#endif // SNIPPETSEARCHPOPUP_H
