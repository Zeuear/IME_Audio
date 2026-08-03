#include <gtest/gtest.h>
#include <QString>
#include <QVector>
#include <QPair>

#include "TermsLibraryManager.h"

// ---------- D1 regression tests: ordering & malformed-rule safety ----------
//
// These tests reproduce the class of bug where applyReplaceRules produces
// wrong output because shorter "wrong" keys are matched before longer ones,
// and where malformed rule strings crash or produce empty results instead
// of being safely skipped.

class TermReplaceTest : public ::testing::Test {};

// --- Ordering: longer key wins ---

TEST_F(TermReplaceTest, LongerKeyNotEatenByShorter) {
    // "aa" should become "y", not "xy"
    QString rules = "a=>x;aa=>y";
    QString input = "aa";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "y");
}

TEST_F(TermReplaceTest, LongerKeyBeforeShorterInString) {
    // "aa" at end of "baa" → "b" + "y"; the standalone "a" in "baa" is part
    // of "aa" so it should not be replaced separately.
    QString rules = "a=>x;aa=>y";
    QString input = "baa";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "by");
}

TEST_F(TermReplaceTest, LongerKeyInMiddleOfString) {
    QString rules = "it=>它;italy=>意大利";
    QString input = "italy is it";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "意大利 is 它");
}

TEST_F(TermReplaceTest, OverlappingKeysPrefersLongest) {
    // Among "ab", "abc", "bc" — "abc" is longest, should win at position 0-2
    QString rules = "ab=>X;abc=>Y;bc=>Z";
    QString input = "abc";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "Y");
}

TEST_F(TermReplaceTest, MultipleShortKeysDoNotCorruptLonger) {
    QString rules = "a=>1;b=>2;ab=>long";
    QString input = "ab a b";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "long 1 2");
}

// --- Malformed rules: should be silently skipped, not crash ---

TEST_F(TermReplaceTest, EmptyWrongWordSkipped) {
    // "=>x" has empty wrong word → rule is malformed, should be skipped
    QString rules = "=>skip;x=>yes";
    QString input = "x";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "yes");
}

TEST_F(TermReplaceTest, EmptyCorrectValueIsDeletion) {
    // "a=>" has empty correct value → treats as deletion (intentional behavior)
    QString rules = "a=>";
    QString input = "banana";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "bnn");
}

TEST_F(TermReplaceTest, RuleWithoutArrowSkipped) {
    // "invalid" has no => → should be skipped
    // Note: "a=>b" will still match substring "a" inside "invalid",
    // this is a known limitation (D2: word-boundary matching) — not part of D1 fix.
    QString rules = "invalid;a=>b";
    QString input = "a";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "b");
}

// --- Edge cases ---

TEST_F(TermReplaceTest, EmptyRulesReturnsOriginal) {
    QString result = TermsLibraryManager::applyReplaceRules("hello world", "");
    EXPECT_EQ(result, "hello world");
}

TEST_F(TermReplaceTest, NoMatchReturnsOriginal) {
    QString rules = "foo=>bar";
    QString result = TermsLibraryManager::applyReplaceRules("hello", rules);
    EXPECT_EQ(result, "hello");
}

TEST_F(TermReplaceTest, ExactMatchAtBoundaries) {
    QString rules = "ok=>OK";
    QString input = "ok";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "OK");
}

TEST_F(TermReplaceTest, MultipleOccurrencesSameRule) {
    QString rules = "hi=>hello";
    QString input = "hi there hi";
    QString result = TermsLibraryManager::applyReplaceRules(input, rules);
    EXPECT_EQ(result, "hello there hello");
}

TEST_F(TermReplaceTest, RulesAlreadySortedByLength) {
    // Rules provided longest-first should behave identically
    QString rulesA = "aa=>y;a=>x";
    QString rulesB = "a=>x;aa=>y";
    QString input = "baa";
    QString resultA = TermsLibraryManager::applyReplaceRules(input, rulesA);
    QString resultB = TermsLibraryManager::applyReplaceRules(input, rulesB);
    EXPECT_EQ(resultA, resultB);
    EXPECT_EQ(resultA, "by");
}
