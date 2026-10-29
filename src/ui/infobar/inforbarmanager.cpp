#include "inforbarmanager.h"
#include "QDebug"

InforBarManager* InforBarManager::m_instance = nullptr;
QMap<InforBarPosition, InforBarManager*> InforBarManager::managers;

InforBarManager* InforBarManager::instance()
{
    if (!m_instance)
        m_instance = new InforBarManager();
    return m_instance;
}

InforBarManager::InforBarManager()
    : m_spacing(16), m_margin(24), m_initialized(false)
{
    initialVarible();
    m_addTimer = new QTimer(this);
    m_addTimer->setInterval(100);
    connect(m_addTimer, &QTimer::timeout, this, &InforBarManager::processPendingInforBars);

}

void InforBarManager::initialVarible()
{
    if (m_initialized) return;
    m_inforBars.clear();
    m_aniGroups.clear();
    m_pendingInforBars.clear();
    m_initialized = true;
}

QParallelAnimationGroup* InforBarManager::createAnimationGroup(QWidget* parent)
{
    QParallelAnimationGroup* group = new QParallelAnimationGroup(this);
    if (parent) {
        parent->installEventFilter(this);
    }
    return group;
}

QPropertyAnimation* InforBarManager::createDropAnimation(InforBar* inforBar)
{
    auto ani = new QPropertyAnimation(inforBar, "pos", this);
    ani->setDuration(400); // ͳһΪ400ms
    ani->setEasingCurve(QEasingCurve::OutCubic);
    return ani;
}

QPropertyAnimation* InforBarManager::createSlideAnimation(InforBar* inforBar)
{
    auto ani = new QPropertyAnimation(inforBar, "pos", this);
    ani->setDuration(400);
    ani->setEasingCurve(QEasingCurve::OutCubic);
    ani->setStartValue(slideStartPos(inforBar));
    ani->setEndValue(pos(inforBar));
    return ani;
}

QPoint InforBarManager::pos(InforBar* inforBar, const QSize& parentSize) const
{
    // To be implemented by subclasses.
    Q_UNUSED(inforBar)
    Q_UNUSED(parentSize)
    return QPoint();
}

QPoint InforBarManager::slideStartPos(InforBar* inforBar) const
{
    // To be implemented by subclasses.
    Q_UNUSED(inforBar)
    return QPoint();
}

void InforBarManager::add(InforBar* inforBar)
{
    //m_pendingInforBars.append(inforBar);
    //if (!m_addTimer->isActive()) {
    //    m_addTimer->start();
    //}

    auto parent = inforBar->parentWidget();
    //if (!parent)
    //    return;

    if (!m_aniGroups.contains(parent))
        m_inforBars[parent].clear();
    m_aniGroups[parent] = createAnimationGroup(parent);

    if (!m_inforBars[parent].contains(inforBar)) {
        m_inforBars[parent].append(inforBar);

        if (m_inforBars[parent].size() > 0) {
            auto dropAni = createDropAnimation(inforBar);
            inforBar->setProperty("dropAni", QVariant::fromValue<QObject*>(dropAni));
        }

        auto slideAni = createSlideAnimation(inforBar);
        inforBar->setProperty("slideAni", QVariant::fromValue<QObject*>(slideAni));
        connect(inforBar, &InforBar::closeSignal, this, [=]() { remove(inforBar); });
        slideAni->start();
    }
}

void InforBarManager::remove(InforBar* inforBar)
{
    auto parent = inforBar->parentWidget();
    if (!m_inforBars.keys().contains(parent)) return;

    if (m_inforBars[parent].contains(inforBar)) {
        m_inforBars[parent].removeOne(inforBar);

        updateDropAnimation(parent);
        m_aniGroups[parent]->start();
        //QTimer::singleShot(400, this, [=]() { updateDropAnimation(parent); });
    }
}


InforBarManager* InforBarManager::make(InforBarPosition position)
{
    if (!managers.contains(position)) {
        qWarning("Invalid InfoBarPosition: %d", static_cast<int>(position));
        return nullptr;
    }
    return managers[position];
}


void InforBarManager::processPendingInforBars() {
    if (m_pendingInforBars.isEmpty()) {
        m_addTimer->stop();
        return;
    }

    InforBar* inforBar = m_pendingInforBars.takeFirst();
    auto parent = inforBar->parentWidget();

    if (!m_aniGroups.contains(parent)) {
        m_inforBars[parent].clear();
        m_aniGroups[parent] = createAnimationGroup(parent);
    }

    if (!m_inforBars[parent].contains(inforBar)) {
        m_inforBars[parent].append(inforBar);

        if (m_inforBars[parent].size() > 1) {
            auto dropAni = createDropAnimation(inforBar);
            inforBar->setProperty("dropAni", QVariant::fromValue<QObject*>(dropAni));
        }

        auto slideAni = createSlideAnimation(inforBar);
        inforBar->setProperty("slideAni", QVariant::fromValue<QObject*>(slideAni));
        connect(inforBar, &InforBar::closeSignal, this, [=]() { remove(inforBar); });

        QParallelAnimationGroup* group = new QParallelAnimationGroup(this);
        group->addAnimation(slideAni);
        group->addAnimation(inforBar->findChild<QPropertyAnimation*>("opacity_animation"));
        group->start();
    }
}



bool InforBarManager::eventFilter(QObject* obj, QEvent* e)
{
    QWidget* o = reinterpret_cast<QWidget*>(obj);
    if (!m_inforBars.contains(o)) { return false; }

    if (e->type() == QEvent::Resize || e->type() == QEvent::WindowStateChange) {
        QResizeEvent* _e = static_cast<QResizeEvent*>(e);
        auto size = (e->type() == QEvent::Resize) ? _e->size() : QSize();
        for (auto inforBar : m_inforBars[o])
            inforBar->move(pos(inforBar, size));
    }
    return QObject::eventFilter(obj, e);
}


void InforBarManager::updateDropAnimation(QWidget* parent)
{
    m_aniGroups[parent] = createAnimationGroup(parent);
    for (int i = 0; i < m_inforBars[parent].size(); i++)
    {
        auto infoBar = m_inforBars[parent][i];
        QPropertyAnimation* ani = infoBar->property("dropAni").value<QPropertyAnimation*>();
        if (ani) {
            ani->setStartValue(infoBar->pos());
            ani->setEndValue(pos(infoBar));
            m_aniGroups[parent]->addAnimation(ani);
        }
    }
}


