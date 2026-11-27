#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "services/TermsLibraryManager.h"

enum class DictionaryKind { Recognition, Translation };

struct DictionaryProfile {
    QString id;
    QString name;
    DictionaryKind kind = DictionaryKind::Recognition;
    QString langA;
    QString langB;
    bool enabled = true;
    QString file;
};

// @brief 词典配置索引：每个配置一个 TSV，索引记录名称、类型、绑定语言与启用状态；旧 terms.tsv 原地登记为「默认」配置。
class DictionaryProfiles : public QObject {
    Q_OBJECT
public:
    DictionaryProfiles(const QString& dir, const QString& legacyTermsPath, QObject* parent = nullptr);

    static QString defaultId();
    static TermsLibraryManager::TermsRuleSet mergeRuleSets(const QList<TermsLibraryManager::TermsRuleSet>& sets);

    QList<DictionaryProfile> profiles(DictionaryKind kind) const;
    const DictionaryProfile* find(const QString& id) const;

    QString addProfile(DictionaryKind kind, const QString& name, const QString& langA, const QString& langB);
    bool renameProfile(const QString& id, const QString& name);
    bool setEnabled(const QString& id, bool enabled);
    bool setLanguages(const QString& id, const QString& langA, const QString& langB);
    bool removeProfile(const QString& id);

    TermsLibraryManager* library(const QString& id);

    QList<DictionaryProfile> applicableRecognition(const QString& languageCode) const;
    TermsLibraryManager::TermsRuleSet buildRecognitionRules(const QString& languageCode);
    QList<QPair<QString, QString>> translationGlossary(const QString& sourceCode, const QString& targetCode);

signals:
    void profilesChanged();
    void rulesChanged();

private:
    QString indexPath() const;
    QString filePathOf(const DictionaryProfile& profile) const;
    void load();
    bool save() const;
    void ensureDefault();
    DictionaryProfile* findMutable(const QString& id);
    QString nextId() const;

    QString m_dir;
    QString m_legacyPath;
    QList<DictionaryProfile> m_profiles;
    QHash<QString, TermsLibraryManager*> m_libraries;
};
