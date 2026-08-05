#include "TextPolishPrompts.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace TextPolishPrompts {

namespace {
void ensurePromptsDirInited() {
    QString dir = QCoreApplication::applicationDirPath() + "/prompts";
    QDir().mkpath(dir);
}
} // namespace

QString loadSystemPrompt(const QString& filename, const QString& defaultPrompt) {
    ensurePromptsDirInited();
    QString path = QCoreApplication::applicationDirPath() + "/prompts/" + filename;

    QFile file(path);
    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(defaultPrompt.toUtf8());
            file.close();
        }
        return defaultPrompt;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return defaultPrompt;
    }

    QByteArray raw = file.readAll();
    file.close();

    if (raw.isEmpty()) {
        return defaultPrompt;
    }
    if (raw.size() >= 3 &&
        (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        raw = raw.mid(3);
    }

    QString content = QString::fromUtf8(raw);
    return content.isEmpty() ? defaultPrompt : content;
}

QString styleInstruction(const QString& style) {
    if (style == QStringLiteral("商務正式")) {
        return loadSystemPrompt("prompt_style_business.txt",
            "请将文本改写得更加商务、正式、专业，适合职场与商务邮件沟通。");
    }
    else if (style == QStringLiteral("日常口語")) {
        return loadSystemPrompt("prompt_style_casual.txt",
            "请保持日常口语风格，使语句流畅自然，不要过于死板。");
    }
    else if (style == QStringLiteral("簡潔扼要")) {
        return loadSystemPrompt("prompt_style_concise.txt",
            "请尽可能简洁明了，去掉赘字，保留核心重点。");
    }
    return loadSystemPrompt("prompt_style_default.txt", "请优化语句，使其流畅、重点清晰。");
}

QStringList buildFewshotMessages(const QString& fewshotText) {
    QStringList result;
    if (fewshotText.isEmpty()) return result;

    const auto lines = fewshotText.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        QString role, content;

        if (line.startsWith("user:", Qt::CaseInsensitive)) {
            role = "user";
            content = line.mid(5).trimmed();
        }
        else if (line.startsWith(QStringLiteral("user："))) { // 全角冒号 "："
            role = "user";
            content = line.mid(QStringLiteral("user：").length()).trimmed();
        }
        else if (line.startsWith("assistant:", Qt::CaseInsensitive)) {
            role = "assistant";
            content = line.mid(10).trimmed();
        }
        else if (line.startsWith(QStringLiteral("assistant："))) {
            role = "assistant";
            content = line.mid(QStringLiteral("assistant：").length()).trimmed();
        }
        else {
            continue;
        }

        if (!content.isEmpty()) {
            result << role << content;
        }
    }
    return result;
}

PolishInstructionParts buildPolishInstructionParts(const TextPolishService::RequestParams& params) {
    PolishInstructionParts p;
    p.style = styleInstruction(params.style);

    if (params.targetLang != QStringLiteral("不翻译") && !params.targetLang.isEmpty()) {
        p.langInstruction = QStringLiteral("并将最终结果翻译为「%1」（如果是中文请用对应的繁简体）。").arg(params.targetLang);
    }

    if (!params.replaceRules.isEmpty()) {
        p.dictInstruction = QStringLiteral("\n【专有名词与字词拼写参考（若语音中听起来相近，请优先使用以下写法）】:\n%1\n").arg(params.replaceRules);
    }

    if (!params.aiVocab.isEmpty()) {
        p.vocabInstruction = QStringLiteral("\n【专有名词与自订词库（若原始文字中听起来相近，请优先使用且纠正为此处的写法）】:\n%1\n").arg(params.aiVocab);
    }

    if (!params.customPrompt.isEmpty() && params.style == QStringLiteral("自訂 Prompt")) {
        p.customInstruction = QStringLiteral("\n【自订额外要求】:\n%1\n").arg(params.customPrompt);
    }

    return p;
}

QString autocompleteApiUrl(const QString& input) {
    if (input.isEmpty()) return input;
    QString url = input;
    if (!url.endsWith('/')) url += '/';
    const QString suffix = "v1/chat/completions";
    if (!url.endsWith(suffix)) {
        url += suffix;
    }
    return url;
}

} // namespace TextPolishPrompts
