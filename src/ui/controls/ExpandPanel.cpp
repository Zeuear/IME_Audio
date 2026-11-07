#include "ExpandPanel.h"
#include "FloatingWindow.h"
#include <QEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr QSize kPanelSize(780, 520);
constexpr int kDictionaryIndex = 0;
constexpr int kFileIndex = 1;
}

ExpandPanel::ExpandPanel(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    // 打开时不激活：前台窗口不被切走，听写注入目标不变；点进输入框时才拿焦点
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAutoFillBackground(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);   // 留出边框
    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);
    connect(m_tabs, &QTabWidget::currentChanged, this, &ExpandPanel::onTabChanged);

    resize(kPanelSize);
}

void ExpandPanel::setPages(QWidget* dictionary, QWidget* file)
{
    // 加第一页时 QTabWidget 会发 currentChanged(0)，那不是用户打开了词典页，不能当成状态变化
    m_syncingTab = true;
    m_tabs->addTab(dictionary, QString());
    m_tabs->addTab(file, QString());
    m_syncingTab = false;
    retranslateUi();
}

void ExpandPanel::attachTo(QWidget* anchor)
{
    if (m_anchor) m_anchor->removeEventFilter(this);
    m_anchor = anchor;
    if (m_anchor) m_anchor->installEventFilter(this);
}

void ExpandPanel::showPage(PanelState::Page page)
{
    if (page == PanelState::Page::None) {
        hide();   // hideEvent 负责把状态复位并报告，避免这里再报一次
        return;
    }

    const bool wasOpen = isVisible();
    const bool changed = page != m_page;
    m_page = page;

    m_syncingTab = true;
    m_tabs->setCurrentIndex(page == PanelState::Page::Dictionary ? kDictionaryIndex : kFileIndex);
    m_syncingTab = false;

    if (!wasOpen) {
        reposition();
        show();   // WA_ShowWithoutActivating：显示但不激活
    }
    if (changed || !wasOpen) emit pageChanged(m_page);
}

void ExpandPanel::onTabChanged(int index)
{
    // 程序切页时 showPage 自己会报；面板收着时 tab 变了也不算打开了哪一页
    if (m_syncingTab || !isVisible()) return;
    const PanelState::Page page = index == kDictionaryIndex ? PanelState::Page::Dictionary
                                                            : PanelState::Page::File;
    if (page == m_page) return;
    m_page = page;
    emit pageChanged(m_page);
}

void ExpandPanel::reposition()
{
    if (!m_anchor) return;
    move(FloatingWindow::placeBelow(m_anchor->frameGeometry(), size(),
                                    FloatingWindow::availableScreens(), FloatingWindow::primaryScreenArea()));
}

void ExpandPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // 最小化等自发的隐藏不算收起
    if (event->spontaneous() || m_page == PanelState::Page::None) return;
    m_page = PanelState::Page::None;
    emit pageChanged(m_page);
}

void ExpandPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        showPage(PanelState::Page::None);
        return;
    }
    QWidget::keyPressEvent(event);
}

bool ExpandPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_anchor && isVisible()) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
            reposition();   // 横条拖到哪，面板跟到哪
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ExpandPanel::paintEvent(QPaintEvent*)
{
    // 边框取自调色板，三个主题都可读
    QPainter p(this);
    QColor border = palette().color(QPalette::WindowText);
    border.setAlpha(60);
    p.setPen(border);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ExpandPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void ExpandPanel::retranslateUi()
{
    if (m_tabs->count() < 2) return;
    m_tabs->setTabText(kDictionaryIndex, tr("Dictionary"));
    m_tabs->setTabText(kFileIndex, tr("File Transcription"));
}
