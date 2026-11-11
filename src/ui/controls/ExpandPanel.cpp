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
constexpr int kTranscriptIndex = 0;
constexpr int kDictionaryIndex = 1;
constexpr int kFileIndex = 2;
}

int ExpandPanel::indexOf(PanelState::Page page)
{
    switch (page) {
    case PanelState::Page::Transcript: return kTranscriptIndex;
    case PanelState::Page::Dictionary: return kDictionaryIndex;
    default: return kFileIndex;
    }
}

PanelState::Page ExpandPanel::pageAt(int index)
{
    switch (index) {
    case kTranscriptIndex: return PanelState::Page::Transcript;
    case kDictionaryIndex: return PanelState::Page::Dictionary;
    default: return PanelState::Page::File;
    }
}

ExpandPanel::ExpandPanel(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    // 打开时不激活：前台窗口不被切走，听写注入目标不变；点进输入框时才拿焦点
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAutoFillBackground(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);
    connect(m_tabs, &QTabWidget::currentChanged, this, &ExpandPanel::onTabChanged);

    resize(kPanelSize);
}

void ExpandPanel::setPages(QWidget* transcript, QWidget* dictionary, QWidget* file)
{
    // 加第一页时 QTabWidget 会发 currentChanged(0)，那不是用户打开了某一页，不能当成状态变化
    m_syncingTab = true;
    m_tabs->addTab(transcript, QString());
    m_tabs->addTab(dictionary, QString());
    m_tabs->addTab(file, QString());
    m_syncingTab = false;
    retranslateUi();
}

void ExpandPanel::setAlwaysOnTop(bool on)
{
    m_changingFlags = true;
    FloatingWindow::setAlwaysOnTop(this, on);
    m_changingFlags = false;
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
    m_tabs->setCurrentIndex(indexOf(page));
    m_syncingTab = false;

    if (!wasOpen) {
        reposition();
        show();
    }
    if (changed || !wasOpen) emit pageChanged(m_page);
}

void ExpandPanel::onTabChanged(int index)
{
    // 程序切页时 showPage 自己会报；面板收着时 tab 变了也不算打开了哪一页
    if (m_syncingTab || !isVisible()) return;
    const PanelState::Page page = pageAt(index);
    if (page == m_page) return;
    m_page = page;
    emit pageChanged(m_page);
}

void ExpandPanel::reposition()
{
    if (!m_anchor) return;
    // 锚点窗口四周若留了透明阴影边距，要按可见主体对齐，否则面板离横条太远
    const int inset = m_anchor->property("visualInset").toInt();
    move(FloatingWindow::placeBelow(m_anchor->frameGeometry().adjusted(inset, inset, -inset, -inset), size(),
                                    FloatingWindow::availableScreens(), FloatingWindow::primaryScreenArea()));
}

void ExpandPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // 最小化等自发的隐藏、改置顶标志时的临时隐藏，都不算收起
    if (event->spontaneous() || m_changingFlags || m_page == PanelState::Page::None) return;
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
            reposition();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ExpandPanel::paintEvent(QPaintEvent*)
{
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
    if (m_tabs->count() < 3) return;
    m_tabs->setTabText(kTranscriptIndex, tr("Live Transcript"));
    m_tabs->setTabText(kDictionaryIndex, tr("Dictionary"));
    m_tabs->setTabText(kFileIndex, tr("File Transcription"));
}
