#include "TextPostProcessor.h"
#include "TermsLibraryManager.h"

TextPostProcessor::TextPostProcessor(QObject *parent) : QObject(parent) {}

void TextPostProcessor::setReplaceRules(const QString& rules) {
    m_replaceRules = rules;
}

void TextPostProcessor::setLanguage(const QString& lang) {
    m_language = lang;
}

QString TextPostProcessor::postProcess(const QString& rawText) {
    QString text = rawText;
    text = TermsLibraryManager::applyReplaceRules(text, m_replaceRules);
    text = addAutoPunctuation(text, m_language);
    return text.trimmed();
}

QString TextPostProcessor::addAutoPunctuation(const QString& text, const QString& language) {
    if (text.isEmpty()) {
        return QString();
    }

    QString finalText = text.trimmed();
    if (finalText.isEmpty() || endsWithSentencePunctuation(text)) {
        return text;
    }

    QString suffix = pickSuffix(text);
    finalText.append(suffix);
    return finalText;
}

bool TextPostProcessor::endsWithSentencePunctuation(const QString& text) const {
    QString cleanText = text.trimmed();
    if (cleanText.isEmpty()) {
        return false;
    }

    QChar lastChar = cleanText.at(cleanText.length() - 1);
    if (lastChar == '.' || lastChar == '!' || lastChar == '?' ||
        lastChar == ',' || lastChar == ';' || lastChar == ':') {
        return true;
    }

    if (cleanText.endsWith(QStringLiteral("。")) ||   // 。
        cleanText.endsWith(QStringLiteral("！")) ||   // ！
        cleanText.endsWith(QStringLiteral("？")) ||   // ？
        cleanText.endsWith(QStringLiteral("，")) ||   // ，
        cleanText.endsWith(QStringLiteral("；")) ||   // ；
        cleanText.endsWith(QStringLiteral("："))) {    // ：
        return true;
    }
    return false;
}


QString TextPostProcessor::pickSuffix(const QString& text) const{
    static const QStringList questionSuffixes = { "吗", "么", "嘛", "呢" };
    static const QStringList questionKeywords = {
        "什么", "怎么", "为什么", "为啥", "是否", "是不是", "能不能", "可不可以",
        "要不要", "有没有", "行不行", "哪儿", "哪里", "谁", "多久", "多少", "几点", "几号"
    };

    static const QStringList exclaimSuffixes = { "啊", "呀", "哇", "啦" };
    static const QStringList exclaimKeywords = { "太好了", "太棒了", "真棒", "好厉害", "牛啊" };
    if (text.isEmpty()) {
        return ".";
    }

    QString cleanText = text.trimmed();
    if (cleanText.isEmpty()) {
        return ".";
    }

    bool hasNonAscii = false;
    for (const QChar& ch : cleanText) {
        if (ch.unicode() > 127) {
            hasNonAscii = true;
            break;
        }
    }
    for (const QString& suffix : questionSuffixes) {
        if (cleanText.endsWith(suffix)) {
            return hasNonAscii ? "？" : "?";
        }
    }
    for (const QString& keyword : questionKeywords) {
        if (cleanText.contains(keyword)) {
            return hasNonAscii ? "？" : "?";
        }
    }
    for (const QString& suffix : exclaimSuffixes) {
        if (cleanText.endsWith(suffix)) {
            return hasNonAscii ? "！" : "!";
        }
    }
    for (const QString& keyword : exclaimKeywords) {
        if (cleanText.contains(keyword)) {
            return hasNonAscii ? "！" : "!";
        }
    }
    return hasNonAscii ? "。" : ".";
}
