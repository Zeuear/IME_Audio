#pragma once
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QPointer>
#include <QRectF>
#include <QScreen>
#include <QTextDocument>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <optional>

class LiveCaptionModel;

// @brief 悬浮窗：置顶且不抢焦点。无字幕时是一颗频谱胶囊，说话时变形为字幕卡片
//        （上方文字，浅色 = 识别中、深色 = 已确定，下方一排频谱圆点）。配色跟随应用主题。
class LiveOverlay : public QWidget
{
    Q_OBJECT
public:
    enum State { Loading, Listening, Paused, Transcribe };
    enum class Theme { Dark, Gray, Light };

    explicit LiveOverlay(QWidget *parent = nullptr);

    State state() const { return m_state; }
    void setTheme(Theme theme);
    void setCaptionFontSize(int pt);
    void setAtTop(bool atTop);
    bool atTop() const { return m_atTop; }

    void setCaptionModel(LiveCaptionModel *model);
    bool hasCaption() const;

    QRectF cardRect() const;
    QSizeF targetCardSize() const { return QSizeF(m_targetW, m_targetH); }
    bool isSettled() const;

    // 悬停时卡片右侧出现「隐藏」「拖动」两个图标按钮；隐藏后只能经托盘菜单重新显示
    bool userHidden() const { return m_userHidden; }
    void setHideButtonEnabled(bool enabled) { m_hideButtonEnabled = enabled; }
    void restoreFromUserHide();
    // 悬停按钮的位置（窗口坐标）：绘制、命中判断和测试共用
    QRectF hideButtonRect() const { return buttonRect(Btn::Hide); }
    QRectF dragButtonRect() const { return buttonRect(Btn::Drag); }
    bool buttonsAreVertical() const { return buttonsVertical(); }
    // 拖动后的位置（卡片贴屏幕那条边的中点），存 ini；无则用默认的屏幕居中
    void loadSavedPosition();

public slots:
    void setSpectrumLevels(const QVector<float>& bands);
    void setLevel(float normalizedLevel);
    void setLoading();
    void setListening();
    void setTranscribe();
    void setPaused();
    void showOnScreen();
    void hideOverlay();
    // 留住悬浮窗：期间的隐藏请求先记下，放开时才执行。指令条显示时用，两者一起消失
    void setHideHeld(bool held);
    void hideTimerStart();
    void hideTimerStop();
    void onVoiceStarted();
    void onVoiceStopped();

signals:
    void sphereClicked();
    void hiddenByUser();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    enum class Icon { None, Play, Pause };

    void setState(State s);
    void tick();
    void relayoutCaption();
    void syncWindowSize();
    void positionOnScreen();
    QScreen *targetScreen() const;
    qreal cardTop(qreal cardH) const;
    void paintCard(QPainter &p, const QPointF &center, qreal opacity) const;
    void paintPillBars(QPainter &p, qreal opacity) const;
    void paintCaptionContent(QPainter &p, const QRectF &card, qreal opacity) const;
    void paintDots(QPainter &p, const QPointF &center, const QColor &color, qreal opacity) const;
    void paintIcon(QPainter &p, const QPointF &center) const;

    enum class Btn { None, Hide, Drag };
    // 按钮排在卡片右侧：卡片够高就竖排，否则横排
    bool buttonsVertical() const;
    QRectF buttonRect(Btn b) const;
    Btn buttonAt(const QPointF &pos) const;
    void paintButtons(QPainter &p) const;
    QImage buttonIcon(Btn b, const QColor &color, int px) const;
    void updateInputMask();
    QPoint currentAnchor() const;

    State m_state = Loading;
    Theme m_theme = Theme::Dark;
    qreal m_fontPt = 12;
    bool m_atTop = false;
    qreal m_level = 0.0;
    QVector<float> m_levels;

    // 动画状态由 tick() 按真实时间步长推进
    QVector<qreal> m_barEnergy;
    qreal m_smoothedLevel = 0.0;
    qreal m_scale = 0.7;
    qreal m_loadingOpacity = 1.0;
    qreal m_transcribeOpacity = 0.0;
    qreal m_spectrumOpacity = 0.0;
    qreal m_dotsMs = 0.0;
    Icon m_icon = Icon::None;
    qreal m_iconOpacity = 0.0;
    qreal m_iconHoldSec = 0.0;

    QTimer *m_animTimer = nullptr;
    QElapsedTimer m_clock;
    QTimer *m_idleFadeTimer = nullptr;
    bool m_hideHeld = false;
    bool m_hideDeferred = false;

    // 悬停按钮、拖动、用户隐藏
    bool m_hover = false;
    qreal m_btnOpacity = 0.0;
    Btn m_hoverBtn = Btn::None;
    Btn m_pressBtn = Btn::None;
    bool m_dragging = false;
    QPoint m_dragStartGlobal, m_dragStartAnchor;
    std::optional<QPoint> m_anchor;           // 用户拖过的位置；空 = 默认居中
    bool m_userHidden = false;
    bool m_hideButtonEnabled = true;
    QRect m_lastMaskRect;
    mutable QHash<QString, QImage> m_iconCache;

    QPointer<QScreen> m_screen;               // 本次显示所在的屏幕，拔掉显示器时自动置空
    QPointer<LiveCaptionModel> m_caption;
    QTextDocument m_doc;
    qreal m_targetW = 120, m_targetH = 60;
    qreal m_cardW = 120, m_cardH = 60;
    qreal m_targetScroll = 0, m_scroll = 0;   // 超过两行时的垂直滚动，<= 0
    qreal m_capT = 0;                         // 0 = 胶囊，1 = 字幕卡片
};
