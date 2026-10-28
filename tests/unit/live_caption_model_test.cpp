#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QObject>

#include "widgets/LiveCaptionModel.h"

namespace {
// 不依赖 Qt Test 的信号计数器
class Counter : public QObject {
public:
    template <typename Sender, typename Signal>
    Counter(Sender* s, Signal sig) { connect(s, sig, this, [this]() { ++count_; }); }
    int count() const { return count_; }
private:
    int count_ = 0;
};
} // namespace

class LiveCaptionModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 0;
            static char* argv[] = { nullptr };
            m_app = new QCoreApplication(argc, argv);
        }
        model.setSpeaking(true);
    }

    void speak(const QString& partial) { model.setPartial(model.epoch(), partial); }
    // 模拟 WorkflowManager：每次切句分配一个新 id
    quint64 cut(qint64 now = 1000) { model.onUtteranceReady(++m_nextId, now); return m_nextId; }

    QCoreApplication* m_app = nullptr;
    LiveCaptionModel model;
    quint64 m_nextId = 0;
};


TEST_F(LiveCaptionModelTest, PartialShowsAsPendingColorSegment) {
    speak("hello");
    const auto segs = model.segments();
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].text, "hello");
    EXPECT_FALSE(segs[0].isFinal);
}

TEST_F(LiveCaptionModelTest, PartialUpdatesReplaceSameSentence) {
    speak("hello");
    speak("hello wor");
    speak("hello world");
    EXPECT_EQ(model.plainText(), "hello world");
    EXPECT_EQ(model.segments().size(), 1);
}

TEST_F(LiveCaptionModelTest, EmptyPartialDoesNotEraseExistingText) {
    speak("你好世界");
    speak("");
    EXPECT_EQ(model.plainText(), "你好世界");
}

TEST_F(LiveCaptionModelTest, PartialIgnoredWhenNotSpeaking) {
    model.setSpeaking(false);
    speak("noise");
    EXPECT_TRUE(model.isEmpty());
}

// VAD 判定"开始说话"比流式识别出字晚（要持续一个最短录音时长）；
// 引擎只在文字变化时才发中间结果，所以这段时间里收到的文字要先留着，说话开始后补上
TEST_F(LiveCaptionModelTest, PartialArrivingBeforeVoiceStartsIsShownOnceItDoes) {
    model.setSpeaking(false);
    speak("你好");                       // 先于 voiceStarted 到达，且之后文字不再变化
    EXPECT_TRUE(model.isEmpty());
    model.setSpeaking(true);
    EXPECT_EQ(model.plainText(), "你好");
}

TEST_F(LiveCaptionModelTest, HeldPartialIsReplacedByNewerOne) {
    model.setSpeaking(false);
    speak("你");
    speak("你好");
    model.setSpeaking(true);
    EXPECT_EQ(model.plainText(), "你好");
}

TEST_F(LiveCaptionModelTest, HeldNoiseIsDroppedWhenSentenceIsCutWithoutSpeech) {
    model.setSpeaking(false);
    speak("noise");
    model.clear();                       // 没有真正的说话：清空时丢弃，下一次开口不能冒出旧噪声
    model.setSpeaking(true);
    EXPECT_TRUE(model.isEmpty());
}

TEST_F(LiveCaptionModelTest, ActiveSentenceKeepsUpdatingAfterVoiceStops) {
    speak("hello");
    model.setSpeaking(false);   // VAD 已判定结束，流式解码还在追句尾
    speak("hello world");
    EXPECT_EQ(model.plainText(), "hello world");
}

TEST_F(LiveCaptionModelTest, LatePartialOfCutSentenceUpdatesThatSentence) {
    speak("first");
    const quint64 old = model.epoch();
    cut();
    model.setPartial(old, "first tail");   // 切句后才解出来的句尾
    EXPECT_EQ(model.plainText(), "first tail");
}

TEST_F(LiveCaptionModelTest, LatePartialOfClearedSentenceIsIgnored) {
    speak("first");
    const quint64 old = model.epoch();
    cut();
    speak("second");                       // 下一句开口，上一句已清掉
    model.setPartial(old, "first tail");
    EXPECT_EQ(model.plainText(), "second");
}

TEST_F(LiveCaptionModelTest, LatePartialFillsPlaceholderWithoutText) {
    const quint64 old = model.epoch();
    cut();                                  // 切句时流式还没出字
    model.setPartial(old, "late");
    EXPECT_EQ(model.plainText(), "late");
    EXPECT_FALSE(model.segments()[0].isFinal);
}

