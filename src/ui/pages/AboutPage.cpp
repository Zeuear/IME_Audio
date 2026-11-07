#include "AboutPage.h"
#include "app/version.h"
#include "services/UpdateManager.h"
#include "ui/models/UpdateStatusModel.h"
#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

AboutPage::AboutPage(QWidget* parent)
    : QWidget(parent), m_status(std::make_unique<UpdateStatusModel>())
{
    setupUi();
}

AboutPage::~AboutPage() = default;

bool AboutPage::isUserDriven() const
{
    return m_status->isUserDriven();
}

QString AboutPage::loadChangelog() const
{
    QFile file(":/CHANGELOG.md");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString::fromUtf8(file.readAll());
    return tr("暂无更新说明。");
}

void AboutPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(12);

    // 顶部：图标 + 名称 + 版本
    auto* header = new QHBoxLayout;
    m_iconLabel = new QLabel(this);
    const QPixmap icon(":/title_icon");
    if (!icon.isNull()) m_iconLabel->setPixmap(icon.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_iconLabel->setFixedSize(56, 56);
    header->addWidget(m_iconLabel);

    auto* titleCol = new QVBoxLayout;
    m_titleLabel = new QLabel(QApplication::applicationName().isEmpty() ? QStringLiteral("ImeAudio")
                                                                        : QApplication::applicationName(), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_versionLabel = new QLabel(this);
    m_versionLabel->setStyleSheet("color: gray;");
    titleCol->addWidget(m_titleLabel);
    titleCol->addWidget(m_versionLabel);
    header->addLayout(titleCol);
    header->addStretch();
    layout->addLayout(header);

    // 检查更新一行：按钮 + 状态文字 + 安装按钮；下面是下载进度条
    auto* updateRow = new QHBoxLayout;
    updateRow->setSpacing(10);
    m_checkBtn = new QPushButton(this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_installBtn = new QPushButton(this);
    m_aboutQtBtn = new QPushButton(this);
    updateRow->addWidget(m_checkBtn);
    updateRow->addWidget(m_statusLabel, 1);
    updateRow->addWidget(m_installBtn);
    updateRow->addWidget(m_aboutQtBtn);
    layout->addLayout(updateRow);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(true);
    layout->addWidget(m_progress);

    // 简介 / 更新说明
    m_tabs = new QTabWidget(this);
    m_descriptionText = new QTextEdit(this);
    m_descriptionText->setReadOnly(true);
    m_changelogText = new QTextEdit(this);
    m_changelogText->setReadOnly(true);
    m_changelogText->setMarkdown(loadChangelog());
    m_tabs->addTab(m_descriptionText, QString());
    m_tabs->addTab(m_changelogText, QString());
    layout->addWidget(m_tabs, 1);

    connect(m_checkBtn, &QPushButton::clicked, this, [this]() {
        if (!m_updates) return;
        m_status->startCheck();
        refresh();
        m_updates->checkForUpdates();
    });
    connect(m_installBtn, &QPushButton::clicked, this, [this]() {
        if (!m_updates) return;
        m_status->startDownload();
        refresh();
        m_updates->downloadUpdate(UpdateManager::defaultDownloadPath());
    });
    connect(m_aboutQtBtn, &QPushButton::clicked, this, []() { QMessageBox::aboutQt(nullptr, "About Qt"); });

    retranslateUi();
}

void AboutPage::setUpdateManager(UpdateManager* updates)
{
    m_updates = updates;
    connect(updates, &UpdateManager::updateAvailable, this,
            [this](const QString& version, const QString&, const QString&) { m_status->available(version); refresh(); });
    connect(updates, &UpdateManager::upToDate, this, [this](const QString& v) { m_status->upToDate(v); refresh(); });
    connect(updates, &UpdateManager::updateError, this, [this](const QString& message) { m_status->failed(message); refresh(); });
    connect(updates, &UpdateManager::downloadProgress, this, [this](qint64 r, qint64 t) { m_status->progress(r, t); refresh(); });
    connect(updates, &UpdateManager::downloadFinished, this, [this]() { m_status->downloadFinished(); refresh(); });
}

void AboutPage::refresh()
{
    const UpdateStatusModel::View v = m_status->view();
    m_statusLabel->setText(v.text);
    m_checkBtn->setEnabled(v.checkEnabled);
    m_installBtn->setVisible(v.installVisible);
    m_progress->setVisible(v.progressVisible);
    if (v.progressVisible) {
        if (v.percent < 0) m_progress->setRange(0, 0);   // 总大小未知：忙碌动画
        else { m_progress->setRange(0, 100); m_progress->setValue(v.percent); }
    }
}

void AboutPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void AboutPage::retranslateUi()
{
    m_versionLabel->setText(tr("Version %1").arg(QString::fromLatin1(PROJECT_VERSION)));
    m_checkBtn->setText(tr("Check for Updates"));
    m_installBtn->setText(tr("Download and Install"));
    m_aboutQtBtn->setText(tr("About Qt"));
    // 模型里的文字在状态变化时才生成；换语言后重新取一遍
    refresh();

    // 模型署名：Kroko 社区模型为 CC-BY-SA 许可，要求注明来源
    m_descriptionText->setText(
        tr("ImeAudio 是一款专注于输入法语音辅助的桌面工具，"
           "支持音频采集、语音转换与快捷键控制，帮助你更高效地完成日常输入操作。")
        + QStringLiteral("\n\n")
        + tr("语音识别基于 sherpa-onnx（k2-fsa，Apache-2.0）。"
             "英、法、德、西语实时字幕使用 Kroko ASR 社区模型（Banafo，https://kroko.ai，CC-BY-SA 许可）。")
        + QStringLiteral("\n\n")
        + tr("本地离线翻译使用 Firefox Translations 的 Bergamot 模型（Mozilla，MPL-2.0，https://github.com/mozilla/firefox-translations-models）。"));
    m_tabs->setTabText(0, tr("软件简介"));
    m_tabs->setTabText(1, tr("更新说明"));
}
