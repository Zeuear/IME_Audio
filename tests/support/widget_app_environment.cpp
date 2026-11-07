#include <gtest/gtest.h>
#include <QApplication>

// 控件需要 QApplication；其它测试文件按需创建的是 QCoreApplication，
// 用全局环境在所有测试之前建好，它们看到 qApp 已存在就会直接复用。
// 放在公共文件里，新增的界面测试不必隐式依赖某个具体测试文件。
namespace {
class WidgetAppEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!qApp) {
#ifdef Q_OS_LINUX
            // 无显示器的环境（如 CI）没有 xcb 可连，改用离屏平台，否则每个用例启动即失败
            if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
                qputenv("QT_QPA_PLATFORM", "offscreen");
            }
#endif
            static int argc = 0;
            static char* argv[] = { nullptr };
            new QApplication(argc, argv);
        }
    }
};
const auto* const kWidgetApp = ::testing::AddGlobalTestEnvironment(new WidgetAppEnvironment);
} // namespace
