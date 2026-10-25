#pragma once
#include <QWidget>

class DictationHistoryModel;
class QLabel;
class QListView;
class QPushButton;

// @brief "声音识别"面板：表头（标题 + 清空）和听写记录列表。
//        每条一行"时间 文字"，悬停时右侧出现"复制"，点它复制整条；双击也会复制。
class DictationLogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DictationLogWidget(QWidget* parent = nullptr);

    void setModel(DictationHistoryModel* model);

signals:
    // 复制成功后的提示由 MainWin 统一弹出
    void copied();

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void copyRow(int row);

    DictationHistoryModel* m_model = nullptr;
    QLabel* m_title = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QListView* m_list = nullptr;
};
