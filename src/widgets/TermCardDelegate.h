#pragma once
#include <QListView>
#include <QStyledItemDelegate>
#include <QWidget>

class QLineEdit;

// @brief 词典卡片列表。卡片颜色由 qss 的 qproperty-* 设置：主题只换 qss 不换 palette，
//        取 palette 的颜色会和输入框撞色。
class TermListView : public QListView {
    Q_OBJECT
    Q_PROPERTY(QColor cardColor MEMBER m_cardColor)
    Q_PROPERTY(QColor cardBorderColor MEMBER m_cardBorderColor)
    Q_PROPERTY(QColor disabledCardColor MEMBER m_disabledCardColor)
    Q_PROPERTY(QColor textColor MEMBER m_textColor)
    Q_PROPERTY(QColor hintColor MEMBER m_hintColor)
public:
    using QListView::QListView;

    QColor m_cardColor{ 66, 66, 66 };
    QColor m_cardBorderColor{ 82, 82, 82 };
    QColor m_disabledCardColor{ 45, 45, 45 };
    QColor m_textColor{ 220, 220, 220 };
    QColor m_hintColor{ 140, 140, 140 };
};

// @brief 双击卡片时的就地编辑器：听成 → 写成 两个输入框。
//        回车 / Esc 由 QStyledItemDelegate 处理（输入框不吃这两个键，会传到这里）；
//        焦点离开整个编辑器时自动提交，在两个输入框之间切换不算离开。
class TermEditor : public QWidget {
    Q_OBJECT
public:
    explicit TermEditor(QWidget* parent = nullptr);

    void setValues(const QString& heard, const QString& correct);
    QString heard() const;
    QString correct() const;

signals:
    void focusLeft();

private:
    QLineEdit* m_heardEdit;
    QLineEdit* m_correctEdit;
};

// @brief 词典卡片的绘制：左侧蓝条 = 启用、灰底 = 禁用；多选模式下右侧滑入复选框。
//        颜色来自 TermListView 的 qss 属性，只有启用的蓝色是固定语义色。
class TermCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // 0 = 普通模式，1 = 多选模式，中间值用于复选框滑入动画
    void setSelectProgress(qreal t) { m_selectT = t; }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& index) const override;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& opt, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& opt, const QModelIndex& index) const override;

private:
    qreal m_selectT = 0;
};
