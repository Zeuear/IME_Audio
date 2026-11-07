#pragma once
#include <QWidget>

class LiveTranscriptWidget;

// @brief 转录内容独立窗口：装着"实时转录"稿子控件，可拖到副屏，关闭只隐藏、内容保留。
//        稿子控件只有这一份（主窗口里不再有），所以数据和保存文件天然只有一份。
//        位置和大小记在配置文件里，下次打开回到原处。
class TranscriptWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TranscriptWindow(QWidget* parent = nullptr);
    ~TranscriptWindow() override;

    LiveTranscriptWidget* transcript() const { return m_transcript; }

    // 打开并带到前台；已经开着就只带到前台
    void openAndRaise();

signals:
    // 窗口真正被显示或隐藏（最小化不算），横条据此点亮或复位图标
    void visibilityChanged(bool visible);

public slots:
    void setVisible(bool visible) override;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void saveGeometry();

    LiveTranscriptWidget* m_transcript = nullptr;
    bool m_restored = false;
};
