#include "LanguageCatalog.h"

#include <QCoreApplication>
#include <QList>

namespace {
struct Info {
    const char* key;    // 英文名，稳定标识
    const char* code;   // ISO 639 代码，仅用于显示
};

const QList<Info> kLanguages = {
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Chinese"), "zh"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "English"), "en"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Cantonese"), "yue"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Japanese"), "ja"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Korean"), "ko"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "French"), "fr"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "German"), "de"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Spanish"), "es"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Portuguese"), "pt"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Russian"), "ru"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Italian"), "it"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Arabic"), "ar"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Hindi"), "hi"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Thai"), "th"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Vietnamese"), "vi"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Indonesian"), "id"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Turkish"), "tr"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Polish"), "pl"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Dutch"), "nl"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Ukrainian"), "uk"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Hebrew"), "he"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Georgian"), "ka"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Armenian"), "hy"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Tibetan"), "bo"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Tagalog"), "tl"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Afrikaans"), "af"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Amharic"), "am"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Assamese"), "as"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Azerbaijani"), "az"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Bashkir"), "ba"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Belarusian"), "be"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Bulgarian"), "bg"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Bengali"), "bn"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Breton"), "br"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Bosnian"), "bs"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Catalan"), "ca"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Czech"), "cs"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Welsh"), "cy"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Danish"), "da"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Greek"), "el"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Estonian"), "et"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Basque"), "eu"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Persian"), "fa"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Finnish"), "fi"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Faroese"), "fo"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Galician"), "gl"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Gujarati"), "gu"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Hausa"), "ha"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Hawaiian"), "haw"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Croatian"), "hr"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Haitian Creole"), "ht"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Hungarian"), "hu"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Icelandic"), "is"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Javanese"), "jv"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Kazakh"), "kk"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Khmer"), "km"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Kannada"), "kn"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Latin"), "la"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Luxembourgish"), "lb"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Lingala"), "ln"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Lao"), "lo"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Lithuanian"), "lt"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Latvian"), "lv"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Malagasy"), "mg"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Maori"), "mi"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Macedonian"), "mk"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Malayalam"), "ml"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Mongolian"), "mn"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Marathi"), "mr"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Malay"), "ms"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Maltese"), "mt"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Burmese"), "my"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Nepali"), "ne"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Norwegian Nynorsk"), "nn"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Norwegian"), "no"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Occitan"), "oc"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Punjabi"), "pa"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Pashto"), "ps"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Romanian"), "ro"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Sanskrit"), "sa"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Sindhi"), "sd"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Sinhala"), "si"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Slovak"), "sk"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Slovenian"), "sl"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Shona"), "sn"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Somali"), "so"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Albanian"), "sq"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Serbian"), "sr"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Sundanese"), "su"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Swedish"), "sv"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Swahili"), "sw"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Tamil"), "ta"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Telugu"), "te"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Tajik"), "tg"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Turkmen"), "tk"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Tatar"), "tt"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Urdu"), "ur"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Uzbek"), "uz"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Yiddish"), "yi"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Yoruba"), "yo"},
    {QT_TRANSLATE_NOOP("LanguageCatalog", "Irish"), "ga"},
};
}

QString LanguageCatalog::codeOf(const QString& key)
{
    for (const Info& i : kLanguages) {
        if (key == QLatin1String(i.key)) return QString::fromLatin1(i.code);
    }
    return {};
}

QString LanguageCatalog::plainName(const QString& key)
{
    const QByteArray utf8 = key.toUtf8();
    return QCoreApplication::translate("LanguageCatalog", utf8.constData());
}

QString LanguageCatalog::displayName(const QString& key)
{
    const QString code = codeOf(key);
    if (code.isEmpty()) return key;
    return QStringLiteral("%1 (%2)").arg(plainName(key), code);
}
