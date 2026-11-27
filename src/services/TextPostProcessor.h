#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include "services/dictionary/FuzzyReplacer.h"


class TextPostProcessor : public QObject {
    Q_OBJECT
public:
    explicit TextPostProcessor(QObject *parent = nullptr);

    void setReplaceRules(const QString& rules);
    void setFuzzyEntries(const QList<HotwordSelector::Entry>& entries) { m_fuzzy.setEntries(entries); }
    void setFuzzyEnabled(bool on) { m_fuzzyEnabled = on; }
    void setLanguage(const QString& lang);
    void setAutoPunctuation(bool on) { m_autoPunctuation = on; }

    QString postProcess(const QString& rawText);
    QString addAutoPunctuation(const QString& text, const QString& language);

    bool endsWithSentencePunctuation(const QString& text) const;
    QString pickSuffix(const QString& text, const QString& language) const;

private:
    QString m_replaceRules;
    FuzzyReplacer m_fuzzy;
    bool m_fuzzyEnabled = false;
    QString m_language;
    bool m_autoPunctuation = true;
};
