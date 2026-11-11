#include "LinkInputDialog.h"
#include "services/media/MediaLink.h"
#include <QDialogButtonBox>
#include <QEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

LinkInputDialog::LinkInputDialog(QWidget* parent)
    : QDialog(parent)
{
    resize(560, 360);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_edit = new QPlainTextEdit(this);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_summary = new QLabel(this);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    layout->addWidget(m_hint);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_summary);
    layout->addWidget(m_buttons);

    connect(m_edit, &QPlainTextEdit::textChanged, this, &LinkInputDialog::refresh);
    connect(m_buttons, &QDialogButtonBox::accepted, this, [this]() {
        m_invalid.clear();
        m_links = MediaLink::parseLines(m_edit->toPlainText(), &m_invalid);
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    retranslateUi();
    refresh();
}

void LinkInputDialog::refresh()
{
    QStringList invalid;
    const QStringList ok = MediaLink::parseLines(m_edit->toPlainText(), &invalid);
    if (m_edit->toPlainText().trimmed().isEmpty())
        m_summary->clear();
    else if (invalid.isEmpty())
        m_summary->setText(tr("%1 valid links").arg(ok.size()));
    else
        m_summary->setText(tr("%1 valid links, %2 lines will be ignored").arg(ok.size()).arg(invalid.size()));
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!ok.isEmpty());
}

void LinkInputDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QDialog::changeEvent(event);
}

void LinkInputDialog::retranslateUi()
{
    setWindowTitle(tr("Add Links"));
    m_hint->setText(tr("Paste public Facebook or YouTube video links, one per line. The audio is downloaded and transcribed automatically."));
    m_edit->setPlaceholderText(tr("https://www.youtube.com/watch?v=..."));
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Add"));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
}
