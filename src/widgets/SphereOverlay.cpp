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

    m_idleFadeTimer = new QTimer(this);
    m_idleFadeTimer->setSingleShot(true);
    m_idleFadeTimer->setInterval(4000); 
    connect(m_idleFadeTimer, &QTimer::timeout, this, [this]() {
        if (m_controller->state() != SphereController::Paused) {
            hideOverlay();
        }
    });
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
        constexpr int margin = -5;  
        move(geo.center().x() - width() / 2, geo.bottom() - height() - margin);
    }
    show();
    raise();
}

void SphereOverlay::hideOverlay() { hide(); }
void SphereOverlay::hideTimerStart() { m_idleFadeTimer->start(); }
void SphereOverlay::hideTimerStop() { m_idleFadeTimer->stop(); }

void SphereOverlay::onVoiceStarted()
{
    // 人声开始：停 fade 计时并显示监听中（替代原 MainWin lambda 的跨层分支）
    m_idleFadeTimer->stop();
    m_controller->setState(SphereController::Listening);
    showAtBottomCenter();
}

void SphereOverlay::onVoiceStopped()
{
    // 人声停止：启动 fade 计时（屏保式淡出）
    m_idleFadeTimer->start();
}