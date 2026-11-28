#pragma once
#include <QList>
#include <QString>

#include "DictionaryProfiles.h"

namespace DictionaryLanguages {

struct Choice {
    QString code;
    QString label;
};

QList<Choice> choices();
QString label(const QString& code);
// 把「中文」「保加利亚语」「Bulgarian」「bg」这类写法认成语言代码；认不出返回空
QString codeFromName(const QString& name);
QString subtitle(const DictionaryProfile& profile);

}
