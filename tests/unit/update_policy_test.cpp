#include "update_policy.h"
#include <gtest/gtest.h>

// 默认配置：未选择"不再提醒" → 应弹更新提示
TEST(UpdatePolicyTest, DefaultPromptsUpdate) {
    EXPECT_TRUE(shouldPromptUpdate(false));
}

// 用户已勾选"不再提醒" → 不再弹更新提示
TEST(UpdatePolicyTest, SkipReminderSuppressesPrompt) {
    EXPECT_FALSE(shouldPromptUpdate(true));
}
