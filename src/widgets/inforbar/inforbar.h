#ifndef MYINFORBAR_H
#define MYINFORBAR_H

#include <QWidget>

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QTimer>
#include <QPainter>
#include <QStyle>
#include <QGraphicsOpacityEffect>
#include "appleshadoweffect.h"

// C++ 11引入的加强型枚举
enum  InforBarPosition {
   I_TOP,
   I_BOTTOM,
   I_TOP_LEFT,
   I_TOP_RIGHT,
   I_BOTTOM_LEFT,
   I_BOTTOM_RIGHT,
   I_NONE
};

class InforBarManager;

class InforBar : public QFrame {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ getOpacity WRITE setOpacity)
public:
    enum InforBarType { Info, Success, Warning, Error };
    enum InforCondition { Show, Hide };

    InforBar(const QString& title,
            const QString& content,
            InforBarType type,
            InforBarPosition position,
            Qt::Orientation orient = Qt::Horizontal,
            int duration = 3000,
            bool isClosable= true,
            QWidget* parent = nullptr)
        : QFrame(parent),
          m_title(title),
          m_content(content),
          m_type(type),
          m_position(position),
          m_orient(orient),
          m_duration(duration),
          m_isClosable(isClosable),
          m_opacity(1)
    {
        if (parent == nullptr)
        {
            setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
            setAttribute(Qt::WA_TranslucentBackground);
        }
        initUI();
        initEffect();
        initAnimation();
    }

    ~InforBar(){}

    static InforBar* newInforBar(const QString& title,const QString& content,
                                 InforBarType type, InforBarPosition position,QWidget* parent = nullptr) {
        InforBar* inforBar = new InforBar(title, content, type, position, Qt::Horizontal, 1000, true, parent);
        inforBar->show();
        inforBar->raise();
        return inforBar;
    }

    static InforBar* info(const QString& title, const QString& content,  InforBarPosition position,QWidget* parent = nullptr) {
        return newInforBar(title, content, InforBarType::Info, position, parent);
    }

    static InforBar* success(const QString& title, const QString& content,  InforBarPosition position,QWidget* parent = nullptr) {
        return newInforBar(title, content, InforBarType::Success, position, parent);
    }

    static InforBar* warning(const QString& title, const QString& content,  InforBarPosition position,QWidget* parent = nullptr) {
        return newInforBar(title, content, InforBarType::Warning, position, parent);
    }

    static InforBar* error(const QString& title, const QString& content,  InforBarPosition position,QWidget* parent = nullptr) {
        return newInforBar(title, content, InforBarType::Error, position, parent);
    }


private:
    void initUI() ;
    void initEffect();
    void initAnimation();
    void __fadeOut();
    void __fadeIn();
    int calculateWidth() const; // 新增：计算自适应宽度


    void setColor(const QColor color);
    void setIcon(const QIcon icon);
    void setText(const QString title="", const QString content="");

    qreal getOpacity()const { return m_opacity; };
    void setOpacity(qreal opacity) { m_opacity = opacity; update(); };

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent*event) override;
    void closeEvent(QCloseEvent* event) override;

signals:
    void closeSignal();

private:
    InforBarType m_type;
    InforCondition m_condition;

    QString m_title;
    QString m_content;
    QIcon m_icon;
    QColor m_color;
    InforBarPosition m_position;
    Qt::Orientation m_orient;
    int m_duration;
    bool m_isClosable;
    AppleShadowEffect* m_shadow_effect;
    QGraphicsOpacityEffect* m_opacity_effect;
    QPropertyAnimation* m_animation;
    QPropertyAnimation* m_opacity_animation;
    qreal m_opacity;

    QPixmap m_cache; // 新增：绘制缓存

    int m_icoSize = 18;
    int m_margin = 10;
    int m_iconSpacing = 10;
    int m_textSpacing = 10;
};


#endif // MYInforBar_H
