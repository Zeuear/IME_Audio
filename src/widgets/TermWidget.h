#ifndef TERMWIDGET_H
#define TERMWIDGET_H

#include <QPersistentModelIndex>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include "../TermsLibraryManager.h"

class QLabel;
class QLineEdit;
class TermListView;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QToolButton;
class QVariantAnimation;
class TermCardDelegate;
class TermListModel;

// @brief 词典页：卡片列表（听成 → 写成），单击一行进入多选，双击就地编辑。
//        顶部平时是搜索 + 添加，多选时换成全选 / 反选 / 启用 / 禁用 / 删除。
class TermWidget : public QWidget {
    Q_OBJECT

signals:
    void errorOccurred(const QString& title, const QString& cause = {});

public:
    explicit TermWidget(QWidget *parent = nullptr);
    ~TermWidget() override = default;

    void setTermsManager(TermsLibraryManager* termsManager);

protected:
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUi();
    void retranslateUi();

    void onClicked(const QModelIndex& proxyIndex);
    void onDoubleClicked(const QModelIndex& proxyIndex);
    void setSelectMode(bool on);
    void updateSelectBar();
    QList<int> visibleSourceRows() const;

    void handleAdd();
    void handleSelectAll();
    void handleReverseSelect();
    void handleSetEnabled(bool enabled);
    void handleDelete();
    void handleReload();
    void handleCheckConflicts();
    void handleImport();
    void handleExport();

    TermsLibraryManager* m_termsManager = nullptr;
    TermListModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    TermCardDelegate* m_delegate = nullptr;
    TermListView* m_list = nullptr;

    QStackedWidget* m_barStack = nullptr;
    // 普通工具栏
    QLineEdit* m_searchEdit = nullptr;
    QLineEdit* m_heardEdit = nullptr;
    QLineEdit* m_correctEdit = nullptr;
    QPushButton* m_addBtn = nullptr;
    QToolButton* m_moreBtn = nullptr;
    // 多选工具栏
    QPushButton* m_selectAllBtn = nullptr;
    QPushButton* m_reverseBtn = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_enableBtn = nullptr;
    QPushButton* m_disableBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_doneBtn = nullptr;

    bool m_selectMode = false;
    QVariantAnimation* m_selectAnim = nullptr;

    // 平时单击要等过了双击间隔才进入多选，期间来了双击就只编辑，复选框不会先闪一下
    QTimer* m_clickTimer = nullptr;
    QPersistentModelIndex m_pendingClick;
    void toggleChecked(const QModelIndex& proxyIndex);
};

#endif // TERMWIDGET_H
