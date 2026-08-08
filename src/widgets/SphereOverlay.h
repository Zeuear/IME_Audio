#pragma once
#include <QWidget>
#include <QQuickWidget>
#include <QTimer>
#include "SphereController.h"

class SphereOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit SphereOverlay(QWidget *parent = nullptr);

public slots:
	void setSpectrumLevels(const QVector<float>& bands);
    void setLevel(float normalizedLevel);
    void setLoading();
    void setListening();
    void setTranscribe();
    void setPaused();
    void showAtBottomCenter();
    void hideOverlay();
    void hideTimerStart();
    void hideTimerStop();
    void onVoiceStarted();    // 人声活动可视化：停 fade 计时并显示监听中
    void onVoiceStopped();    // 人声停止：启动 fade 计时

signals:
    void sphereClicked(); 

private:
    QQuickWidget *m_quickWidget;
    SphereController *m_controller;
    QTimer* m_idleFadeTimer;
};