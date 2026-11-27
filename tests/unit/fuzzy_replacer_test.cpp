#include <gtest/gtest.h>

#include <QFile>

#include "services/dictionary/FuzzyReplacer.h"

namespace {

const PinyinTable& realTable() {
    static const PinyinTable table = []() {
        QFile f(QStringLiteral(IME_AUDIO_RESOURCES_DIR "/data/pinyin_table.txt"));
        EXPECT_TRUE(f.open(QIODevice::ReadOnly));
        return PinyinTable::fromText(QString::fromUtf8(f.readAll()));
    }();
    return table;
}

FuzzyReplacer make(const QList<HotwordSelector::Entry>& entries) {
    FuzzyReplacer r(&realTable());
    r.setEntries(entries);
    return r;
}

}

TEST(FuzzyReplacerTest, HomophoneOfAChineseTermIsReplaced) {
    const auto r = make({ { QStringLiteral("科达"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("我在科大上班")), QStringLiteral("我在科达上班"));
}

TEST(FuzzyReplacerTest, FuzzyPinyinPairsCountAsTheSameSound) {
    const auto r = make({ { QStringLiteral("张三"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("占三来了")), QStringLiteral("张三来了"));
}

TEST(FuzzyReplacerTest, ADifferentSoundIsLeftAlone) {
    const auto r = make({ { QStringLiteral("科达"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("我在公司上班")), QStringLiteral("我在公司上班"));
}

TEST(FuzzyReplacerTest, TermAlreadyWrittenCorrectlyIsKept) {
    const auto r = make({ { QStringLiteral("科达"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("科达和科大")), QStringLiteral("科达和科大"));
}

TEST(FuzzyReplacerTest, MisspelledLatinWordIsCorrected) {
    const auto r = make({ { QStringLiteral("Kubernetes"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("deploy it on kubernetis today")), QStringLiteral("deploy it on Kubernetes today"));
}

TEST(FuzzyReplacerTest, LatinMatchNeverCutsIntoAnotherWord) {
    const auto r = make({ { QStringLiteral("Kubernetes"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("the kubernetesclusters here")), QStringLiteral("the kubernetesclusters here"));
    EXPECT_EQ(r.apply(QStringLiteral("a completely different sentence")), QStringLiteral("a completely different sentence"));
}

TEST(FuzzyReplacerTest, ShortLatinTermsAreNotFuzzyMatched) {
    const auto r = make({ { QStringLiteral("Kuda"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("I met cuda yesterday")), QStringLiteral("I met cuda yesterday"));
}

TEST(FuzzyReplacerTest, SingleCharacterChineseTermsAreIgnored) {
    const auto r = make({ { QStringLiteral("达"), {} } });
    EXPECT_EQ(r.entryCount(), 0);
    EXPECT_EQ(r.apply(QStringLiteral("大")), QStringLiteral("大"));
}

TEST(FuzzyReplacerTest, HeardFormsAlsoActAsPatterns) {
    const auto r = make({ { QStringLiteral("Claude"), { QStringLiteral("克劳德") } } });
    EXPECT_EQ(r.apply(QStringLiteral("请问克老德怎么用")), QStringLiteral("请问Claude怎么用"));
}

TEST(FuzzyReplacerTest, OverlappingHitsKeepTheBetterScore) {
    const auto r = make({ { QStringLiteral("科达科技"), {} }, { QStringLiteral("科达"), {} } });
    EXPECT_EQ(r.apply(QStringLiteral("我在科大科技上班")), QStringLiteral("我在科达科技上班"));
}

TEST(FuzzyReplacerTest, EmptyInputsAreSafe) {
    const FuzzyReplacer empty(&realTable());
    EXPECT_EQ(empty.apply(QStringLiteral("你好")), QStringLiteral("你好"));
    const auto r = make({ { QStringLiteral("科达"), {} } });
    EXPECT_EQ(r.apply(QString()), QString());
}