TEST_F(LiveCaptionModelTest, LatePartialDoesNotOverwriteFinal) {
    speak("first");
    const quint64 old = model.epoch();
    const quint64 id = cut();
    model.onTranscribed(id, "First.", 2000);
    model.setPartial(old, "first tail");
    EXPECT_EQ(model.plainText(), "First.");
}

TEST_F(LiveCaptionModelTest, TextFilterAppliesToPartials) {
    model.setTextFilter([](const QString& t) { QString s = t; return s.replace("k8s", "Kubernetes"); });
    speak("deploy k8s");
    EXPECT_EQ(model.plainText(), "deploy Kubernetes");
}

TEST_F(LiveCaptionModelTest, ChangedSignalOnlyWhenTextChanges) {
    Counter spy(&model, &LiveCaptionModel::changed);
    speak("abc");
    speak("abc");
    EXPECT_EQ(spy.count(), 1);
}


TEST_F(LiveCaptionModelTest, UtteranceReadyBumpsEpochAndKeepsText) {
    Counter spy(&model, &LiveCaptionModel::epochChanged);
    speak("你好");
    const quint64 before = model.epoch();
    cut();
    EXPECT_EQ(model.epoch(), before + 1);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(model.plainText(), "你好");
    EXPECT_FALSE(model.segments()[0].isFinal);
}

TEST_F(LiveCaptionModelTest, FinalReplacesPendingAndTurnsFinal) {
    speak("hello wor");
    const quint64 id = cut();
    model.onTranscribed(id, "Hello world.", 2000);
    const auto segs = model.segments();
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].text, "Hello world.");
    EXPECT_TRUE(segs[0].isFinal);
}

TEST_F(LiveCaptionModelTest, FinalsMatchPendingById) {
    speak("one");  const quint64 one = cut(1000);
    const quint64 two = cut(1001);           // 第二句切出时流式还没出字，第一句仍在等结果
    model.onTranscribed(one, "ONE-final", 2000);
    auto segs = model.segments();
    ASSERT_EQ(segs.size(), 1);
    EXPECT_TRUE(segs[0].isFinal);
    EXPECT_EQ(segs[0].text, "ONE-final");

    model.onTranscribed(two, "TWO-final", 2001);   // 后一句出结果，前一句随之清掉
    segs = model.segments();
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].text, "TWO-final");
    EXPECT_TRUE(segs[0].isFinal);
}

TEST_F(LiveCaptionModelTest, StartingNextSentenceClearsPendingPrevious) {
    speak("one"); const quint64 one = cut(1000);
    speak("two");                            // 第一句还在等正式结果，第二句已开口
    EXPECT_EQ(model.plainText(), "two");
    model.onTranscribed(one, "ONE-final", 2000);   // 迟到的结果不再显示
    EXPECT_EQ(model.plainText(), "two");
}

TEST_F(LiveCaptionModelTest, EmptyFinalRemovesThePendingSentence) {
    speak("noise words");
    const quint64 id = cut();
    model.onTranscribed(id, "", 2000);   // 转录失败/空结果：浅色残留必须消失
    EXPECT_TRUE(model.isEmpty());
}

TEST_F(LiveCaptionModelTest, CutWithoutAnyPartialStillOccupiesASlot) {
    // 没有流式模型的语言：从未有中间结果，但正式结果仍要对上号
    const quint64 a = cut(1000);
    const quint64 b = cut(1001);
    model.onTranscribed(a, "A.", 2000);
    EXPECT_EQ(model.plainText(), "A.");
    model.onTranscribed(b, "B.", 2001);
    EXPECT_EQ(model.plainText(), "B.");
    EXPECT_TRUE(model.segments()[0].isFinal);
}

TEST_F(LiveCaptionModelTest, FinalWithUnknownIdIsDropped) {
    model.onTranscribed(0, "orphan", 1000);
    EXPECT_TRUE(model.isEmpty());
}

TEST_F(LiveCaptionModelTest, ExpiredPendingDoesNotShiftLaterFinals) {
    speak("slow"); const quint64 slow = cut(0);
    model.expire(LiveCaptionModel::kPendingTimeoutMs);   // 第一句超时被移除
    speak("next"); const quint64 next = cut(LiveCaptionModel::kPendingTimeoutMs + 1);
    model.onTranscribed(slow, "SLOW-final", LiveCaptionModel::kPendingTimeoutMs + 2);   // 迟到的结果
    EXPECT_EQ(model.plainText(), "next");
    EXPECT_FALSE(model.segments()[0].isFinal);
    model.onTranscribed(next, "NEXT-final", LiveCaptionModel::kPendingTimeoutMs + 3);
    EXPECT_EQ(model.plainText(), "NEXT-final");
}

