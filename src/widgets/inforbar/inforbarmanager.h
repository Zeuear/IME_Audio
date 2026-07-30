#ifndef InforBarManager_H
#define InforBarManager_H

#include <QObject>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QResizeEvent>
#include <QDebug>
#include <QWeakPointer>
#include <functional>
#include "inforbar.h"


class InforBarManager : public QObject
{
    Q_OBJECT
public:
    static InforBarManager* m_instance;
    static QMap<InforBarPosition, InforBarManager*> managers;

    QMap<QWidget*, QList<InforBar*>> m_inforBars;
    QMap<QWidget*, QParallelAnimationGroup*> m_aniGroups;
    QList<QPropertyAnimation*> m_slideAnis;
    QList<QPropertyAnimation*> m_dropAnis;
    int m_spacing;
    int m_margin;
    bool m_initialized;

    QTimer* m_addTimer; 
    QList<InforBar*> m_pendingInforBars; 


public:
    static InforBarManager* instance();
    InforBarManager();

    template <typename T>
    static void registerManager(InforBarPosition position){
        static_assert (std::is_base_of<InforBarManager, T>::value,  "T must be derived from InfoBarManager");
        InforBarManager* obj = managers.value(position);
        if(!obj){
            obj = static_cast<T*>(T::instance());
            managers.insert(position, obj);
        }
    }

    void add(InforBar* inforBar);
    void remove(InforBar* inforBar);
    static InforBarManager* make(InforBarPosition postion);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void initialVarible();
    void updateDropAnimation(QWidget* parent);
    QParallelAnimationGroup* createAnimationGroup(QWidget* parent);
    QPropertyAnimation* createDropAnimation(InforBar* inforBar);
    QPropertyAnimation* createSlideAnimation(InforBar* inforBar);
    virtual QPoint pos(InforBar* inforBar, const QSize& parentSize = QSize()) const;
    virtual QPoint slideStartPos(InforBar* inforBar) const;

private slots:
    void processPendingInforBars();

};






#endif // InforBarManager_H
