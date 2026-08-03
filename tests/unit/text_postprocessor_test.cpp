#include <gtest/gtest.h>
#include <QString>

// RED PHASE: This test will not compile until TextPostProcessor is extracted
// from TranscriptionService::postProcess into a testable public interface.
#include "TextPostProcessor.h"

class TextPostProcessorTest : public ::testing::Test {};

// --- Punctuation auto-append for Sherpa backend ---
TEST_F(TextPostProcessorTest, AppendsPeriodForIncompleteEnglishSentence) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("Hello world", "en");
    EXPECT_EQ(result, "Hello world.");
}

TEST_F(TextPostProcessorTest, AppendsQuestionMarkForIncompletedQuestion) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("What time is it", "en");
    EXPECT_EQ(result, "What time is it?");
}

TEST_F(TextPostProcessorTest, AppendsChineseFullStopForIncompleteChinese) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("今天天气很好", "zh");
    EXPECT_EQ(result, "今天天气很好。");
}

TEST_F(TextPostProcessorTest, DoesNotAppendWhenAlreadyEndsWithPunctuation) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("Hello world.", "en");
    EXPECT_EQ(result, "Hello world.");
}

TEST_F(TextPostProcessorTest, DoesNotAppendWhenAlreadyEndsWithChinesePunctuation) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("你好吗？", "zh");
    EXPECT_EQ(result, "你好吗？");
}

TEST_F(TextPostProcessorTest, DetectsAskingQuestionByAskingWords) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("What is the plan", "en");
    EXPECT_EQ(result, "What is the plan?");
}

TEST_F(TextPostProcessorTest, DetectsAskingQuestionByAskingWordsChinese) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("今天几点", "zh");
    EXPECT_EQ(result, "今天几点？");
}

TEST_F(TextPostProcessorTest, EmptyInputReturnsEmpty) {
    TextPostProcessor proc;
    QString result = proc.addAutoPunctuation("", "zh");
    EXPECT_TRUE(result.isEmpty());
}

// --- Term replacement integration with postProcess ---
TEST_F(TextPostProcessorTest, FullPostProcessAppliesRulesThenPunctuation) {
    TextPostProcessor proc;
    proc.setReplaceRules("hi=>hello");
    proc.setLanguage("en");
    QString result = proc.postProcess("hi world");
    EXPECT_EQ(result, "hello world.");
}

TEST_F(TextPostProcessorTest, FullPostProcessNoRulesJustPunctuation) {
    TextPostProcessor proc;
    proc.setReplaceRules("");
    proc.setLanguage("en");
    QString result = proc.postProcess("test sentence");
    EXPECT_EQ(result, "test sentence.");
}
