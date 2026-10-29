#ifndef INFORPOSMANAGER_H
#define INFORPOSMANAGER_H

#include "inforbarmanager.h"

class TopInforBarManager : public InforBarManager
{
public:
    static TopInforBarManager* instance(){
        static TopInforBarManager instance;
        return &instance;
    }

protected:
    QPoint pos(InforBar* inforBar, const QSize& _parentSize = QSize()) const override;
    QPoint slideStartPos(InforBar* inforBar) const override;
};


class TopRightInfoBarManager : public InforBarManager
{
public:
    static TopRightInfoBarManager* instance(){
        static TopRightInfoBarManager instance;
        return &instance;
    }

protected:
    QPoint pos(InforBar* inforBar, const QSize& _parentSize = QSize()) const override;
    QPoint slideStartPos(InforBar* inforBar) const override;
};

class BottomRightInfoBarManager : public InforBarManager
{
public:
    static BottomRightInfoBarManager* instance(){
        static BottomRightInfoBarManager instance;
        return &instance;
    }

protected:
    QPoint pos(InforBar* inforBar, const QSize& _parentSize = QSize()) const override;
    QPoint slideStartPos(InforBar* inforBar) const override;
};

class TopLeftInfoBarManager : public InforBarManager
{

public:
    static TopLeftInfoBarManager* instance(){
        static TopLeftInfoBarManager instance;
        return &instance;
    }
protected:
    QPoint pos(InforBar* inforBar, const QSize& _parentSize = QSize()) const override;
    QPoint slideStartPos(InforBar* inforBar) const override;
};

class BottomLeftInfoBarManager : public InforBarManager
{

public:
    static BottomLeftInfoBarManager* instance(){
        static BottomLeftInfoBarManager instance;
        return &instance;
    }
protected:
    QPoint pos(InforBar* inforBar, const QSize& _parentSize = QSize()) const override;
    QPoint slideStartPos(InforBar* inforBar) const override;
};

class BottomInfoBarManager : public InforBarManager
{

public:
    static BottomInfoBarManager* instance(){
        static BottomInfoBarManager instance;
        return &instance;
    }
protected:
    QPoint pos(InforBar* inforBar, const QSize& _parentSize = QSize()) const override;
    QPoint slideStartPos(InforBar* inforBar) const override;
};


#endif // INFORPOSMANAGER_H
