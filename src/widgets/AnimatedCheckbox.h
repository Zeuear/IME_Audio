#ifndef ANIMATECHECKBOX_H
#define ANIMATECHECKBOX_H

#include <QCheckBox>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QApplication>
#include <QMouseEvent>

class AnimatedCheckBox : public QCheckBox
{
    Q_OBJECT
    Q_PROPERTY(float animationProgress READ getAnimationProgress WRITE setAnimationProgress)

public:
    AnimatedCheckBox(QWidget* parent = nullptr);
    ~AnimatedCheckBox();

    void painterCenter(QPainter& painter, QRectF& boxRect, QPoint offsetPos);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    float getAnimationProgress() const { return m_animationProgress; }
    void setAnimationProgress(float value){ m_animationProgress = value; update(); }

    void startAnimation();
    void changeState();

private:
    QPropertyAnimation* m_animation;
    float m_animationProgress;
};


#endif // ANIMATECHECKBOX_H