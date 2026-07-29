#pragma once
#include <QWidget>
#include <QQuickWidget>
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

signals:
    void sphereClicked(); 

private:
    QQuickWidget *m_quickWidget;
    SphereController *m_controller;
};