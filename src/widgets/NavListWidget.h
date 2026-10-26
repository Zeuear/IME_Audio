#pragma once
#include <QWidget>
#include <QListWidget>
#include <QHash>

class QLabel;

class NavListItemWidget : public QWidget
{
    Q_OBJECT
public:
    enum class IndicatorState {
        None,        // 不显示指示灯
        Downloading, // 黄色:有下载任务进行中
        Completed    // 绿色:下载已完成
    };

    explicit NavListItemWidget(const QString& iconText, const QString& text, QWidget* parent = nullptr);

    void setIndicatorState(IndicatorState state);
    IndicatorState indicatorState() const { return m_state; }

    void setText(const QString& text);
    void setActive(bool active); 
    int preferredWidth() const;

private:
    QLabel* m_iconLabel = nullptr;
    QLabel* m_textLabel = nullptr;
    QLabel* m_indicatorDot = nullptr;
    IndicatorState m_state = IndicatorState::None;
};


class NavListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit NavListWidget(QWidget* parent = nullptr);

    void addNavItem(const QString& id, const QString& iconText, const QString& text);
    void setIndicator(const QString& id, NavListItemWidget::IndicatorState state);
    void setNavText(const QString& id, const QString& text);
    void selectItem(const QString& id);

    QString currentItemId() const;

signals:
    void navItemSelected(const QString& id);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onCurrentRowChanged(int row);

private:
    void refreshItemSizes();

    QHash<QString, QListWidgetItem*> m_itemsById;
    QHash<QListWidgetItem*, QString> m_idsByItem;
};