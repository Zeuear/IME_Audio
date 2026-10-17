#pragma once
#include <QComboBox>
#include <QElapsedTimer>

// @brief
// 条目很多时可输入过滤的下拉框：单击弹出列表选择，双击进入输入模式，按名称任意位置过滤。
// 平时输入框只读，避免单击只出现光标却不弹出列表。
class SearchableComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit SearchableComboBox(QWidget* parent = nullptr);

    void showPopup() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupUi();
    void enterEditMode();
    void leaveEditMode();
    bool isEditing() const;

    QElapsedTimer m_popupClock;          // 列表弹出后计时，用来识别落在列表外的"第二次点击"
    bool m_swallowEditClick = false;     // 双击进入输入后，吞掉第二下剩余的双击/松开事件
    bool m_openingPress = false;         // 单击输入框弹出列表的那次按下，其松开不能让列表收起
};
