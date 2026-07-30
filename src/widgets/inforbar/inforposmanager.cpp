#include "inforposmanager.h"
#include <QGuiApplication>

// TopInforBarManager
QPoint TopInforBarManager::pos(InforBar* inforBar, const QSize& _parentSize) const
{
    QWidget* p = inforBar->parentWidget();
    QSize parentSize;
    if (!p) {
        parentSize = QGuiApplication::primaryScreen()->availableGeometry().size();
    }
    else {
        parentSize = !_parentSize.isEmpty() ? _parentSize : p->size();
    }

    int x = (parentSize.width() - inforBar->width()) / 2;
    int y = m_margin;
    int index = m_inforBars[p].indexOf(inforBar);
    for (int i = 0; i < index; ++i)
        y += m_inforBars[p][i]->height() + m_spacing;
    return QPoint(x, y);
}

QPoint TopInforBarManager::slideStartPos(InforBar* inforBar) const
{
    auto pos = this->pos(inforBar);
    return QPoint(pos.x(), pos.y() - 16);
}


// TopRightInfoBarManager
QPoint TopRightInfoBarManager::pos(InforBar* inforBar, const QSize& _parentSize) const
{
    QWidget* p = inforBar->parentWidget();
    QSize parentSize;
    if (!p) {
        parentSize = QGuiApplication::primaryScreen()->availableGeometry().size();
    }
    else {
        parentSize = !_parentSize.isEmpty() ? _parentSize : p->size();
    }

    int x = parentSize.width() - inforBar->width() - m_margin;
    int y = m_margin;
    int index = m_inforBars[p].indexOf(inforBar);
    for (int i = 0; i < index; ++i)
        y += m_inforBars[p][i]->height() + m_spacing;
    return QPoint(x, y);
}

QPoint TopRightInfoBarManager::slideStartPos(InforBar* inforBar) const
{
    QWidget* p = inforBar->parentWidget();
    int x;
    if (p) {
        x = p->width();
    }
    else {
        x = QGuiApplication::primaryScreen()->availableGeometry().width();
    }

    auto pos = this->pos(inforBar);
    return QPoint(x, pos.y());
}



// BottomRightInfoBarManager
QPoint BottomRightInfoBarManager::pos(InforBar* inforBar, const QSize& _parentSize) const
{
    QWidget* p = inforBar->parentWidget();
    QSize parentSize;
    if (!p) {
        parentSize = QGuiApplication::primaryScreen()->availableGeometry().size();
    }
    else {
        parentSize = !_parentSize.isEmpty() ? _parentSize : p->size();
    }

    int x = parentSize.width() - inforBar->width() - m_margin;
    int y = parentSize.height()-inforBar->height() - m_margin;
    int index = m_inforBars[p].indexOf(inforBar);
    for (int i = 0; i < index; ++i)
        y -= m_inforBars[p][i]->height() + m_spacing;
    return QPoint(x, y);
}

QPoint BottomRightInfoBarManager::slideStartPos(InforBar* inforBar) const
{
    QWidget* p = inforBar->parentWidget();
    int x;
    if (p) {
        x = p->width();
    }
    else {
        x = QGuiApplication::primaryScreen()->availableGeometry().width();
    }

    auto pos = this->pos(inforBar);
    return QPoint(x, pos.y());
}



// TopLeftInfoBarManager
QPoint TopLeftInfoBarManager::pos(InforBar* inforBar, const QSize& _parentSize) const
{
    QWidget* p = inforBar->parentWidget();
    QSize parentSize;
    if (!p) {
        parentSize = QGuiApplication::primaryScreen()->availableGeometry().size();
    }
    else {
        parentSize = !_parentSize.isEmpty() ? _parentSize : p->size();
    }

    int y =  m_margin;
    int index = m_inforBars[p].indexOf(inforBar);
    for (int i = 0; i < index; ++i)
        y += m_inforBars[p][i]->height() + m_spacing;
    return QPoint(m_margin, y);
}

QPoint TopLeftInfoBarManager::slideStartPos(InforBar* inforBar) const
{
    auto pos = this->pos(inforBar);
    return QPoint(-inforBar->width(), pos.y());
}





// BottomLeftInfoBarManager
QPoint BottomLeftInfoBarManager::pos(InforBar* inforBar, const QSize& _parentSize) const
{
    QWidget* p = inforBar->parentWidget();
    QSize parentSize;
    if (!p) {
        parentSize = QGuiApplication::primaryScreen()->availableGeometry().size();
    }
    else {
        parentSize = !_parentSize.isEmpty() ? _parentSize : p->size();
    }

    int y = parentSize.height()-inforBar->height() - m_margin;
    int index = m_inforBars[p].indexOf(inforBar);
    for (int i = 0; i < index; ++i)
        y -= m_inforBars[p][i]->height() + m_spacing;
    return QPoint(m_margin, y);
}

QPoint BottomLeftInfoBarManager::slideStartPos(InforBar* inforBar) const
{
    auto pos = this->pos(inforBar);
    return QPoint(-inforBar->width(), pos.y());
}



// BottomInfoBarManager
QPoint BottomInfoBarManager::pos(InforBar* inforBar, const QSize& _parentSize) const
{
    QWidget* p = inforBar->parentWidget();
    QSize parentSize;
    if (!p) {
        parentSize = QGuiApplication::primaryScreen()->availableGeometry().size();
    }
    else {
        parentSize = !_parentSize.isEmpty() ? _parentSize : p->size();
    }

    int x = (parentSize.width() - inforBar->width()) / 2;
    int y =  parentSize.height() - inforBar->height() - m_margin;
    int index = m_inforBars[p].indexOf(inforBar);
    for (int i = 0; i < index; ++i)
        y -= m_inforBars[p][i]->height() + m_spacing;
    return QPoint(x, y);
}

QPoint BottomInfoBarManager::slideStartPos(InforBar* inforBar) const
{
    auto pos = this->pos(inforBar);
    return QPoint(pos.x(), pos.y() + 16);
}


