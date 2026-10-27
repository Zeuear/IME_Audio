#include "TermCardDelegate.h"
#include "TermListModel.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace {
constexpr int kCardHeight = 44;
constexpr int kGap = 4;           // 卡片上下间距
constexpr int kRadius = 8;
constexpr int kPad = 14;
constexpr int kBarWidth = 4;
constexpr int kCheckBox = 18;
constexpr int kCheckArea = 40;    // 多选模式下右侧给复选框留出的宽度
constexpr int kArrowW = 64;       // 中间箭头区：两侧各留约 20px，左右两列不会贴着箭头
constexpr int kArrowLen = 18;
const QColor kEnabledColor(0x3b, 0x82, 0xf6);

QRect cardRect(const QStyleOptionViewItem& opt)
{
    return opt.rect.adjusted(0, kGap, 0, -kGap);
}
} // namespace

TermEditor::TermEditor(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    auto* layout = new QHBoxLayout(this);
    // 和卡片绘制用同一套列宽，双击进入编辑时文字位置不跳
    layout->setContentsMargins(kBarWidth + kPad - 6, 4, kPad, 4);
    layout->setSpacing(0);
    m_heardEdit = new QLineEdit(this);
    m_heardEdit->setPlaceholderText(tr("Heard as (comma separated, optional)"));
    m_correctEdit = new QLineEdit(this);
    m_correctEdit->setPlaceholderText(tr("Write as"));
    auto* arrow = new QLabel(QStringLiteral("→"), this);
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setFixedWidth(kArrowW);
    layout->addWidget(m_heardEdit, 1);
    layout->addWidget(arrow);
    layout->addWidget(m_correctEdit, 1);
    setFocusProxy(m_heardEdit);

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
        if (!now || !isAncestorOf(now)) emit focusLeft();
    });
}

void TermEditor::setValues(const QString& heard, const QString& correct)
{
    m_heardEdit->setText(heard);
    m_correctEdit->setText(correct);
}

QString TermEditor::heard() const { return m_heardEdit->text(); }
QString TermEditor::correct() const { return m_correctEdit->text(); }

QSize TermCardDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const
{
    return QSize(opt.rect.width(), kCardHeight + 2 * kGap);
}

void TermCardDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const
{
    const auto* view = qobject_cast<const TermListView*>(opt.widget);
    if (!view) {
        QStyledItemDelegate::paint(p, opt, index);
        return;
    }
    const TermListView& colors = *view;
    const bool enabled = index.data(TermListModel::EnabledRole).toBool();
    const bool checked = index.data(TermListModel::CheckedRole).toBool();
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const QStringList heard = index.data(TermListModel::HeardRole).toStringList();
    const QString correct = index.data(TermListModel::CorrectRole).toString();

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const QRect card = cardRect(opt);
    QPainterPath path;
    path.addRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);

    QColor bg = enabled ? colors.m_cardColor : colors.m_disabledCardColor;
    // 悬停时往文字色方向靠一点，深浅主题都适用
    if (hovered) {
        const QColor& t = colors.m_textColor;
        bg = QColor::fromRgbF(bg.redF() * 0.92f + t.redF() * 0.08f, bg.greenF() * 0.92f + t.greenF() * 0.08f,
                              bg.blueF() * 0.92f + t.blueF() * 0.08f);
    }
    p->fillPath(path, bg);
    p->setPen(QPen(checked ? kEnabledColor : colors.m_cardBorderColor, checked ? 1.5 : 1.0));
    p->drawPath(path);

    // 左侧蓝条：裁在卡片圆角内
    if (enabled) {
        p->save();
        p->setClipPath(path);
        p->fillRect(QRect(card.left(), card.top(), kBarWidth, card.height()), kEnabledColor);
        p->restore();
    }

    // 右侧复选框随多选模式滑入
    const int checkW = int(kCheckArea * m_selectT);
    if (m_selectT > 0.01) {
        const QRectF box(card.right() - checkW + (kCheckArea - kCheckBox) / 2.0,
                         card.center().y() - kCheckBox / 2.0 + 0.5, kCheckBox, kCheckBox);
        p->setOpacity(m_selectT);
        p->setPen(QPen(checked ? kEnabledColor : colors.m_hintColor, 1.5));
        p->setBrush(checked ? kEnabledColor : Qt::transparent);
        p->drawRoundedRect(box, 4, 4);
        if (checked) {
            QPainterPath tick;
            tick.moveTo(box.left() + 4.5, box.center().y());
            tick.lineTo(box.left() + 7.5, box.bottom() - 5);
            tick.lineTo(box.right() - 4, box.top() + 5);
            p->setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(Qt::NoBrush);
            p->drawPath(tick);
        }
        p->setOpacity(1.0);
    }

    // 文字：听成 → 写成；听成为空时显示"自动识别"
    const QColor textColor = enabled ? colors.m_textColor : colors.m_hintColor;
    const QColor hintColor = colors.m_hintColor;
    const int left = card.left() + kBarWidth + kPad;
    const int right = card.right() - kPad - checkW;
    // 左右两列等宽，箭头居中占一段固定宽度，两列文字都左对齐，扫读时能对齐成两栏
    const int colW = std::max(0, (right - left - kArrowW) / 2);
    const QRect heardRect(left, card.top(), colW, card.height());
    const QRect arrowRect(heardRect.right() + 1, card.top(), kArrowW, card.height());
    const QRect correctRect(arrowRect.right() + 1, card.top(), std::max(0, right - arrowRect.right()), card.height());

    QFont font = opt.font;
    p->setFont(font);
    const QFontMetrics fm(font);
    if (heard.isEmpty()) {
        QFont hint = font;
        hint.setItalic(true);
        p->setFont(hint);
        p->setPen(hintColor);
        p->drawText(heardRect, Qt::AlignLeft | Qt::AlignVCenter, tr("Auto recognize"));
        p->setFont(font);
    } else {
        p->setPen(textColor);
        p->drawText(heardRect, Qt::AlignLeft | Qt::AlignVCenter,
                    fm.elidedText(heard.join(QStringLiteral(", ")), Qt::ElideRight, heardRect.width()));
    }
    // 箭头自己画：字体里的 "→" 各平台粗细、基线不一，画出来更细更稳
    {
        const QPointF c = QRectF(arrowRect).center();
        const qreal x0 = c.x() - kArrowLen / 2.0, x1 = c.x() + kArrowLen / 2.0;
        p->setPen(QPen(hintColor, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(x0, c.y()), QPointF(x1, c.y()));
        QPainterPath head;
        head.moveTo(x1 - 5, c.y() - 4);
        head.lineTo(x1, c.y());
        head.lineTo(x1 - 5, c.y() + 4);
        p->drawPath(head);
    }

    QFont bold = font;
    bold.setBold(true);
    p->setFont(bold);
    p->setPen(textColor);
    QString rightText = correct;
    if (!enabled) rightText += QStringLiteral("  ") + tr("(disabled)");
    p->drawText(correctRect, Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(bold).elidedText(rightText, Qt::ElideRight, correctRect.width()));

    p->restore();
}

QWidget* TermCardDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const
{
    auto* editor = new TermEditor(parent);
    connect(editor, &TermEditor::focusLeft, this, [this, editor]() {
        // focusChanged 可能连发几次，只提交一次
        if (editor->property("closing").toBool()) return;
        editor->setProperty("closing", true);
        emit const_cast<TermCardDelegate*>(this)->commitData(editor);
        emit const_cast<TermCardDelegate*>(this)->closeEditor(editor);
    });
    return editor;
}

void TermCardDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    static_cast<TermEditor*>(editor)->setValues(index.data(TermListModel::HeardRole).toStringList().join(QStringLiteral(", ")),
                                                index.data(TermListModel::CorrectRole).toString());
}

void TermCardDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* e = static_cast<TermEditor*>(editor);
    model->setData(index, QStringList{ e->heard(), e->correct() }, Qt::EditRole);
}

void TermCardDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& opt, const QModelIndex&) const
{
    editor->setGeometry(cardRect(opt));
}
