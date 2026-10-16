#include "AnimatedCheckbox.h"


AnimatedCheckBox::AnimatedCheckBox(QWidget* parent) :
    QCheckBox(parent), m_animationProgress(1.0)
{
    setStyleSheet("QCheckBox::indicator { width: 0px; height: 0px; }");
    setMinimumSize(30, 30); 
    setFixedSize(30, 30);

    m_animation = new QPropertyAnimation(this, "animationProgress", this);
    m_animation->setDuration(300);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad); 
}


AnimatedCheckBox::~AnimatedCheckBox()
{
    delete m_animation;
}

void AnimatedCheckBox::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        changeState();
        startAnimation();
    }   
}

void AnimatedCheckBox::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int boxSize = qMin(width(), height()) * 0.6;
    // 无文字时方框居中；带文字时靠左，文字画在右侧
    const qreal boxX = text().isEmpty() ? (width() - boxSize) / 2 : 4;
    QRectF boxRect(boxX, (height() - boxSize) / 2, boxSize, boxSize);

    painter.setPen(Qt::NoPen);

    if (!isChecked()) {
        painter.setBrush(QColor(100, 100, 100));
        painter.drawRoundedRect(boxRect, 2, 2);
    }
    else {
        QColor startColor = QColor("#1b60d2");
        QColor endColor = QColor("#154ba5");
        startColor.setAlphaF(m_animationProgress);
        endColor.setAlphaF(m_animationProgress);

        QLinearGradient gradient(QPoint(width() / 2, 0), QPoint(width() / 2, height()));
        gradient.setColorAt(0, startColor);
        gradient.setColorAt(0.7, endColor);
        gradient.setColorAt(1, endColor);

        painter.setBrush(gradient);
        painter.drawRoundedRect(boxRect, 2, 2);

        painter.setPen(QPen(QColor(0, 0, 0, 100), boxSize / 9, Qt::SolidLine, Qt::RoundCap));
        painterCenter(painter, boxRect, QPoint(0, 2));

        painter.setPen(QPen(Qt::white, boxSize / 9, Qt::SolidLine, Qt::RoundCap));
        painterCenter(painter, boxRect, QPoint(0, 0));
    }

    if (!text().isEmpty()) {
        painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
        const QRectF textRect(boxRect.right() + 6, 0, width() - boxRect.right() - 6, height());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
}

QSize AnimatedCheckBox::sizeHint() const
{
    if (text().isEmpty()) return QSize(30, 30);
    // 方框 18 + 左边距 4 + 间距 6 + 文字 + 右边距
    return QSize(28 + fontMetrics().horizontalAdvance(text()) + 6, 30);
}

void AnimatedCheckBox::setLabelText(const QString& text)
{
    setText(text);
    // 构造时固定了 30x30，带文字时按文字宽度重新固定，否则文字被裁掉
    setFixedSize(sizeHint());
}

void AnimatedCheckBox::painterCenter(QPainter& painter, QRectF& boxRect, QPoint offsetPos)
{
    int boxSize = boxRect.width();
    float progress = m_animationProgress;
    if (progress > 0) {
        float padding = boxSize * 0.3; 
        QPointF p1(boxRect.left() + padding * 0.9, boxRect.center().y());           
        QPointF p2(boxRect.center().x() - 1, boxRect.bottom() - padding);        
        QPointF p3(boxRect.right() - padding * 0.8, boxRect.top() + padding * 1.2);      

        p1 += offsetPos;
        p2 += offsetPos;
        p3 += offsetPos;

        if (progress <= 0.5) {
            float t = progress / 0.5;
            QPointF mid = p1 + (p2 - p1) * t;
            painter.drawLine(p1, mid);
        }
        else {
            painter.drawLine(p1, p2);
            float t = (progress - 0.5) / 0.5;
            QPointF mid = p2 + (p3 - p2) * t;
            painter.drawLine(p2, mid);
        }
    }
}

void AnimatedCheckBox::startAnimation()
{
    m_animation->stop();
    if (isChecked()) {
        m_animation->setStartValue(0.0);
        m_animation->setEndValue(1.0);
    }
    else {
        m_animation->setStartValue(1.0);
        m_animation->setEndValue(0.0);
    }
    m_animation->start();
}


void AnimatedCheckBox::changeState()
{
    setChecked(!isChecked());
    update();
}