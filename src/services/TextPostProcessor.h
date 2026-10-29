#pragma once
#include <QObject>
#include <QString>
#include <QStringList>


class TextPostProcessor : public QObject {
    Q_OBJECT
public:
    explicit TextPostProcessor(QObject *parent = nullptr);

    void setReplaceRules(const QString& rules);
    void setLanguage(const QString& lang);  // "en", "zh", etc.
    void setAutoPunctuation(bool on) { m_autoPunctuation = on; }

    QString postProcess(const QString& rawText);
    QString addAutoPunctuation(const QString& text, const QString& language);

    bool endsWithSentencePunctuation(const QString& text) const;
    QString pickSuffix(const QString& text) const;

private:
    QString m_replaceRules;
    QString m_language;
    bool m_autoPunctuation = true;
};
