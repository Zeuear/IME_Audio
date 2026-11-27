#include "OllamaModelButton.h"

#include <QDesktopServices>
#include <QEvent>
#include <QUrl>

#include "services/ollama/OllamaCatalog.h"
#include "services/ollama/OllamaClient.h"

namespace {
const QUrl kOllamaDownloadUrl(QStringLiteral("https://ollama.com/download"));
}

OllamaModelButton::OllamaModelButton(QWidget* parent) : QPushButton(parent)
{
    connect(this, &QPushButton::clicked, this, &OllamaModelButton::onClicked);
    hide();
}

void OllamaModelButton::setClient(OllamaClient* client)
{
    m_client = client;
    if (!m_client) return;

    connect(m_client, &OllamaClient::serviceChecked, this, [this](bool running, const QString&) {
        m_checked = true;
        m_running = running;
        refresh();
    });
    connect(m_client, &OllamaClient::pullProgress, this,
            [this](const QString& name, qint64 done, qint64 total, const QString&) {
        if (!OllamaClient::sameModel(name, m_model)) return;
        m_state = State::Pulling;
        if (total > 0 && done >= 0) m_percent = int(done * 100 / total);
        refresh();
    });
    connect(m_client, &OllamaClient::pullFinished, this,
            [this](const QString& name, bool ok, bool cancelled, const QString& error) {
        const bool own = m_ownPull && OllamaClient::sameModel(name, m_model);
        if (own) m_ownPull = false;
        if (!OllamaClient::sameModel(name, m_model)) return;
        m_percent = -1;
        if (ok) {
            m_state = State::Idle;
            if (!installed()) m_installed << name;
            if (own) emit modelInstalled(name);
        } else if (cancelled) {
            m_state = State::Idle;
        } else {
            m_state = State::Failed;
            m_error = error;
            if (own) emit errorOccurred(QStringLiteral("Ollama 模型下载失败"), error);
        }
        refresh();
    });
}

void OllamaModelButton::setActive(bool active)
{
    m_active = active;
    refresh();
}

void OllamaModelButton::setModel(const QString& name)
{
    if (OllamaClient::sameModel(name, m_model) && !name.isEmpty() == !m_model.isEmpty()) return;
    m_model = name;
    // 换了模型：状态跟着新模型走（它可能正由另一个按钮下载）
    m_state = (m_client && !name.isEmpty() && m_client->isPulling(name)) ? State::Pulling : State::Idle;
    m_percent = -1;
    m_error.clear();
    refresh();
}

void OllamaModelButton::setInstalled(const QStringList& models)
{
    m_installed = models;
    m_checked = true;
    m_running = true;
    refresh();
}

bool OllamaModelButton::installed() const
{
    for (const QString& m : m_installed) {
        if (OllamaClient::sameModel(m, m_model)) return true;
    }
    return false;
}

void OllamaModelButton::onClicked()
{
    if (!m_client) return;
    if (!m_running) {
        QDesktopServices::openUrl(kOllamaDownloadUrl);
        return;
    }
    if (m_state == State::Pulling) {
        m_client->cancelPull(m_model);
        return;
    }
    m_ownPull = true;
    m_state = State::Pulling;
    m_percent = -1;
    m_error.clear();
    refresh();
    m_client->pull(m_model);
}

void OllamaModelButton::refresh()
{
    if (!m_active || !m_checked) {
        hide();
        return;
    }
    if (!m_running) {
        setText(tr("Get Ollama"));
        setToolTip(tr("Ollama not detected. Install and start it, then check the connection again."));
        show();
        return;
    }
    if (m_model.isEmpty() || (installed() && m_state != State::Pulling)) {
        hide();
        return;
    }
    switch (m_state) {
    case State::Pulling:
        setText(m_percent >= 0 ? tr("Cancel %1%").arg(m_percent) : tr("Cancel"));
        setToolTip(tr("Downloading %1").arg(m_model));
        break;
    case State::Failed:
        setText(tr("Retry"));
        setToolTip(m_error);
        break;
    case State::Idle: {
        const QString size = ollamaCatalogSize(m_model);
        setText(tr("Download"));
        setToolTip(size.isEmpty() ? tr("Not downloaded yet") : tr("Not downloaded yet, about %1").arg(size));
        break;
    }
    }
    show();
}

void OllamaModelButton::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) refresh();
    QPushButton::changeEvent(event);
}
