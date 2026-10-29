#include "inforbar.h"
#include "inforbarmanager.h"
#include "painttool.h"
#include <QPainterPath>


void InforBar::initUI()
{
    setStyleSheet("background: transparent;");
    setColor(QColor("#ffffff"));

    switch (m_type) {
    case Info:
        setIcon(QIcon(QStringLiteral(":/info")));
        setColor(QColor(230, 244, 255));
        break;

    case Success:
        setIcon(QIcon(QStringLiteral(":/info_success")));
        setColor(QColor(246, 255, 237));
        break;

    case Warning:
        setIcon(QIcon(QStringLiteral(":/info_warning")));
        setColor(QColor(255, 251, 230));
        break;

    case Error:
        setIcon(QIcon(QStringLiteral(":/info_error")));
        setColor(QColor(255, 241, 240));
        break;
    }
    resize(calculateWidth(), 40);
}


void InforBar::initAnimation()
{
    m_opacity_animation = new QPropertyAnimation(this, "opacity", this);
    m_opacity_animation->setObjectName("opacity_animation");
    m_opacity_animation->setDuration(400);
    m_opacity_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_opacity_animation, &QPropertyAnimation::finished, this, [=]() {
        if (m_condition == InforCondition::Hide)
            close();
    });


}

void InforBar::initEffect()
{
    //m_shadow_effect = new AppleShadowEffect(this);
    //m_shadow_effect->setBlurRadius(15);
    //m_shadow_effect->setInnerDistance(15);
    //m_shadow_effect->setOffset(QPointF(0, 4));
    //m_shadow_effect->setColor(QColor(0, 0, 0, 120));

    //if(!parentWidget())
    //    setGraphicsEffect(m_shadow_effect);
}


void InforBar::showEvent(QShowEvent* event)
{
    Q_UNUSED(event)
    if (m_duration >= 0)
    {
        QTimer::singleShot(m_duration, this, &InforBar::__fadeOut);
    }

    if (m_position != InforBarPosition::I_NONE)
    {
        InforBarManager* manager = InforBarManager::make(m_position);
        manager->add(this);
    }
    if (this->parentWidget())
    {
        this->parentWidget()->installEventFilter(this);
    }
    else
    {
        __fadeIn();
    }
}


void InforBar::paintEvent(QPaintEvent*)
{
    qreal dpr = devicePixelRatioF();
    QSize targetDeviceSize = size() * dpr;

    if (m_cache.isNull() || m_cache.size() != targetDeviceSize) {
        m_cache = QPixmap(targetDeviceSize);
        m_cache.setDevicePixelRatio(dpr);

        m_cache.fill(Qt::transparent);
        QPainter cachePainter(&m_cache);
        cachePainter.setRenderHints(QPainter::Antialiasing);
        cachePainter.setPen(Qt::NoPen);

        QRect infoRect = rect().adjusted(5, 5, -5, -5);
        QPainterPath path;
        path.addRect(rect());
        QPainterPath path2;
        path2.addRoundedRect(infoRect, 10, 10);

        cachePainter.setOpacity(1.0); // 缓存绘制时不应用透明度
        PaintTool::paintShadow(cachePainter, width(), height(), QColor(0, 0, 0, 22), path - path2);
        cachePainter.setBrush(m_color);
        cachePainter.drawRoundedRect(infoRect, 10, 10);

        int currentX = m_margin;
        int yCenter = height() / 2;
        if (!m_icon.isNull()) {
            QPixmap pixmap = m_icon.pixmap(QSize(m_icoSize, m_icoSize));
            cachePainter.drawPixmap(currentX, yCenter - m_icoSize / 2, pixmap);
            currentX += m_icoSize + m_iconSpacing;
        }

        QFont font(QStringLiteral("Roboto"));
        font.setPointSize(10);
        cachePainter.setFont(font);
        cachePainter.setPen(Qt::black);
        cachePainter.drawText(currentX, yCenter + fontMetrics().ascent() / 2, m_title);
        currentX += fontMetrics().horizontalAdvance(m_title) + m_textSpacing;
        cachePainter.drawText(currentX, yCenter + fontMetrics().ascent() / 2, m_content);
    }
    QPainter painter(this);
    painter.setOpacity(m_opacity);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, m_cache);
   
    
}

void InforBar::closeEvent(QCloseEvent* event)
{
    Q_UNUSED(event)
    emit closeSignal();
}


void InforBar::__fadeOut()
{
    if (m_isClosable)
    {
        m_condition = InforCondition::Hide;
        m_opacity_animation->setStartValue(1.0);
        m_opacity_animation->setEndValue(0.0);
        m_opacity_animation->start();
    }
}

void InforBar::__fadeIn()
{
    m_condition = InforCondition::Show;
    m_opacity_animation->setStartValue(0.0);
    m_opacity_animation->setEndValue(1.0);
    m_opacity_animation->start();
}


int InforBar::calculateWidth() const {
    const int iconSize = m_icoSize; // 图标大小
    const int margin = m_margin;   // 左右边距
    const int iconSpacing = m_iconSpacing; // 图标与文本间距
    const int textSpacing = m_textSpacing; // 标题与内容间距
    const int shadowAdjustment = 10; // 阴影调整（5px 每侧）

    // 计算文本宽度
    QFont font(QStringLiteral("Roboto"));
    font.setPointSize(11);
    QFontMetrics fm(font);
    int titleWidth = fm.horizontalAdvance(m_title);
    int contentWidth = fm.horizontalAdvance(m_content);

    // 计算总宽度
    int totalWidth = 2 * margin + shadowAdjustment; // 左右边距 + 阴影
    if (!m_icon.isNull()) {
        totalWidth += iconSize + iconSpacing; // 图标 + 间距
    }
    totalWidth += titleWidth + textSpacing + contentWidth; // 标题 + 间距 + 内容

    // 确保最小宽度
    const int minWidth = 200;
    return qMax(totalWidth, minWidth);
}


void InforBar::setColor(const QColor color)
{
    m_color = color;
    m_cache = QPixmap(); // 失效缓存
    update();
}


void InforBar::setIcon(const QIcon icon)
{
    m_icon = icon;
    m_cache = QPixmap(); // 失效缓存
    update();
}


void InforBar::setText(const QString title, const QString content)
{
    m_title = title;
    m_content = content;
    resize(calculateWidth(), height());
    m_cache = QPixmap(); // 失效缓存
    update();
}