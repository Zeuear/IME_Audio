#pragma once
#include <QPushButton>
#include <QStringList>

class OllamaClient;

// @brief 模型下拉框旁的 Ollama 下载按钮：所选模型没装时显示「下载」，下载中显示进度、点一下取消，
// 失败显示「重试」；没检测到 Ollama 时变成「下载 Ollama」打开官网。模型已装或不是 Ollama 引擎时隐藏。
// 只负责下载，不保存设置：选哪个模型仍由下拉框决定。依赖由 MainWin 通过 setClient 注入。
class OllamaModelButton : public QPushButton
{
    Q_OBJECT
public:
    explicit OllamaModelButton(QWidget* parent = nullptr);

    void setClient(OllamaClient* client);
    // 是否处于 Ollama 引擎下；否则始终隐藏
    void setActive(bool active);
    // 下拉框当前选中的模型；空串（如"跟随润色模型"）时隐藏
    void setModel(const QString& name);
    void setInstalled(const QStringList& models);

signals:
    // 本按钮发起的下载完成；宿主据此刷新模型列表
    void modelInstalled(const QString& name);
    // title / cause 为中文，由宿主统一弹出
    void errorOccurred(const QString& title, const QString& cause);

protected:
    void changeEvent(QEvent* event) override;

private:
    enum class State { Idle, Pulling, Failed };

    void onClicked();
    void refresh();
    bool installed() const;

    OllamaClient* m_client = nullptr;
    bool m_active = false;
    bool m_checked = false;
    bool m_running = false;
    QString m_model;
    QStringList m_installed;
    State m_state = State::Idle;
    int m_percent = -1;
    QString m_error;
    bool m_ownPull = false;   // 两个按钮可能选中同一模型：只有发起下载的那个报错
};
