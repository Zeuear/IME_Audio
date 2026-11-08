#include <gtest/gtest.h>
#include <QColor>
#include <QImage>
#include <QPalette>
#include <QString>

#include "ui/controls/ToolIcon.h"

namespace {
const QStringList kToolIcons = {
    "tool_record", "tool_stop", "tool_ai_enhance", "tool_translate",
    "tool_inject", "tool_transcript", "tool_dictionary", "tool_file", "tool_settings",
};

QString iconPath(const QString& name)
{
    return QStringLiteral(IME_AUDIO_RESOURCES_DIR "/icons/%1.svg").arg(name);
}

// 三套主题的调色板各取不同的已知颜色，断言颜色来自调色板而不是写死
QPalette makePalette(const char* placeholder, const char* highlight, const char* disabledText)
{
    QPalette p;
    p.setColor(QPalette::PlaceholderText, QColor(placeholder));
    p.setColor(QPalette::Highlight, QColor(highlight));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(disabledText));
    return p;
}
} // namespace

TEST(ToolIcon, OffUsesMutedTextOnUsesHighlightDisabledUsesDisabledText) {
    const QPalette light = makePalette("#889099", "#1a73e8", "#b0b0b0");
    EXPECT_EQ(ToolIcon::stateColor(light, ToolIcon::State::Off), QColor("#889099"));
    EXPECT_EQ(ToolIcon::stateColor(light, ToolIcon::State::On), QColor("#1a73e8"));
    EXPECT_EQ(ToolIcon::stateColor(light, ToolIcon::State::Disabled), QColor("#b0b0b0"));
}

TEST(ToolIcon, NormalUsesTheRegularTextColor) {
    // 开始、停止这类动作按钮：可点时用普通文字色，既不是弱化色也不是强调色
    QPalette p = makePalette("#889099", "#1a73e8", "#b0b0b0");
    p.setColor(QPalette::Active, QPalette::ButtonText, QColor("#202124"));
    EXPECT_EQ(ToolIcon::stateColor(p, ToolIcon::State::Normal), QColor("#202124"));
}

TEST(ToolIcon, ColorsFollowThePaletteOfEachTheme) {
    const QPalette dark = makePalette("#7a7f87", "#4da3ff", "#555555");
    EXPECT_EQ(ToolIcon::stateColor(dark, ToolIcon::State::Off), QColor("#7a7f87"));
    EXPECT_EQ(ToolIcon::stateColor(dark, ToolIcon::State::On), QColor("#4da3ff"));
    EXPECT_NE(ToolIcon::stateColor(dark, ToolIcon::State::On),
              ToolIcon::stateColor(makePalette("#7a7f87", "#1a73e8", "#555555"), ToolIcon::State::On));
}

TEST(ToolIcon, RenderedStrokesTakeTheRequestedColor) {
    const QImage img = ToolIcon::render(iconPath("tool_stop"), QColor("#ff8800"), 48);
    ASSERT_FALSE(img.isNull());
    EXPECT_EQ(img.width(), 48);
    EXPECT_EQ(img.height(), 48);

    const QImage px = img.convertToFormat(QImage::Format_ARGB32);   // 非预乘，才能直接比较 RGB
    int solid = 0;
    for (int y = 0; y < px.height(); ++y) {
        for (int x = 0; x < px.width(); ++x) {
            const QColor c = px.pixelColor(x, y);
            if (c.alpha() < 200) continue;
            ++solid;
            // 预乘 alpha 换回来会差 1 到 2
            EXPECT_NEAR(c.red(), 0xff, 2);
            EXPECT_NEAR(c.green(), 0x88, 2);
            EXPECT_NEAR(c.blue(), 0x00, 2);
        }
    }
    EXPECT_GT(solid, 20);   // 确实画出了线条，不是空图
}

TEST(ToolIcon, SameIconRendersInADifferentColorOnRequest) {
    const QImage a = ToolIcon::render(iconPath("tool_stop"), QColor("#ff0000"), 32).convertToFormat(QImage::Format_ARGB32);
    const QImage b = ToolIcon::render(iconPath("tool_stop"), QColor("#0000ff"), 32).convertToFormat(QImage::Format_ARGB32);
    bool foundRed = false, foundBlue = false;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            if (a.pixelColor(x, y).alpha() > 200) foundRed |= a.pixelColor(x, y).red() == 255;
            if (b.pixelColor(x, y).alpha() > 200) foundBlue |= b.pixelColor(x, y).blue() == 255;
        }
    }
    EXPECT_TRUE(foundRed);
    EXPECT_TRUE(foundBlue);
}

TEST(ToolIcon, EveryToolbarIconFileExistsAndRenders) {
    for (const QString& name : kToolIcons) {
        const QImage img = ToolIcon::render(iconPath(name), QColor("#000000"), 24);
        EXPECT_FALSE(img.isNull()) << name.toStdString();
        bool anyInk = false;
        for (int y = 0; y < img.height() && !anyInk; ++y)
            for (int x = 0; x < img.width() && !anyInk; ++x)
                anyInk = img.pixelColor(x, y).alpha() > 0;
        EXPECT_TRUE(anyInk) << name.toStdString();
    }
}

TEST(ToolIcon, MissingFileGivesNullImage) {
    EXPECT_TRUE(ToolIcon::render(iconPath("no_such_icon"), QColor("#000000"), 24).isNull());
}

// 人工查看用：把十个图标按浅色/深色底各画一行，输出到环境变量 IME_AUDIO_ICON_PREVIEW 指定的 PNG。
// 默认不运行（DISABLED_），需要时加 --gtest_also_run_disabled_tests --gtest_filter=*WritePreviewSheet
#include <QPainter>
TEST(ToolIcon, DISABLED_WritePreviewSheet) {
    const QString out = qEnvironmentVariable("IME_AUDIO_ICON_PREVIEW");
    ASSERT_FALSE(out.isEmpty());
    constexpr int cell = 72, size = 40;
    QImage sheet(cell * kToolIcons.size(), cell * 2, QImage::Format_ARGB32);
    struct Theme { QColor bg, off, on; };
    const Theme themes[2] = { { QColor("#f5f5f5"), QColor("#7a828c"), QColor("#1a73e8") },
                              { QColor("#202225"), QColor("#8a9099"), QColor("#4da3ff") } };
    QPainter p(&sheet);
    for (int row = 0; row < 2; ++row) {
        p.fillRect(0, row * cell, sheet.width(), cell, themes[row].bg);
        for (int i = 0; i < kToolIcons.size(); ++i) {
            const QColor c = (i == 3 || i == 4 || i == 5) ? themes[row].on : themes[row].off;   // 三个开关显示"开启"色
            p.drawImage(i * cell + (cell - size) / 2, row * cell + (cell - size) / 2,
                        ToolIcon::render(iconPath(kToolIcons[i]), c, size));
        }
    }
    p.end();
    ASSERT_TRUE(sheet.save(out));
}

// ---- 横条配色：开始键用录音软件惯用的红色；三个开关各有自己的开启色 ----

TEST(ToolIconColors, RecordIsAFixedRedInEveryTheme) {
    const QColor red("#e5484d");
    EXPECT_EQ(ToolIcon::stateColor(makePalette("#889099", "#1a73e8", "#b0b0b0"), ToolIcon::State::Record), red);
    EXPECT_EQ(ToolIcon::stateColor(makePalette("#7a7f87", "#4da3ff", "#555555"), ToolIcon::State::Record), red);
}
