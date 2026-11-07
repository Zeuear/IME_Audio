#include <gtest/gtest.h>
#include <QApplication>
#include <QCheckBox>
#include <QToolButton>

#include "ui/controls/ToggleLink.h"

// QApplication 由 support/widget_app_environment.cpp 里的全局环境提供

namespace {
QToolButton* makeBarButton()
{
    auto* b = new QToolButton;
    b->setCheckable(true);
    return b;
}
} // namespace

TEST(ToggleLink, ClickingTheBarButtonDrivesTheSettingsCheckBox) {
    QCheckBox box;
    QToolButton* button = makeBarButton();
    ToggleLink::connect(button, &box);

    button->click();
    EXPECT_TRUE(box.isChecked());
    button->click();
    EXPECT_FALSE(box.isChecked());
    delete button;
}

TEST(ToggleLink, ChangingTheCheckBoxUpdatesTheBarButton) {
    QCheckBox box;
    QToolButton* button = makeBarButton();
    ToggleLink::connect(button, &box);

    box.setChecked(true);
    EXPECT_TRUE(button->isChecked());
    box.setChecked(false);
    EXPECT_FALSE(button->isChecked());
    delete button;
}

TEST(ToggleLink, InitialStateFollowsTheCheckBox) {
    QCheckBox box;
    box.setChecked(true);
    QToolButton* button = makeBarButton();
    ToggleLink::connect(button, &box);
    EXPECT_TRUE(button->isChecked());
    EXPECT_TRUE(button->isEnabled());
    delete button;
}

TEST(ToggleLink, EachChangeNotifiesTheCheckBoxExactlyOnce) {
    // 两边互相更新不能来回弹：设置里的开关一次变化只写一次配置
    QCheckBox box;
    QToolButton* button = makeBarButton();
    ToggleLink::connect(button, &box);
    int boxToggled = 0, buttonToggled = 0;
    QObject::connect(&box, &QCheckBox::toggled, &box, [&](bool) { ++boxToggled; });
    QObject::connect(button, &QToolButton::toggled, button, [&](bool) { ++buttonToggled; });

    button->click();
    EXPECT_EQ(boxToggled, 1);
    EXPECT_EQ(buttonToggled, 1);

    box.setChecked(false);
    EXPECT_EQ(boxToggled, 2);
    EXPECT_EQ(buttonToggled, 2);
    delete button;
}

TEST(ToggleLink, DisabledCheckBoxDisablesTheBarButtonToo) {
    QCheckBox box;
    QToolButton* button = makeBarButton();
    ToggleLink::connect(button, &box);

    box.setEnabled(false);
    EXPECT_FALSE(button->isEnabled());
    box.setEnabled(true);
    EXPECT_TRUE(button->isEnabled());
    delete button;
}

TEST(ToggleLink, ButtonIsDisabledAlreadyWhenLinkedToADisabledCheckBox) {
    QCheckBox box;
    box.setEnabled(false);
    QToolButton* button = makeBarButton();
    ToggleLink::connect(button, &box);
    EXPECT_FALSE(button->isEnabled());
    delete button;
}