TEST_F(LiveCaptionModelTest, NewSentenceClearsLingeringFinal) {
    speak("first"); const quint64 id = cut(); model.onTranscribed(id, "First.", 1000);
    EXPECT_EQ(model.plainText(), "First.");   // 还在停留期内
    speak("second");                          // 开始说下一句，上一句立即清掉
    const auto segs = model.segments();
    ASSERT_EQ(segs.size(), 1);
    EXPECT_EQ(segs[0].text, "second");
    EXPECT_FALSE(segs[0].isFinal);
}


TEST_F(LiveCaptionModelTest, FinalExpiresAfterLinger) {
    speak("x"); const quint64 id = cut(); model.onTranscribed(id, "done", 10000);
    EXPECT_FALSE(model.expire(10000 + LiveCaptionModel::kFinalLingerMs - 1));
    EXPECT_FALSE(model.isEmpty());
    EXPECT_TRUE(model.expire(10000 + LiveCaptionModel::kFinalLingerMs));
    EXPECT_TRUE(model.isEmpty());
}

TEST_F(LiveCaptionModelTest, PendingNeverExpiresEarlyButEventuallyDoes) {
    speak("slow cloud"); cut(5000);
    EXPECT_FALSE(model.expire(5000 + LiveCaptionModel::kFinalLingerMs + 1));   // 云端慢，不能像最终句那样很快消失
    EXPECT_FALSE(model.isEmpty());
    EXPECT_TRUE(model.expire(5000 + LiveCaptionModel::kPendingTimeoutMs));
    EXPECT_TRUE(model.isEmpty());
}

TEST_F(LiveCaptionModelTest, ClearEmptiesAndInvalidatesOldEpoch) {
    speak("old");
    const quint64 old = model.epoch();
    model.clear();
    EXPECT_TRUE(model.isEmpty());
    model.setSpeaking(true);
    model.setPartial(old, "ghost");
    EXPECT_TRUE(model.isEmpty());
}

TEST_F(LiveCaptionModelTest, ClearEmitsEpochChangedEvenWhenEmpty) {
    Counter spy(&model, &LiveCaptionModel::epochChanged);
    model.clear();
    EXPECT_EQ(spy.count(), 1);   // 流式引擎靠它重置，空内容也不能漏
}

TEST_F(LiveCaptionModelTest, KeepsOnlyCurrentSentence) {
    for (int i = 0; i < 10; ++i) {
        speak(QString("s%1").arg(i));
        const quint64 id = cut(1000 + i);
        model.onTranscribed(id, QString("F%1").arg(i), 2000 + i);
    }
    EXPECT_EQ(model.plainText(), "F9");
}


TEST(CaptionText, AllCapsEnglishBecomesSentenceCase) {
    EXPECT_EQ(CaptionText::normalizePartial("HELLO WORLD"), "Hello world");
    EXPECT_EQ(CaptionText::normalizePartial("I THINK I CAN"), "I think I can");
}

TEST(CaptionText, MixedCaseIsLeftAlone) {
    EXPECT_EQ(CaptionText::normalizePartial("Hello World"), "Hello World");
    EXPECT_EQ(CaptionText::normalizePartial("iPhone"), "iPhone");
}

TEST(CaptionText, UpperCaseInsideCjkIsKept) {
    EXPECT_EQ(CaptionText::normalizePartial("我用 API 调用"), "我用 API 调用");
}

TEST(CaptionText, ChineseIsLeftAloneAndWhitespaceCollapsed) {
    EXPECT_EQ(CaptionText::normalizePartial("  你好   世界 "), "你好 世界");
    EXPECT_EQ(CaptionText::normalizePartial(""), "");
}

TEST(CaptionText, SeparatorNoSpaceAroundCjk) {
    EXPECT_EQ(LiveCaptionModel::separatorBetween("你好", "世界"), "");
    EXPECT_EQ(LiveCaptionModel::separatorBetween("hello", "世界"), "");
    EXPECT_EQ(LiveCaptionModel::separatorBetween("你好", "world"), "");
    EXPECT_EQ(LiveCaptionModel::separatorBetween("안녕", "하세요"), "");
    EXPECT_EQ(LiveCaptionModel::separatorBetween("hello", "world"), " ");
    EXPECT_EQ(LiveCaptionModel::separatorBetween("", "world"), "");
}
