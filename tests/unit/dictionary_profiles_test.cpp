#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "services/dictionary/DictionaryProfiles.h"

namespace {

struct Fixture {
    QTemporaryDir tmp;
    QString dir() const { return tmp.filePath(QStringLiteral("dictionaries")); }
    QString legacy() const { return tmp.filePath(QStringLiteral("terms.tsv")); }

    void writeLegacy(const QByteArray& content) const {
        QFile f(legacy());
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(content);
    }
    QByteArray readLegacy() const {
        QFile f(legacy());
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }
};

TermItem item(const QString& heard, const QString& correct) {
    TermItem t;
    t.setHeard({ heard });
    t.correct = correct;
    return t;
}

}

TEST(DictionaryProfilesTest, FreshInstallRegistersTheLegacyFileAsTheDefaultProfile) {
    Fixture f;
    f.writeLegacy("1\t科达\tKuda\t\tall\t\t\n");
    const QByteArray before = f.readLegacy();

    DictionaryProfiles profiles(f.dir(), f.legacy());
    const auto list = profiles.profiles(DictionaryKind::Recognition);

    ASSERT_EQ(list.size(), 1);
    EXPECT_EQ(list[0].id, DictionaryProfiles::defaultId());
    EXPECT_TRUE(list[0].langA.isEmpty());
    EXPECT_TRUE(list[0].enabled);
    EXPECT_TRUE(QFile::exists(QDir(f.dir()).filePath(QStringLiteral("index.json"))));
    EXPECT_EQ(f.readLegacy(), before);
    EXPECT_TRUE(profiles.profiles(DictionaryKind::Translation).isEmpty());
}

TEST(DictionaryProfilesTest, IndexSurvivesARestart) {
    Fixture f;
    QString id;
    {
        DictionaryProfiles profiles(f.dir(), f.legacy());
        id = profiles.addProfile(DictionaryKind::Translation, QStringLiteral("工作"), QStringLiteral("zh"), QStringLiteral("es"));
        profiles.setEnabled(id, false);
        profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("西语"), QStringLiteral("es"), QString());
    }
    DictionaryProfiles again(f.dir(), f.legacy());
    const auto translation = again.profiles(DictionaryKind::Translation);
    ASSERT_EQ(translation.size(), 1);
    EXPECT_EQ(translation[0].id, id);
    EXPECT_EQ(translation[0].name, QStringLiteral("工作"));
    EXPECT_EQ(translation[0].langA, QStringLiteral("zh"));
    EXPECT_EQ(translation[0].langB, QStringLiteral("es"));
    EXPECT_FALSE(translation[0].enabled);
    EXPECT_EQ(again.profiles(DictionaryKind::Recognition).size(), 2);
}

TEST(DictionaryProfilesTest, EachProfileOwnsItsTsvAndKeepsItsEntries) {
    Fixture f;
    QString id;
    {
        DictionaryProfiles profiles(f.dir(), f.legacy());
        id = profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("西语"), QStringLiteral("es"), QString());
        profiles.library(id)->addItem(item(QStringLiteral("hola"), QStringLiteral("Hola")));
    }
    EXPECT_TRUE(QFile::exists(QDir(f.dir()).filePath(id + QStringLiteral(".tsv"))));

    DictionaryProfiles again(f.dir(), f.legacy());
    ASSERT_EQ(again.library(id)->items().size(), 1);
    EXPECT_EQ(again.library(id)->items()[0].correct, QStringLiteral("Hola"));
}

TEST(DictionaryProfilesTest, DefaultCannotBeRemovedButOthersCanWithTheirFile) {
    Fixture f;
    DictionaryProfiles profiles(f.dir(), f.legacy());
    EXPECT_FALSE(profiles.removeProfile(DictionaryProfiles::defaultId()));

    const QString id = profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("临时"), QStringLiteral("zh"), QString());
    profiles.library(id)->addItem(item(QStringLiteral("a"), QStringLiteral("b")));
    const QString file = QDir(f.dir()).filePath(id + QStringLiteral(".tsv"));
    ASSERT_TRUE(QFile::exists(file));

    EXPECT_TRUE(profiles.removeProfile(id));
    EXPECT_FALSE(QFile::exists(file));
    EXPECT_EQ(profiles.profiles(DictionaryKind::Recognition).size(), 1);
}

TEST(DictionaryProfilesTest, ApplicableRecognitionProfilesFollowLanguageAndSwitch) {
    Fixture f;
    DictionaryProfiles profiles(f.dir(), f.legacy());
    const QString es = profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("西语"), QStringLiteral("es"), QString());
    const QString zh = profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("中文"), QStringLiteral("zh"), QString());
    profiles.addProfile(DictionaryKind::Translation, QStringLiteral("中西"), QStringLiteral("zh"), QStringLiteral("es"));

    auto ids = [&](const QString& code) {
        QStringList out;
        for (const auto& p : profiles.applicableRecognition(code)) out << p.id;
        return out;
    };
    EXPECT_EQ(ids(QStringLiteral("es")), (QStringList{ DictionaryProfiles::defaultId(), es }));
    EXPECT_EQ(ids(QStringLiteral("zh")), (QStringList{ DictionaryProfiles::defaultId(), zh }));
    EXPECT_EQ(ids(QStringLiteral("yue")), (QStringList{ DictionaryProfiles::defaultId(), zh }));
    EXPECT_EQ(ids(QStringLiteral("en")), (QStringList{ DictionaryProfiles::defaultId() }));

    profiles.setEnabled(zh, false);
    EXPECT_EQ(ids(QStringLiteral("zh")), (QStringList{ DictionaryProfiles::defaultId() }));
}

