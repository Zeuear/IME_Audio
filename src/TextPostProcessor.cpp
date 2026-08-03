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

QString TextPostProcessor::pickSuffix(const QString& text) const {
    if (text.isEmpty()) {
        return QStringLiteral(".");
    }

    QString cleanText = text.trimmed();
    if (cleanText.isEmpty()) {
        return QStringLiteral(".");
    }

    bool hasNonAscii = false;
    for (const QChar& ch : cleanText) {
        if (ch.unicode() > 127) {
            hasNonAscii = true;
            break;
        }
    }

    // English question detection
    if (!hasNonAscii) {
        if (cleanText.endsWith("?")) return QStringLiteral("?");
        if (cleanText.endsWith("!")) return QStringLiteral("!");
        if (cleanText.endsWith(".")) return QStringLiteral(".");

        QString lower = cleanText.toLower();
        static const QStringList enQuestionKeywords = {
            "what", "how", "why", "is it", "is this",
            "are you", "can you", "could you", "would you",
            "will you", "should ", "right", " ok", "okay"
        };
        for (const QString& kw : enQuestionKeywords) {
            if (lower.contains(kw)) {
                return QStringLiteral("?");
            }
        }

        static const QStringList enExclaimKeywords = {
            "too good", "awesome", "great", "nice", "terrific"
        };
        for (const QString& kw : enExclaimKeywords) {
            if (lower.contains(kw)) {
                return QStringLiteral("!");
            }
        }

        return QStringLiteral(".");
    }

    // Chinese/CJK question suffixes: 吗 么 嘛 呢
    static const QChar cnQuestionSuffixes[] = {
        QChar(0x5480),  // 吗
        QChar(0x4E58),  // 么
        QChar(0x563B),  // 嘛
        QChar(0x54E4),  // 呢
    };
    for (QChar suf : cnQuestionSuffixes) {
        if (!cleanText.isEmpty() && cleanText.back() == suf) {
            return QStringLiteral("？");  // ？
        }
    }

    // Chinese/CJK question keywords:
    // 什么 怎么 为什么 是否 能不能 可不 要不要 有没有 哪 谁 行不
    static const QStringList cnQuestionKeywords = {
        QStringLiteral("什么"), QStringLiteral("怎么"), QStringLiteral("为什么"),
        QStringLiteral("是否"), QStringLiteral("是不是"),
        QStringLiteral("能不能"), QStringLiteral("可不"),
        QStringLiteral("要不要"), QStringLiteral("有没有"),
        QStringLiteral("行不"), QStringLiteral("哪儿"),
        QStringLiteral("哪里"), QStringLiteral("谁"),
        QStringLiteral("多少"), QStringLiteral("几点"),
        QStringLiteral("几号")
    };
    for (const QString& kw : cnQuestionKeywords) {
        if (cleanText.contains(kw)) {
            return QStringLiteral("？");  // ？
        }
    }

    // Chinese/CJK exclamation suffixes: 啊 呀 哇 啦
    static const QChar cnExclaimSuffixes[] = {
        QChar(0x5580),  // 啊
        QChar(0x565A),  // 呀
        QChar(0x54E7),  // 哇
        QChar(0x5562),  // 啦
    };
    for (QChar suf : cnExclaimSuffixes) {
        if (!cleanText.isEmpty() && cleanText.back() == suf) {
            return QStringLiteral("！");  // ！
        }
    }

    // Chinese/CJK exclamation keywords: 太好了 太棒了 真棒 好厉害 牛啊
    static const QStringList cnExclaimKeywords = {
        QStringLiteral("太好"), QStringLiteral("太棒"),
        QStringLiteral("真棒"), QStringLiteral("好厉害"),
        QStringLiteral("牛啊")
    };
    for (const QString& kw : cnExclaimKeywords) {
        if (cleanText.contains(kw)) {
            return QStringLiteral("！");  // ！
        }
    }

    return QStringLiteral("。");  // 。
}
