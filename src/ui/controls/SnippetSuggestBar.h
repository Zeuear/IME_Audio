#ifndef SNIPPETSUGGESTBAR_H
#define SNIPPETSUGGESTBAR_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QTimer>
#include <QWidget>

class QHBoxLayout;

// @brief 悬浮窗旁的指令条：听写里出现话术快捷码时，列出可点击的条目。
//        置顶且不抢焦点，点击后焦点仍在目标应用里，粘贴才会落到用户正在输入的位置。
class SnippetSuggestBar : public QWidget {
    Q_OBJECT

public:
    struct Suggestion {
        QString id;
        QString label;
        QString tooltip;
    };

    explicit SnippetSuggestBar(QWidget *parent = nullptr);

    // 建议的条目没变时不重建按钮，也不重置自动隐藏计时
    void setSuggestions(const QVector<Suggestion>& suggestions);
    void placeNear(const QRect& anchor, bool below);
    void dismiss();

signals:
    void snippetClicked(const QString& id);
    // 点击或超时收起时带出这批条目的 id，调用方据此在同一句话里不再重复提示
    void closed(const QStringList& ids);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void closeAndRemember();

    QHBoxLayout* m_layout = nullptr;
    QStringList m_currentIds;
    QTimer m_hideTimer;
};

#endif // SNIPPETSUGGESTBAR_H
