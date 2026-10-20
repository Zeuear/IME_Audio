#pragma once
#include <QElapsedTimer>
#include <QPointer>
#include <QRectF>
#include <QTextDocument>
#include <QTimer>
#include <QVector>
#include <QWidget>

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

    void setCaptionModel(LiveCaptionModel *model);
    bool hasCaption() const;

    QRectF cardRect() const;
    QSizeF targetCardSize() const { return QSizeF(m_targetW, m_targetH); }
    bool isSettled() const;

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
    void onVoiceStarted();
    void onVoiceStopped();

signals:
    void sphereClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    enum class Icon { None, Play, Pause };

    void setState(State s);
    void tick();
    void relayoutCaption();
    void syncWindowSize();
    void positionBottomCenter();
    void paintCard(QPainter &p, const QPointF &center, qreal opacity) const;
    void paintPillBars(QPainter &p, qreal opacity) const;
    void paintCaptionContent(QPainter &p, const QRectF &card, qreal opacity) const;
    void paintDots(QPainter &p, const QPointF &center, const QColor &color, qreal opacity) const;
    void paintIcon(QPainter &p, const QPointF &center) const;

    State m_state = Loading;
    Theme m_theme = Theme::Dark;
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

    QPointer<LiveCaptionModel> m_caption;
    QTextDocument m_doc;
    qreal m_targetW = 120, m_targetH = 60;
    qreal m_cardW = 120, m_cardH = 60;
    qreal m_targetScroll = 0, m_scroll = 0;   // 超过两行时的垂直滚动，<= 0
    qreal m_capT = 0;                         // 0 = 胶囊，1 = 字幕卡片
};