TEST(DictionaryProfilesTest, MergedRulesKeepTheEarlierProfileForTheSameHeardWord) {
    Fixture f;
    f.writeLegacy("1\tfoo\tFirst\t\tall\t\t\n");
    DictionaryProfiles profiles(f.dir(), f.legacy());
    const QString es = profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("西语"), QStringLiteral("es"), QString());
    TermItem later = item(QStringLiteral("foo"), QStringLiteral("Second"));
    profiles.library(es)->addItem(later);
    profiles.library(es)->addItem(item(QStringLiteral("bar"), QStringLiteral("Bar")));

    const auto rules = profiles.buildRecognitionRules(QStringLiteral("es"));
    const QStringList parts = rules.replaceRules.split(';', Qt::SkipEmptyParts);
    EXPECT_TRUE(parts.contains(QStringLiteral("foo=>First")));
    EXPECT_FALSE(parts.contains(QStringLiteral("foo=>Second")));
    EXPECT_TRUE(parts.contains(QStringLiteral("bar=>Bar")));
    EXPECT_TRUE(rules.aiVocab.contains(QStringLiteral("First")));
    EXPECT_TRUE(rules.aiVocab.contains(QStringLiteral("Bar")));
    EXPECT_EQ(rules.hotwords, (QStringList{ QStringLiteral("First"), QStringLiteral("Second"), QStringLiteral("Bar") }));
    EXPECT_EQ(rules.hotwordCount, rules.hotwords.size());
}

TEST(DictionaryProfilesTest, HotwordsWithAMisheardFormComeFirstAndAreDeduplicated) {
    Fixture f;
    DictionaryProfiles profiles(f.dir(), f.legacy());
    TermItem plain;
    plain.correct = QStringLiteral("Alpha");
    profiles.library(DictionaryProfiles::defaultId())->addItem(plain);
    const QString es = profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("西语"), QStringLiteral("es"), QString());
    profiles.library(es)->addItem(item(QStringLiteral("beta"), QStringLiteral("Beta")));
    TermItem dup;
    dup.correct = QStringLiteral("Alpha");
    profiles.library(es)->addItem(dup);

    const auto rules = profiles.buildRecognitionRules(QStringLiteral("es"));
    EXPECT_EQ(rules.hotwords, (QStringList{ QStringLiteral("Beta"), QStringLiteral("Alpha") }));
}

TEST(DictionaryProfilesTest, RulesChangedFiresOnEditsAndSwitches) {
    Fixture f;
    DictionaryProfiles profiles(f.dir(), f.legacy());
    int fired = 0;
    QObject::connect(&profiles, &DictionaryProfiles::rulesChanged, [&] { ++fired; });

    profiles.library(DictionaryProfiles::defaultId())->addItem(item(QStringLiteral("a"), QStringLiteral("b")));
    EXPECT_GE(fired, 1);
    const int afterEdit = fired;
    const QString id = profiles.addProfile(DictionaryKind::Recognition, QStringLiteral("西语"), QStringLiteral("es"), QString());
    profiles.setEnabled(id, false);
    EXPECT_GT(fired, afterEdit);
}

TEST(DictionaryProfilesTest, CorruptIndexFallsBackToTheDefaultProfile) {
    Fixture f;
    QDir().mkpath(f.dir());
    QFile bad(QDir(f.dir()).filePath(QStringLiteral("index.json")));
    ASSERT_TRUE(bad.open(QIODevice::WriteOnly));
    bad.write("{ not json");
    bad.close();

    DictionaryProfiles profiles(f.dir(), f.legacy());
    ASSERT_EQ(profiles.profiles(DictionaryKind::Recognition).size(), 1);
    EXPECT_EQ(profiles.profiles(DictionaryKind::Recognition)[0].id, DictionaryProfiles::defaultId());
}

TEST(DictionaryProfilesTest, TranslationGlossaryFollowsThePairInEitherDirection) {
    Fixture f;
    DictionaryProfiles profiles(f.dir(), f.legacy());
    const QString id = profiles.addProfile(DictionaryKind::Translation, QStringLiteral("工作"), QStringLiteral("zh"), QStringLiteral("es"));
    profiles.library(id)->addItem(item(QStringLiteral("密钥"), QStringLiteral("clave")));

    const auto forward = profiles.translationGlossary(QStringLiteral("zh"), QStringLiteral("es"));
    ASSERT_EQ(forward.size(), 1);
    EXPECT_EQ(forward[0].first, QStringLiteral("密钥"));
    EXPECT_EQ(forward[0].second, QStringLiteral("clave"));

    const auto backward = profiles.translationGlossary(QStringLiteral("es"), QStringLiteral("zh"));
    ASSERT_EQ(backward.size(), 1);
    EXPECT_EQ(backward[0].first, QStringLiteral("clave"));
    EXPECT_EQ(backward[0].second, QStringLiteral("密钥"));

    EXPECT_TRUE(profiles.translationGlossary(QStringLiteral("zh"), QStringLiteral("en")).isEmpty());
    profiles.setEnabled(id, false);
    EXPECT_TRUE(profiles.translationGlossary(QStringLiteral("zh"), QStringLiteral("es")).isEmpty());
}
