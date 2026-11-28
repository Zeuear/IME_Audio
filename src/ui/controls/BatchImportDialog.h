#pragma once
#include <QDialog>
#include <QList>

#include "services/dictionary/GlossaryImport.h"

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;
class QTimer;

// @brief 批量导入：粘贴或打开文件，一行一条，自动识别分隔符（=>、->、=、制表符、逗号等）和 JSON；
//        下方实时显示识别了几条、哪几行读不懂。真正写入词典由调用方做。
class BatchImportDialog : public QDialog {
    Q_OBJECT
public:
    // translation：翻译词典（每条都要有源词）；langA / langB 用来判断两栏是否写反
    BatchImportDialog(bool translation, const QString& langA, const QString& langB, QWidget* parent = nullptr);

    void setText(const QString& text);
    QList<GlossaryImport::Entry> entries() const { return m_parsed.entries; }
    bool swapColumns() const;

private:
    void reparse();
    void openFile();

    bool m_translation;
    QString m_langA;
    QString m_langB;
    GlossaryImport::Parsed m_parsed;
    bool m_swapTouched = false;

    QPlainTextEdit* m_edit = nullptr;
    QPlainTextEdit* m_issues = nullptr;
    QLabel* m_summary = nullptr;
    QCheckBox* m_swap = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QTimer* m_timer = nullptr;
};
