#ifndef TERMWIDGET_H
#define TERMWIDGET_H

#include <QHash>
#include <QPersistentModelIndex>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include "services/TermsLibraryManager.h"
#include "services/dictionary/DictionaryProfiles.h"

class QLabel;
class QLineEdit;
class TermListView;
class QListView;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QToolButton;
class QVariantAnimation;
class TermCardDelegate;
class TermListModel;
class DictionaryProfileModel;

// @brief 词典页：左侧是识别 / 翻译两个 tab 下的词典配置列表，右侧是所选配置的卡片列表（单击一行进入多选，双击就地编辑）。
//        顶部平时是搜索 + 添加，多选时换成全选 / 反选 / 启用 / 禁用 / 删除。
class TermWidget : public QWidget {
    Q_OBJECT

signals:
    void errorOccurred(const QString& title, const QString& cause = {});

public:
    explicit TermWidget(QWidget *parent = nullptr);
    ~TermWidget() override = default;

    void setDictionaryProfiles(DictionaryProfiles* profiles);

protected:
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUi();
    QWidget* buildSidePanel();
    QWidget* buildContentPanel();
    void retranslateUi();

    void selectKind(DictionaryKind kind);
    void restoreProfileSelection();
    void showProfile(const QString& id);
    void updateBarTexts();
    void showProfileMenu(const QPoint& pos);
    void handleNewProfile();
    void handleEditProfile(const QString& id);
    void handleDeleteProfile(const QString& id);

    void onClicked(const QModelIndex& proxyIndex);
    void onDoubleClicked(const QModelIndex& proxyIndex);
    void setSelectMode(bool on);
    void updateSelectBar();
    void updateStats();
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

    DictionaryProfiles* m_profiles = nullptr;
    DictionaryKind m_kind = DictionaryKind::Recognition;
    QString m_currentId;
    QString m_shownId;   // 右侧实际显示的词典；切换标签时 m_currentId 会先指向目标，不能拿它判断是否已显示
    QString m_lastId[2];
    QHash<QString, TermListModel*> m_models;

    TermsLibraryManager* m_termsManager = nullptr;
    TermListModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    TermCardDelegate* m_delegate = nullptr;
    TermListView* m_list = nullptr;

    DictionaryProfileModel* m_profileModel = nullptr;
    QListView* m_profileList = nullptr;
    QPushButton* m_recognitionTab = nullptr;
    QPushButton* m_translationTab = nullptr;
    QPushButton* m_newBtn = nullptr;
    QStackedWidget* m_rightStack = nullptr;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_noteLabel = nullptr;

    QStackedWidget* m_barStack = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLineEdit* m_heardEdit = nullptr;
    QLineEdit* m_correctEdit = nullptr;
    QPushButton* m_addBtn = nullptr;
    QToolButton* m_moreBtn = nullptr;
    QPushButton* m_selectAllBtn = nullptr;
    QPushButton* m_reverseBtn = nullptr;
    QLabel* m_countLabel = nullptr;
    QPushButton* m_enableBtn = nullptr;
    QPushButton* m_disableBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_doneBtn = nullptr;
    QLabel* m_statsLabel = nullptr;

    bool m_selectMode = false;
    QVariantAnimation* m_selectAnim = nullptr;

    // 平时单击要等过了双击间隔才进入多选，期间来了双击就只编辑，复选框不会先闪一下
    QTimer* m_clickTimer = nullptr;
    QPersistentModelIndex m_pendingClick;
    void toggleChecked(const QModelIndex& proxyIndex);
};

#endif // TERMWIDGET_H
