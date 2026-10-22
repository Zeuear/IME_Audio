#include <gtest/gtest.h>
#include <QString>

#include "utils/OrderedResults.h"

TEST(OrderedResults, InOrderCompletionReleasesImmediately) {
    OrderedResults<QString> q;
    const auto a = q.reserve();
    const auto b = q.reserve();
    EXPECT_TRUE(q.complete(a, "a"));
    EXPECT_EQ(q.takeReady(), (QList<QString>{"a"}));
    EXPECT_TRUE(q.complete(b, "b"));
    EXPECT_EQ(q.takeReady(), (QList<QString>{"b"}));
}

TEST(OrderedResults, LaterResultWaitsForEarlierOne) {
    OrderedResults<QString> q;
    const auto a = q.reserve();
    const auto b = q.reserve();
    const auto c = q.reserve();
    q.complete(c, "c");
    q.complete(b, "b");
    EXPECT_TRUE(q.takeReady().isEmpty());
    q.complete(a, "a");
    EXPECT_EQ(q.takeReady(), (QList<QString>{"a", "b", "c"}));
}

TEST(OrderedResults, OnlyFirstCompletionCounts) {
    OrderedResults<QString> q;
    const auto a = q.reserve();
    const auto b = q.reserve();
    EXPECT_TRUE(q.complete(b, "timeout"));
    EXPECT_FALSE(q.complete(b, "late real result"));   // 还没放行，也不能覆盖
    q.complete(a, "a");
    EXPECT_EQ(q.takeReady(), (QList<QString>{"a", "timeout"}));
    EXPECT_FALSE(q.complete(a, "again"));               // 已放行
}

TEST(OrderedResults, UnknownSequenceIsRejected) {
    OrderedResults<QString> q;
    EXPECT_FALSE(q.complete(0, "never reserved"));
    EXPECT_TRUE(q.takeReady().isEmpty());
}
