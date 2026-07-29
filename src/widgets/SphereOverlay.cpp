#include "SphereOverlay.h"
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QQmlContext>

SphereOverlay::SphereOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    resize(120, 120);

    m_controller = new SphereController(this);

    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_quickWidget->setClearColor(Qt::transparent); 
    m_quickWidget->rootContext()->setContextProperty("sphereController", m_controller);
    m_quickWidget->setSource(QUrl("qrc:/qml/main.qml"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quickWidget);

    connect(m_controller, &SphereController::sphereClicked, this, &SphereOverlay::sphereClicked);
}

void SphereOverlay::setLevel(float normalizedLevel)
{
    m_controller->setLevel(normalizedLevel);
}

void SphereOverlay::setSpectrumLevels(const QVector<float>& bands)
{
    m_controller->setSpectrumLevels(bands);
}

void SphereOverlay::setLoading()  { m_controller->setState(SphereController::Loading); }
void SphereOverlay::setListening() { m_controller->setState(SphereController::Listening); }
void SphereOverlay::setTranscribe(){ m_controller->setState(SphereController::Transcribe); }
void SphereOverlay::setPaused()   { m_controller->setState(SphereController::Paused); }

void SphereOverlay::showAtBottomCenter()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect geo = screen->availableGeometry();
        move(geo.center().x() - width() / 2, geo.bottom() - height() - 100);
    }
    show();
    raise();
}

void SphereOverlay::hideOverlay() { hide(); }