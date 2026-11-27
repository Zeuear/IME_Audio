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

    bool userHidden() const { return m_userHidden; }
    void setHideButtonEnabled(bool enabled) { m_hideButtonEnabled = enabled; }
    void restoreFromUserHide();
    QRectF hideButtonRect() const { return buttonRect(Btn::Hide); }
    QRectF dragButtonRect() const { return buttonRect(Btn::Drag); }
    bool buttonsAreVertical() const { return buttonsVertical(); }
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

    bool m_hover = false;
    qreal m_btnOpacity = 0.0;
    Btn m_hoverBtn = Btn::None;
    Btn m_pressBtn = Btn::None;
    bool m_dragging = false;
    QPoint m_dragStartGlobal, m_dragStartAnchor;
    std::optional<QPoint> m_anchor;
    bool m_userHidden = false;
    bool m_hideButtonEnabled = true;
    QRect m_lastMaskRect;
    mutable QHash<QString, QImage> m_iconCache;

    QPointer<QScreen> m_screen;
    QPointer<LiveCaptionModel> m_caption;
    QTextDocument m_doc;
    qreal m_targetW = 120, m_targetH = 60;
    qreal m_cardW = 120, m_cardH = 60;
    qreal m_targetScroll = 0, m_scroll = 0;
    qreal m_capT = 0;
};
