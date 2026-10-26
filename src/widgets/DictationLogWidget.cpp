#include "DictationLogWidget.h"
#include "DictationHistoryModel.h"
#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {
constexpr int kRowHeight = 26;
constexpr int kPad = 8;
constexpr int kTimeWidth = 64;
constexpr int kCopyWidth = 44;

// 悬停时"复制"所在的区域；绘制和点击判断共用
QRect copyRect(const QRect& row)
{
    return QRect(row.right() - kPad - kCopyWidth, row.top(), kCopyWidth, row.height());
}

class DictationLogDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const override
    {
        return QSize(opt.rect.width(), kRowHeight);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override
    {
        const QPalette& pal = opt.palette;
        const bool hovered = opt.state & QStyle::State_MouseOver;
        const QRect r = opt.rect;
        p->save();
        if (hovered) {
            QColor bg = pal.color(QPalette::Highlight);
            bg.setAlpha(40);
            p->fillRect(r, bg);
        }

        p->setPen(pal.color(QPalette::PlaceholderText));
        const QRect timeRect(r.left() + kPad, r.top(), kTimeWidth, r.height());
        p->drawText(timeRect, Qt::AlignLeft | Qt::AlignVCenter, index.data(DictationHistoryModel::TimeRole).toString());

        const int textRight = hovered ? copyRect(r).left() - kPad : r.right() - kPad;
        const QRect textRect(timeRect.right() + kPad, r.top(), textRight - timeRect.right() - kPad, r.height());
        p->setPen(pal.color(QPalette::Text));
        // 多行听写只显示首行，完整内容见悬停提示
        const QString text = index.data().toString().simplified();
        p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                    opt.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width()));

        if (hovered) {
            p->setPen(pal.color(QPalette::Highlight));
            p->drawText(copyRect(r), Qt::AlignCenter, DictationLogWidget::tr("Copy"));
        }
        p->restore();
    }
};
} // namespace

DictationLogWidget::DictationLogWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DictationLogWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 表头沿用原日志面板的 qss（QLabel#identification_log_header）；清空按钮用布局放进表头右侧
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("identification_log_header"));
    auto* headerLayout = new QHBoxLayout(m_title);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    // 表头按钮：复制全部、清空；都是无边框的文字按钮，禁用时变灰
    const QString flatStyle = QStringLiteral(
        "QPushButton { border: none; padding: 0 6px; background: transparent; }"
        "QPushButton:disabled { color: palette(mid); }");
    auto makeHeaderButton = [&]() {
        auto* b = new QPushButton(m_title);
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(flatStyle);
        return b;
    };
    m_copyAllBtn = makeHeaderButton();
    m_clearBtn = makeHeaderButton();
    headerLayout->addStretch();
    headerLayout->addWidget(m_copyAllBtn);
    headerLayout->addWidget(m_clearBtn);

    m_list = new QListView(this);
    m_list->setObjectName(QStringLiteral("identification_log_list"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->setItemDelegate(new DictationLogDelegate(m_list));
    m_list->viewport()->installEventFilter(this);

    layout->addWidget(m_title);
    layout->addWidget(m_list, 1);

    connect(m_copyAllBtn, &QPushButton::clicked, this, &DictationLogWidget::copyAll);
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_model) m_model->clear();
    });
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& idx) { copyRow(idx.row()); });

    retranslateUi();
}

void DictationLogWidget::setModel(DictationHistoryModel* model)
{
    m_model = model;
    m_list->setModel(model);
    // 没有记录时"复制""清空"都没有意义，置灰
    connect(model, &QAbstractItemModel::rowsInserted, this, &DictationLogWidget::updateButtons);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &DictationLogWidget::updateButtons);
    connect(model, &QAbstractItemModel::modelReset, this, &DictationLogWidget::updateButtons);
    updateButtons();
}

void DictationLogWidget::updateButtons()
{
    const bool has = m_model && m_model->rowCount() > 0;
    m_copyAllBtn->setEnabled(has);
    m_clearBtn->setEnabled(has);
}

void DictationLogWidget::copyAll()
{
    if (!m_model) return;
    const QString text = m_model->allText();
    if (text.isEmpty()) return;
    QGuiApplication::clipboard()->setText(text);
    emit copied();
}

void DictationLogWidget::copyRow(int row)
{
    if (!m_model) return;
    const QString text = m_model->textAt(row);
    if (text.isEmpty()) return;
    QGuiApplication::clipboard()->setText(text);
    emit copied();
}

bool DictationLogWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_list->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QModelIndex idx = m_list->indexAt(me->position().toPoint());
        if (idx.isValid() && copyRect(m_list->visualRect(idx)).contains(me->position().toPoint())) {
            copyRow(idx.row());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DictationLogWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void DictationLogWidget::retranslateUi()
{
    m_title->setText(tr("Recognition Log"));
    m_copyAllBtn->setText(tr("Copy"));
    m_copyAllBtn->setToolTip(tr("Copy all records to the clipboard"));
    m_clearBtn->setText(tr("Clear"));
}
