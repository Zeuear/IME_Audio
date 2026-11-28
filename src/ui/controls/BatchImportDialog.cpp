#include "BatchImportDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

BatchImportDialog::BatchImportDialog(bool translation, const QString& langA, const QString& langB, QWidget* parent)
    : QDialog(parent), m_translation(translation), m_langA(langA), m_langB(langB) {
    setWindowTitle(tr("Batch Import"));
    setMinimumSize(600, 460);

    auto* hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    hint->setText(translation
        ? tr("One entry per line: source => translation. Also accepted: ->, =, tab, comma, \"source: translation\", "
             "several rules on one line (a=>b;c=>d), JSON such as {\"source\": \"translation\"}, and exported TSV. "
             "Separate several source words with commas or /.")
        : tr("One entry per line: heard as => write as. Also accepted: ->, =, tab, comma, several rules on one line "
             "(a=>b;c=>d), JSON, and exported TSV. A line with a single word just teaches the recognizer that word. "
             "Separate several \"heard as\" words with commas or /."));

    m_edit = new QPlainTextEdit(this);
    // 示例是数据而不是界面文字，不走翻译
    m_edit->setPlaceholderText(translation
        ? QStringLiteral("neural network => 神经网络\nGPU -> 显卡\nlatency = 延迟")
        : QStringLiteral("k8s, 开八斯 => Kubernetes\nGPT -> GPT-4\nAurapro"));
    m_edit->setAcceptDrops(false);

    auto* openBtn = new QPushButton(tr("Open File..."), this);
    m_swap = new QCheckBox(tr("Swap the two columns"), this);
    m_swap->setVisible(translation);
    auto* tools = new QHBoxLayout;
    tools->addWidget(openBtn);
    tools->addWidget(m_swap);
    tools->addStretch();

    m_summary = new QLabel(this);
    m_issues = new QPlainTextEdit(this);
    m_issues->setReadOnly(true);
    m_issues->setMaximumHeight(90);
    m_issues->hide();

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Import"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(m_edit, 1);
    layout->addLayout(tools);
    layout->addWidget(m_summary);
    layout->addWidget(m_issues);
    layout->addWidget(m_buttons);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(150);
    connect(m_timer, &QTimer::timeout, this, &BatchImportDialog::reparse);
    connect(m_edit, &QPlainTextEdit::textChanged, m_timer, qOverload<>(&QTimer::start));
    connect(openBtn, &QPushButton::clicked, this, &BatchImportDialog::openFile);
    connect(m_swap, &QCheckBox::clicked, this, [this]() { m_swapTouched = true; reparse(); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    reparse();
}

void BatchImportDialog::setText(const QString& text) {
    m_edit->setPlainText(text);
    m_timer->stop();
    reparse();
}

bool BatchImportDialog::swapColumns() const {
    return m_translation && m_swap->isChecked();
}

void BatchImportDialog::openFile() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
        tr("Dictionary files (*.json *.tsv *.csv *.txt);;All files (*)"));
    if (path.isEmpty()) return;
    QString error;
    const QString text = GlossaryImport::readTextFile(path, &error);
    if (!error.isEmpty()) {
        m_summary->setText(error);
        return;
    }
    // 文件原文放进编辑框，用户能看到并修改再导入
    setText(text);
}

void BatchImportDialog::reparse() {
    m_parsed = GlossaryImport::parseText(m_edit->toPlainText(), m_translation);

    if (m_translation && !m_swapTouched) {
        const QSignalBlocker blocker(m_swap);
        m_swap->setChecked(GlossaryImport::looksSwapped(m_parsed.entries, m_langA, m_langB));
    }

    const int n = m_parsed.entries.size();
    const int bad = m_parsed.issues.size();
    QString text = tr("%1 entries recognised").arg(n);
    if (bad > 0) text += tr(" · %1 lines skipped").arg(bad);
    if (m_translation && m_swap->isChecked()) text += tr(" · columns will be swapped");
    m_summary->setText(text);

    QStringList lines;
    for (const GlossaryImport::Issue& issue : m_parsed.issues.mid(0, 50)) {
        lines << (issue.line > 0 ? tr("Line %1: %2 (%3)").arg(issue.line).arg(issue.text.left(60), issue.reason)
                                 : tr("%1 (%2)").arg(issue.text.left(60), issue.reason));
    }
    if (bad > 50) lines << tr("... and %1 more").arg(bad - 50);
    m_issues->setPlainText(lines.join('\n'));
    m_issues->setVisible(bad > 0);

    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(n > 0);
}
