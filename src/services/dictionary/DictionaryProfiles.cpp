#include "DictionaryProfiles.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include "LanguageScript.h"
#include "utils/Logger.h"

namespace {

constexpr auto kLegacyFile = "@legacy";

QString kindName(DictionaryKind kind) {
    return kind == DictionaryKind::Translation ? QStringLiteral("translation") : QStringLiteral("recognition");
}

DictionaryKind kindFromName(const QString& name) {
    return name == QLatin1String("translation") ? DictionaryKind::Translation : DictionaryKind::Recognition;
}

}

DictionaryProfiles::DictionaryProfiles(const QString& dir, const QString& legacyTermsPath, QObject* parent)
    : QObject(parent), m_dir(dir), m_legacyPath(legacyTermsPath) {
    QDir().mkpath(m_dir);
    load();
    ensureDefault();
}

QString DictionaryProfiles::defaultId() {
    return QStringLiteral("default");
}

QString DictionaryProfiles::indexPath() const {
    return QDir(m_dir).filePath(QStringLiteral("index.json"));
}

QString DictionaryProfiles::filePathOf(const DictionaryProfile& profile) const {
    if (profile.file == QLatin1String(kLegacyFile)) return m_legacyPath;
    return QDir(m_dir).filePath(profile.file);
}

void DictionaryProfiles::load() {
    QFile file(indexPath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN(QStringLiteral("Dictionary index unreadable, starting from the default profile: %1").arg(error.errorString()));
        return;
    }
    for (const QJsonValue& value : doc.object().value(QStringLiteral("profiles")).toArray()) {
        const QJsonObject o = value.toObject();
        DictionaryProfile p;
        p.id = o.value(QStringLiteral("id")).toString();
        p.file = o.value(QStringLiteral("file")).toString();
        if (p.id.isEmpty() || p.file.isEmpty()) continue;
        p.name = o.value(QStringLiteral("name")).toString();
        p.kind = kindFromName(o.value(QStringLiteral("kind")).toString());
        p.langA = o.value(QStringLiteral("langA")).toString();
        p.langB = o.value(QStringLiteral("langB")).toString();
        p.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        m_profiles.append(p);
    }
}

bool DictionaryProfiles::save() const {
    QJsonArray array;
    for (const DictionaryProfile& p : m_profiles) {
        array.append(QJsonObject{
            { QStringLiteral("id"), p.id },
            { QStringLiteral("name"), p.name },
            { QStringLiteral("kind"), kindName(p.kind) },
            { QStringLiteral("langA"), p.langA },
            { QStringLiteral("langB"), p.langB },
            { QStringLiteral("enabled"), p.enabled },
            { QStringLiteral("file"), p.file },
        });
    }
    QSaveFile file(indexPath());
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(QJsonObject{ { QStringLiteral("version"), 1 }, { QStringLiteral("profiles"), array } })
                   .toJson(QJsonDocument::Indented));
    return file.commit();
}

void DictionaryProfiles::ensureDefault() {
    if (find(defaultId())) return;
    DictionaryProfile p;
    p.id = defaultId();
    p.name = tr("Default");
    p.kind = DictionaryKind::Recognition;
    p.file = QLatin1String(kLegacyFile);
    m_profiles.prepend(p);
    save();
}

DictionaryProfile* DictionaryProfiles::findMutable(const QString& id) {
    for (DictionaryProfile& p : m_profiles) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const DictionaryProfile* DictionaryProfiles::find(const QString& id) const {
    for (const DictionaryProfile& p : m_profiles) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

QList<DictionaryProfile> DictionaryProfiles::profiles(DictionaryKind kind) const {
    QList<DictionaryProfile> out;
    for (const DictionaryProfile& p : m_profiles) {
        if (p.kind == kind) out.append(p);
    }
    return out;
}

QString DictionaryProfiles::nextId() const {
    int highest = 0;
    for (const DictionaryProfile& p : m_profiles) {
        if (p.id.startsWith(QLatin1Char('p'))) highest = qMax(highest, p.id.mid(1).toInt());
    }
    return QStringLiteral("p%1").arg(highest + 1);
}

QString DictionaryProfiles::addProfile(DictionaryKind kind, const QString& name, const QString& langA, const QString& langB) {
    DictionaryProfile p;
    p.id = nextId();
    p.name = name.trimmed();
    p.kind = kind;
    p.langA = langA;
    p.langB = kind == DictionaryKind::Translation ? langB : QString();
    p.file = p.id + QStringLiteral(".tsv");
    m_profiles.append(p);
    save();
    emit profilesChanged();
    emit rulesChanged();
    return p.id;
}

bool DictionaryProfiles::renameProfile(const QString& id, const QString& name) {
    DictionaryProfile* p = findMutable(id);
    if (!p || name.trimmed().isEmpty()) return false;
    p->name = name.trimmed();
    save();
    emit profilesChanged();
    return true;
}

bool DictionaryProfiles::setEnabled(const QString& id, bool enabled) {
    DictionaryProfile* p = findMutable(id);
    if (!p) return false;
    if (p->enabled == enabled) return true;
    p->enabled = enabled;
    save();
    emit profilesChanged();
    emit rulesChanged();
    return true;
}

bool DictionaryProfiles::setLanguages(const QString& id, const QString& langA, const QString& langB) {
    DictionaryProfile* p = findMutable(id);
    if (!p) return false;
    p->langA = langA;
    p->langB = p->kind == DictionaryKind::Translation ? langB : QString();
    save();
    emit profilesChanged();
    emit rulesChanged();
    return true;
}

bool DictionaryProfiles::removeProfile(const QString& id) {
    if (id == defaultId()) return false;
    const DictionaryProfile* p = find(id);
    if (!p) return false;
    const QString path = filePathOf(*p);
    if (TermsLibraryManager* lib = m_libraries.take(id)) lib->deleteLater();
    QFile::remove(path);
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == id) {
            m_profiles.removeAt(i);
            break;
        }
    }
    save();
    emit profilesChanged();
    emit rulesChanged();
    return true;
}

