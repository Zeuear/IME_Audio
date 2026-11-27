#include "DictionaryProfileDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "services/dictionary/DictionaryLanguages.h"
#include "services/dictionary/LanguageScript.h"

DictionaryProfileDialog::DictionaryProfileDialog(DictionaryKind kind, bool editing, QWidget* parent)
    : QDialog(parent), m_kind(kind) {
    const bool translation = kind == DictionaryKind::Translation;
    setWindowTitle(editing ? tr("Edit Dictionary") : (translation ? tr("New Translation Dictionary") : tr("New Recognition Dictionary")));
    setMinimumWidth(380);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Name"));
    m_langA = new QComboBox(this);
    fillLanguages(m_langA, !translation);

    auto* form = new QFormLayout;
    form->addRow(tr("Name"), m_nameEdit);
    if (translation) {
        m_langB = new QComboBox(this);
        fillLanguages(m_langB, false);
        if (m_langB->count() > 1) m_langB->setCurrentIndex(1);
        auto* row = new QHBoxLayout;
        auto* arrow = new QLabel(QStringLiteral("↔"), this);
        arrow->setAlignment(Qt::AlignCenter);
        row->addWidget(m_langA, 1);
        row->addWidget(arrow);
        row->addWidget(m_langB, 1);
        form->addRow(tr("Languages"), row);
    } else {
        form->addRow(tr("Language"), m_langA);
    }

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &DictionaryProfileDialog::updateOk);
    connect(m_langA, &QComboBox::currentIndexChanged, this, &DictionaryProfileDialog::updateOk);
    if (m_langB) connect(m_langB, &QComboBox::currentIndexChanged, this, &DictionaryProfileDialog::updateOk);
    updateOk();
}

void DictionaryProfileDialog::fillLanguages(QComboBox* combo, bool withAny) {
    if (withAny) combo->addItem(tr("Any language"), QString());
    for (const auto& choice : DictionaryLanguages::choices()) combo->addItem(choice.label, choice.code);
}

void DictionaryProfileDialog::setValues(const QString& name, const QString& langA, const QString& langB) {
    m_nameEdit->setText(name);
    const int a = m_langA->findData(LanguageScript::family(langA));
    if (a >= 0) m_langA->setCurrentIndex(a);
    if (m_langB) {
        const int b = m_langB->findData(LanguageScript::family(langB));
        if (b >= 0) m_langB->setCurrentIndex(b);
    }
}

QString DictionaryProfileDialog::name() const {
    return m_nameEdit->text().trimmed();
}

QString DictionaryProfileDialog::langA() const {
    return m_langA->currentData().toString();
}

QString DictionaryProfileDialog::langB() const {
    return m_langB ? m_langB->currentData().toString() : QString();
}

void DictionaryProfileDialog::updateOk() {
    bool ok = !name().isEmpty();
    if (m_kind == DictionaryKind::Translation) ok = ok && !langA().isEmpty() && langA() != langB();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}
