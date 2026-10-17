#include "PermissionGuideDialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "utils/Logger.h"

namespace {

using Status = PlatformPermissions::Status;

constexpr int kPollMs = 1000;   // 授权在系统弹框 / 系统设置里异步完成，没有回调可等，只能轮询

bool isReady(Status s)
{
    return s == Status::Granted || s == Status::NotRequired;
}

}   // namespace

PermissionGuideDialog::PermissionGuideDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Permissions Setup"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setupUi();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollMs);
    connect(m_pollTimer, &QTimer::timeout, this, &PermissionGuideDialog::refresh);
    m_pollTimer->start();
    refresh();
}

bool PermissionGuideDialog::isNeeded()
{
    return !isReady(PlatformPermissions::microphoneStatus())
        || !isReady(PlatformPermissions::accessibilityStatus());
}

void PermissionGuideDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto* intro = new QLabel(tr("ImeAudio needs two permissions to record your voice and type the result "
                                "into other apps. Usually you only need to do this once."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(12);
    grid->setColumnStretch(0, 1);
    m_mic = addRow(grid, 0, tr("Microphone"), tr("Record your voice for transcription."));
    m_accessibility = addRow(grid, 2, tr("Accessibility"), tr("Type the transcribed text into the active window."));
    layout->addLayout(grid);

    // 签名变化（如旧版本是 ad-hoc 签名）后，系统设置里的勾还在但已失效，这是最常见的卡点
    m_accessibilityHint = new QLabel(tr("Already ticked but still shown as not granted? Select ImeAudio in the list, "
                                 "remove it with \"−\", add it again with \"+\", then restart ImeAudio."), this);
    m_accessibilityHint->setWordWrap(true);
    m_accessibilityHint->setForegroundRole(QPalette::PlaceholderText);
    grid->addWidget(m_accessibilityHint, 4, 0, 1, 3);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* laterBtn = new QPushButton(tr("Later"), this);
    m_restartBtn = new QPushButton(tr("Restart Now"), this);
    m_restartBtn->setDefault(true);
    buttons->addWidget(laterBtn);
    buttons->addWidget(m_restartBtn);
    layout->addLayout(buttons);

    connect(laterBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_restartBtn, &QPushButton::clicked, this, []() { PlatformPermissions::relaunchApp(); });
    connect(m_mic.button, &QPushButton::clicked, this, &PermissionGuideDialog::onMicrophoneClicked);
    connect(m_accessibility.button, &QPushButton::clicked, this, &PermissionGuideDialog::onAccessibilityClicked);

    setMinimumWidth(460);
}

PermissionGuideDialog::Row PermissionGuideDialog::addRow(QGridLayout* grid, int row,
                                                         const QString& title, const QString& description)
{
    auto* titleLabel = new QLabel(title, this);
    QFont bold = titleLabel->font();
    bold.setBold(true);
    titleLabel->setFont(bold);

    auto* descLabel = new QLabel(description, this);
    descLabel->setWordWrap(true);
    descLabel->setForegroundRole(QPalette::PlaceholderText);

    Row r;
    r.status = new QLabel(this);
    r.button = new QPushButton(this);
    // 授权后按钮隐藏，但仍占位，避免状态文字跟着左右跳
    QSizePolicy sp = r.button->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    r.button->setSizePolicy(sp);

    grid->addWidget(titleLabel, row, 0);
    grid->addWidget(r.status, row, 1, Qt::AlignRight);
    grid->addWidget(r.button, row, 2);
    grid->addWidget(descLabel, row + 1, 0, 1, 3);
    return r;
}

void PermissionGuideDialog::refresh()
{
    const Status mic = PlatformPermissions::microphoneStatus();
    const Status ax = PlatformPermissions::accessibilityStatus();

    setStatus(m_mic.status, mic);
    m_mic.button->setText(mic == Status::NotDetermined ? tr("Allow") : tr("Open Settings"));
    m_mic.button->setVisible(!isReady(mic));

    setStatus(m_accessibility.status, ax);
    m_accessibility.button->setText(tr("Open Settings"));
    m_accessibility.button->setVisible(!isReady(ax));
    m_accessibilityHint->setVisible(!isReady(ax));

    const bool allReady = isReady(mic) && isReady(ax);
    m_restartBtn->setEnabled(allReady);
    m_summaryLabel->setText(allReady
        ? tr("All set. Restart ImeAudio to apply the permissions.")
        : tr("Grant the permissions above. This window updates automatically."));
}

void PermissionGuideDialog::setStatus(QLabel* label, Status status)
{
    // 成功 / 失败用固定语义色，其余跟随主题
    switch (status) {
    case Status::Granted:
    case Status::NotRequired:
        label->setText(tr("Granted"));
        label->setStyleSheet(QStringLiteral("color: #2e9d47;"));
        break;
    case Status::NotDetermined:
        label->setText(tr("Not requested"));
        label->setStyleSheet(QString());
        break;
    case Status::Denied:
        label->setText(tr("Not granted"));
        label->setStyleSheet(QStringLiteral("color: #d64545;"));
        break;
    }
}

void PermissionGuideDialog::onMicrophoneClicked()
{
    if (PlatformPermissions::microphoneStatus() != Status::NotDetermined) {
        // 拒绝过之后系统不会再弹框，只能去系统设置里打开
        PlatformPermissions::openMicrophoneSettings();
        return;
    }
    // 回调可能在任意线程，切回界面线程刷新；对话框可能已关闭，用 QPointer 守护
    QPointer<PermissionGuideDialog> self(this);
    PlatformPermissions::requestMicrophoneAccess([self](bool granted) {
        LOG_INFO(QString("PermissionGuide: microphone request answered, granted=%1").arg(granted));
        if (self) QMetaObject::invokeMethod(self, &PermissionGuideDialog::refresh, Qt::QueuedConnection);
    });
}

void PermissionGuideDialog::onAccessibilityClicked()
{
    // 第一次走系统提示：它会把本应用加入辅助功能列表，用户只需打勾；之后直接打开设置面板
    if (!m_accessibilityPrompted) {
        m_accessibilityPrompted = true;
        PlatformPermissions::requestAccessibility(true);
        return;
    }
    PlatformPermissions::openAccessibilitySettings();
}