TermsLibraryManager* DictionaryProfiles::library(const QString& id) {
    if (TermsLibraryManager* existing = m_libraries.value(id)) return existing;
    const DictionaryProfile* p = find(id);
    if (!p) return nullptr;
    auto* lib = new TermsLibraryManager(filePathOf(*p), this);
    connect(lib, &TermsLibraryManager::termsReloaded, this, &DictionaryProfiles::rulesChanged);
    m_libraries.insert(id, lib);
    return lib;
}

QList<DictionaryProfile> DictionaryProfiles::applicableRecognition(const QString& languageCode) const {
    const QString current = LanguageScript::family(languageCode);
    QList<DictionaryProfile> out;
    for (const DictionaryProfile& p : m_profiles) {
        if (p.kind != DictionaryKind::Recognition || !p.enabled) continue;
        if (p.langA.isEmpty() || LanguageScript::family(p.langA) == current) out.append(p);
    }
    return out;
}

TermsLibraryManager::TermsRuleSet DictionaryProfiles::buildRecognitionRules(const QString& languageCode) {
    QList<TermsLibraryManager::TermsRuleSet> sets;
    for (const DictionaryProfile& p : applicableRecognition(languageCode)) {
        if (TermsLibraryManager* lib = library(p.id)) sets.append(lib->buildAllRules());
    }
    return mergeRuleSets(sets);
}

QList<QPair<QString, QString>> DictionaryProfiles::translationGlossary(const QString& sourceCode, const QString& targetCode) {
    const QString source = LanguageScript::family(sourceCode);
    const QString target = LanguageScript::family(targetCode);
    QList<QPair<QString, QString>> pairs;
    QSet<QString> seen;
    for (const DictionaryProfile& p : m_profiles) {
        if (p.kind != DictionaryKind::Translation || !p.enabled) continue;
        const QString a = LanguageScript::family(p.langA);
        const QString b = LanguageScript::family(p.langB);
        const bool forward = a == source && b == target;
        const bool backward = b == source && a == target;
        if (!forward && !backward) continue;
        TermsLibraryManager* lib = library(p.id);
        if (!lib) continue;
        for (const TermItem& item : lib->items()) {
            if (!item.enabled || item.correct.isEmpty()) continue;
            for (const QString& heard : item.heard()) {
                const QString from = forward ? heard : item.correct;
                const QString to = forward ? item.correct : heard;
                if (from.isEmpty() || seen.contains(from)) continue;
                seen.insert(from);
                pairs.append({ from, to });
            }
        }
    }
    return pairs;
}

TermsLibraryManager::TermsRuleSet DictionaryProfiles::mergeRuleSets(const QList<TermsLibraryManager::TermsRuleSet>& sets) {
    TermsLibraryManager::TermsRuleSet merged;
    QStringList replace;
    QStringList vocab;
    QSet<QString> seenWrong;
    QSet<QString> seenHotword;
    QList<HotwordSelector::Entry> withHeard;
    QList<HotwordSelector::Entry> withoutHeard;

    for (const auto& set : sets) {
        for (const QString& rule : set.replaceRules.split(';', Qt::SkipEmptyParts)) {
            const int arrow = rule.indexOf(QStringLiteral("=>"));
            if (arrow <= 0) continue;
            if (seenWrong.contains(rule.left(arrow))) continue;
            seenWrong.insert(rule.left(arrow));
            replace << rule;
        }
        if (!set.aiVocab.isEmpty()) vocab << set.aiVocab;
        for (const auto& entry : set.hotwordEntries) {
            if (seenHotword.contains(entry.text)) continue;
            seenHotword.insert(entry.text);
            (entry.heard.isEmpty() ? withoutHeard : withHeard).append(entry);
        }
        merged.enabledCount += set.enabledCount;
        merged.replaceCount += set.replaceCount;
        merged.aiCount += set.aiCount;
    }

    merged.replaceRules = replace.join(';');
    merged.aiVocab = vocab.join(';');
    merged.hotwordEntries = withHeard + withoutHeard;
    for (const auto& entry : merged.hotwordEntries) merged.hotwords << entry.text;
    merged.hotwordCount = merged.hotwords.size();
    return merged;
}
