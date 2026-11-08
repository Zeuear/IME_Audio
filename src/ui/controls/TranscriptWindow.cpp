#include "TranscriptWindow.h"
#include "FloatingWindow.h"
#include "ui/pages/LiveTranscriptWidget.h"
#include <QCloseEvent>
#include <QEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {
const QString kGeometryGroup = QStringLiteral("TranscriptWindow");
constexpr QSize kDefaultSize(760, 560);
}

TranscriptWindow::TranscriptWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    // 关闭它不该让程序退出：横条才是常驻入口
    setAttribute(Qt::WA_QuitOnClose, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    m_transcript = new LiveTranscriptWidget(this);
    layout->addWidget(m_transcript);

    resize(FloatingWindow::loadSize(kGeometryGroup).value_or(kDefaultSize));
    retranslateUi();
}

TranscriptWindow::~TranscriptWindow()
{
    saveGeometry();
}

void TranscriptWindow::openAndRaise()
{
    showNormal();
    raise();
    activateWindow();
}

void TranscriptWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_restored) {
        m_restored = true;
        // 大小在构造时已按保存值设好；位置经校正后再放，保存的屏幕拔掉了就回主屏
        FloatingWindow::restore(this, kGeometryGroup);
    }
    if (!event->spontaneous()) emit visibilityChanged(true);
}

void TranscriptWindow::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // 最小化也会发 hideEvent，但那是自发的，窗口仍算"开着"
    if (!event->spontaneous()) emit visibilityChanged(false);
}

void TranscriptWindow::setVisible(bool visible)
{
    // 隐藏之前保存：hideEvent 到来时窗口已经不可见，几何信息就不能当作用户摆放的位置了
    if (!visible) saveGeometry();
    QWidget::setVisible(visible);
}

void TranscriptWindow::closeEvent(QCloseEvent* event)
{
    // 只隐藏，稿子留着；退出走横条右键菜单或托盘
    hide();
    event->ignore();
}

void TranscriptWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void TranscriptWindow::retranslateUi()
{
    setWindowTitle(tr("Live Transcript"));
}

void TranscriptWindow::saveGeometry()
{
    if (!isVisible() || isMinimized()) return;   // 隐藏或最小化后的几何不是用户摆放的位置
    FloatingWindow::savePosition(kGeometryGroup, pos());
    FloatingWindow::saveSize(kGeometryGroup, size());
}
