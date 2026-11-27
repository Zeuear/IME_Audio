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
QString subtitle(const DictionaryProfile& profile);

}
