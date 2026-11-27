#pragma once
#include <QPushButton>
#include <QStringList>

class OllamaClient;

// @brief 模型下拉框旁的 Ollama 下载按钮（下载 / 进度 / 重试 / 下载 Ollama），只负责下载不保存设置；依赖由 MainWin 经 setClient 注入。
class OllamaModelButton : public QPushButton
{
    Q_OBJECT
public:
    explicit OllamaModelButton(QWidget* parent = nullptr);

    void setClient(OllamaClient* client);
    void setActive(bool active);
    void setModel(const QString& name);
    void setInstalled(const QStringList& models);

signals:
    void modelInstalled(const QString& name);
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
