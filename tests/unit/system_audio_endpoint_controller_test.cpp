#include <gtest/gtest.h>

#include "utils/SystemAudioEndpointController.h"

// 控制器是纯 C++、不依赖 Qt；这些测试验证接口可安全调用、边界（空 id）不崩、
// 跨平台空实现/Win COM 实现都不产生未定义行为。注意：本测试不会切换到真实系统
// 默认设备（避免污染实机音频设置），只用非法/空 id 验证安全路径。

TEST(SystemAudioEndpointControllerTest, ConstructAndDestroySafe) {
    SystemAudioEndpointController ctrl;
    // 析构不应崩溃
}

TEST(SystemAudioEndpointControllerTest, EmptyIdIsNoOp) {
    SystemAudioEndpointController ctrl;
    // 空 id：不应切换、不应记录恢复点、不应崩溃
    EXPECT_FALSE(ctrl.setDefaultOutput(""));
    EXPECT_FALSE(ctrl.setDefaultInput(""));
    // 此时 restore 也安全（无记录）
    EXPECT_NO_THROW(ctrl.restore());
}

TEST(SystemAudioEndpointControllerTest, BogusIdDoesNotCrash) {
    SystemAudioEndpointController ctrl;
    // 一个不存在的端点 id：
    // - Windows：COM SetDefaultEndpoint 失败，应返回 false（不切换真实设备）
    // - 非 Windows：no-op 成功，返回 true
    bool out = ctrl.setDefaultOutput("__nonexistent_endpoint_id__");
    bool in = ctrl.setDefaultInput("__nonexistent_endpoint_id__");
    (void)out; (void)in;
#ifdef _WIN32
    EXPECT_FALSE(out);
    EXPECT_FALSE(in);
#else
    EXPECT_TRUE(out);
    EXPECT_TRUE(in);
#endif
    // restore 恢复（安全）
    EXPECT_NO_THROW(ctrl.restore());
}

TEST(SystemAudioEndpointControllerTest, GetDefaultReturnsString) {
    SystemAudioEndpointController ctrl;
    // 取当前系统默认端点 id：在非 Win 返回空串，Win 返回真实 id，均不应崩溃
    std::string outId = ctrl.getDefaultOutputId();
    std::string inId = ctrl.getDefaultInputId();
    (void)outId; (void)inId;
    SUCCEED();
}

TEST(SystemAudioEndpointControllerTest, RestoreAfterSetIsSafe) {
    SystemAudioEndpointController ctrl;
    // 即便 set 失败（非法 id），restore 仍应安全
    ctrl.setDefaultOutput("bad");
    ctrl.setDefaultInput("bad");
    EXPECT_NO_THROW(ctrl.restore());
    // 二次 restore 也安全
    EXPECT_NO_THROW(ctrl.restore());
}
