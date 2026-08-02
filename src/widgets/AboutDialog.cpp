#include "AboutDialog.h"
#include "version.h"

#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QFile>
#include <QPixmap>
#include <QIcon>
#include <QApplication>
#include <QFont>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("关于 ImeAudio"));
    setFixedSize(480, 420);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setupUi();
}

QString AboutDialog::loadChangelog() const
{
    // 优先尝试读取打包进资源文件的 CHANGELOG.md，找不到则给出默认文案
    QFile file(":/resources/CHANGELOG.md");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(file.readAll());
        file.close();
        return content;
    }
    return tr("暂无更新说明。");
}

void AboutDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ---- 顶部：图标 + 名称 + 版本 ----
    auto* headerLayout = new QHBoxLayout();

    m_iconLabel = new QLabel(this);
    QPixmap iconPixmap(":/title_icon");
    if (!iconPixmap.isNull()) {
        m_iconLabel->setPixmap(iconPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    m_iconLabel->setFixedSize(64, 64);
    headerLayout->addWidget(m_iconLabel);

    auto* titleLayout = new QVBoxLayout();
    m_titleLabel = new QLabel(QApplication::applicationName().isEmpty()
                                   ? tr("ImeAudio")
                                   : QApplication::applicationName(), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_versionLabel = new QLabel(
        tr("版本 %1").arg(QString::fromLatin1(PROJECT_VERSION)), this);
    m_versionLabel->setStyleSheet("color: gray;");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_versionLabel);
    titleLayout->addStretch();

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // ---- 中部：Tab 切换 简介 / 更新说明 ----
    auto* tabWidget = new QTabWidget(this);

    m_descriptionText = new QTextEdit(this);
    m_descriptionText->setReadOnly(true);
    m_descriptionText->setText(
        tr("ImeAudio 是一款专注于输入法语音辅助的桌面工具，"
           "支持音频采集、语音转换与快捷键控制，帮助你更高效地完成日常输入操作。"));
    tabWidget->addTab(m_descriptionText, tr("软件简介"));

    m_changelogText = new QTextEdit(this);
    m_changelogText->setReadOnly(true);
    m_changelogText->setMarkdown(loadChangelog());
    tabWidget->addTab(m_changelogText, tr("更新说明"));

    mainLayout->addWidget(tabWidget, 1);

    // ---- 底部：关闭按钮 ----
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_closeButton = new QPushButton(tr("关闭"), this);
    m_closeButton->setFixedWidth(80);
    connect(m_closeButton, &QPushButton::clicked, this, &AboutDialog::accept);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);
}