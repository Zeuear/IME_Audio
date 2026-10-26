#include <gtest/gtest.h>

#include "utils/EnhancementFallback.h"

TEST(EnhancementFallbackTest, SuccessUsesPolishedText) {
    const auto outcome = decideEnhancementOutcome(true, QStringLiteral("原文"), QStringLiteral("润色后"));
    EXPECT_TRUE(outcome.success);
    EXPECT_FALSE(outcome.usedFallback);
    EXPECT_EQ(outcome.text, QStringLiteral("润色后"));
}

TEST(EnhancementFallbackTest, FailureFallsBackToOriginal) {
    const auto outcome = decideEnhancementOutcome(false, QStringLiteral("原文"), {});
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.usedFallback);
    EXPECT_EQ(outcome.text, QStringLiteral("原文"));
}

TEST(EnhancementFallbackTest, FailureWithoutOriginalStaysFailed) {
    const auto outcome = decideEnhancementOutcome(false, {}, {});
    EXPECT_FALSE(outcome.success);
    EXPECT_FALSE(outcome.usedFallback);
    EXPECT_TRUE(outcome.text.isEmpty());
}
