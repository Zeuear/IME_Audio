#include "DocsSetupDialog.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "services/docs/DocsBridge.h"
#include "services/docs/DocsExtension.h"

namespace {
const QString kOkStyle = QStringLiteral("color: #2e9d47;");
const QString kWarnStyle = QStringLiteral("color: #d64545;");
constexpr int kCopiedFeedbackMs = 1500;
}

DocsSetupDialog::DocsSetupDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Google Docs"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setupUi();
}

void DocsSetupDialog::setBridge(DocsBridge* bridge)
{
    m_bridge = bridge;
    connect(bridge, &DocsBridge::linkChanged, this, &DocsSetupDialog::refresh);
    connect(bridge, &DocsBridge::bindFailed, this, &DocsSetupDialog::showBindError);
    refresh();
}

void DocsSetupDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    m_installStatus = addHeader(layout, tr("1. Install the Chrome extension (once)"));
    m_installSteps = new QWidget(this);
    auto* steps = new QVBoxLayout(m_installSteps);
    steps->setContentsMargins(12, 0, 0, 0);
    auto addStep = [&](const QString& text, QPushButton* button) {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(text, m_installSteps);
        label->setWordWrap(true);
        row->addWidget(label, 1);
        if (button) row->addWidget(button);
        steps->addLayout(row);
    };
    addStep(tr("Open chrome://extensions in Chrome's address bar"),
            addCopyButton(tr("Copy Address"), []() { return QStringLiteral("chrome://extensions"); }));
    addStep(tr("Turn on \"Developer mode\" at the top right"), nullptr);
    addStep(tr("Click \"Load unpacked\" and paste the folder path"),
            addCopyButton(tr("Copy Path"), []() { return QDir::toNativeSeparators(DocsExtension::installDir()); }));
    layout->addWidget(m_installSteps);

    layout->addSpacing(8);
    m_bindStatus = addHeader(layout, tr("2. Choose the document to write into"));
    auto* bindRow = new QHBoxLayout;
    bindRow->setContentsMargins(12, 0, 0, 0);
    m_newDocBtn = new QPushButton(tr("New Document"), this);
    m_bindActiveBtn = new QPushButton(tr("Use Current Chrome Tab"), this);
    bindRow->addWidget(m_newDocBtn);
    bindRow->addWidget(m_bindActiveBtn);
    bindRow->addStretch();
    layout->addLayout(bindRow);

    m_bindHint = new QLabel(this);
    m_bindHint->setWordWrap(true);
    m_bindHint->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_bindHint);

    layout->addStretch();
    auto* buttons = new QHBoxLayout;
    m_stopBtn = new QPushButton(tr("Stop Output"), this);
    auto* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    buttons->addWidget(m_stopBtn);
    buttons->addStretch();
    buttons->addWidget(closeBtn);
    layout->addLayout(buttons);

    connect(m_newDocBtn, &QPushButton::clicked, this, [this]() {
        m_bindHint->clear();
        if (m_bridge) m_bridge->requestNewDoc();
    });
    connect(m_bindActiveBtn, &QPushButton::clicked, this, [this]() {
        m_bindHint->clear();
        if (m_bridge) m_bridge->requestBindActive();
    });
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() { if (m_bridge) m_bridge->release(); });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    setMinimumWidth(460);
}

QLabel* DocsSetupDialog::addHeader(QVBoxLayout* layout, const QString& title)
{
    auto* row = new QHBoxLayout;
    auto* titleLabel = new QLabel(title, this);
    QFont bold = titleLabel->font();
    bold.setBold(true);
    titleLabel->setFont(bold);
    auto* status = new QLabel(this);
    row->addWidget(titleLabel, 1);
    row->addWidget(status);
    layout->addLayout(row);
    return status;
}

QPushButton* DocsSetupDialog::addCopyButton(const QString& text, const std::function<QString()>& value)
{
    auto* button = new QPushButton(text, this);
    connect(button, &QPushButton::clicked, this, [button, text, value]() {
        QGuiApplication::clipboard()->setText(value());
        button->setText(tr("Copied"));
        QTimer::singleShot(kCopiedFeedbackMs, button, [button, text]() { button->setText(text); });
    });
    return button;
}

void DocsSetupDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    m_bindHint->clear();
    prepareExtension();
    refresh();
}

// 每次打开都导出一遍，程序升级后扩展文件跟着更新；令牌沿用已导出的，避免已装好的扩展失效
void DocsSetupDialog::prepareExtension()
{
    const QString dir = DocsExtension::installDir();
    QString token = DocsExtension::readToken(dir);
    const bool newToken = !DocsBridge::isCurrentToken(token);
    if (newToken) token = DocsBridge::newToken();
    QString error;
    if (!DocsExtension::exportTo(dir, token, &error)) {
        emit errorOccurred(QStringLiteral("扩展导出失败"), error);
        return;
    }
    if (newToken) DocsBridge::saveToken(token);
    if (m_bridge) m_bridge->setEnabled(true);
}

void DocsSetupDialog::refresh()
{
    if (!m_bridge) return;
    const bool connected = m_bridge->isConnected();
    m_installStatus->setText(connected ? tr("✓ Installed") : tr("Waiting for the extension…"));
    m_installStatus->setStyleSheet(connected ? kOkStyle : QString());
    m_installSteps->setVisible(!connected);

    if (m_bridge->isBound()) {
        const QString title = m_bridge->boundTitle();
        m_bindStatus->setText(title.isEmpty() ? tr("✓ Bound") : tr("✓ Bound: %1").arg(title));
        m_bindStatus->setStyleSheet(kOkStyle);
    } else if (m_bridge->isLost()) {
        m_bindStatus->setText(tr("Disconnected"));
        m_bindStatus->setStyleSheet(kWarnStyle);
        m_bindHint->setText(tr("Dictation is held and not typed into other apps. Bind a document again, "
                               "or click \"Stop Output\" to type into the active window."));
    } else {
        m_bindStatus->setText(tr("Not bound"));
        m_bindStatus->setStyleSheet(QString());
        m_bindHint->clear();
    }
    if (m_bridge->isBound()) m_bindHint->setText(tr("Dictation now goes into this document."));
    m_newDocBtn->setEnabled(connected);
    m_bindActiveBtn->setEnabled(connected);
    m_stopBtn->setEnabled(m_bridge->holdsOutput());
}

void DocsSetupDialog::showBindError(const QString& reason)
{
    m_bindHint->setText(reason == QLatin1String("notDocs")
                            ? tr("The current Chrome tab is not a Google Doc. Open the document in Chrome first.")
                            : tr("Could not bind: %1").arg(reason));
}
