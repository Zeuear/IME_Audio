#pragma once

#include "snippets/SnippetTypes.h"
#include "snippets/SnippetStore.h"
#include "snippets/GoogleSheetsService.h"
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

// 对应原 _open_google_sheets_dialog：Google 表格一站式导入与 Sheet 选页中心
// 勾选状态直接读取右侧列表的 check state（按 Sheet 名称保存），网络请求为同步阻塞（期间对话框被禁用）。
class GoogleSheetsDialog : public QDialog {
    Q_OBJECT
public:
    explicit GoogleSheetsDialog(SnippetStore* store, QWidget* parent = nullptr);
    ~GoogleSheetsDialog() override;

    // 在左侧「已保存」列表中选中指定 L1 tree_id
    void selectTree(const QString& treeId);

signals:
    void errorOccurred(const QString& title, const QString& cause = {});

private slots:
    void onFetchSheets();
    void onImportChecked();
    void onSelectSaved(int row);
    void onSetAllChecked(bool checked);
    void onInvertAll();
    void onRemoveSaved();
    void onRefreshSaved();
    void onNewSheet();

private:
    void reloadSavedList();
    void fillSheetList(const QStringList& names, const QStringList& checkedNames, bool allChecked);
    QStringList checkedNames() const;

    SnippetStore* m_store;
    GoogleSheetsService m_service;  
    QLineEdit* m_urlEdit = nullptr;
    QLineEdit* m_aliasEdit = nullptr;
    QListWidget* m_savedList = nullptr;
    QListWidget* m_sheetList = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_importBtn = nullptr;

    QString m_activeTreeId;
};
