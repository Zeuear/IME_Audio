#include <gtest/gtest.h>
#include <QPoint>
#include <QRect>

#include "ui/delegates/ModelInfoDelegate.h"
#include "ui/models/ModelDownloadTracker.h"

using Action = ModelInfoDelegate::Action;
using State = ModelDownloadTracker::State;

namespace {
const QRect kRow(0, 100, 700, 58);
QPoint insideAction() { return ModelInfoDelegate::actionRect(kRow).center(); }
QPoint onModelName() { return ModelInfoDelegate::columns(kRow).name.center(); }
}

TEST(ModelRowAction, ClickingTheActionWhileDownloadingCancels) {
    EXPECT_EQ(ModelInfoDelegate::actionAt(kRow, insideAction(), State::Downloading), Action::Cancel);
}

TEST(ModelRowAction, ClickingTheActionAfterAFailureRetries) {
    EXPECT_EQ(ModelInfoDelegate::actionAt(kRow, insideAction(), State::Failed), Action::Retry);
}

TEST(ModelRowAction, NoActionWhenNothingIsHappeningOrWhileExtracting) {
    // 解压中不能取消；没有下载任务时那一格是"已安装"之类的特性文字，不是按钮
    EXPECT_EQ(ModelInfoDelegate::actionAt(kRow, insideAction(), State::None), Action::None);
    EXPECT_EQ(ModelInfoDelegate::actionAt(kRow, insideAction(), State::Extracting), Action::None);
}

TEST(ModelRowAction, ClickingElsewhereOnTheRowIsNotAnAction) {
    // 点模型名所在位置只是选中这个模型
    EXPECT_EQ(ModelInfoDelegate::actionAt(kRow, onModelName(), State::Downloading), Action::None);
    EXPECT_EQ(ModelInfoDelegate::actionAt(kRow, onModelName(), State::Failed), Action::None);
}

TEST(ModelRowAction, ActionAreaIsInsideTheRowAndInTheFeatureColumn) {
    const QRect a = ModelInfoDelegate::actionRect(kRow);
    EXPECT_TRUE(kRow.contains(a));
    EXPECT_TRUE(ModelInfoDelegate::columns(kRow).features.contains(a));
}

TEST(ModelRowAction, WorksOnNarrowRowsToo) {
    const QRect narrow(0, 0, 420, 58);
    const QRect a = ModelInfoDelegate::actionRect(narrow);
    EXPECT_TRUE(narrow.contains(a));
    EXPECT_EQ(ModelInfoDelegate::actionAt(narrow, a.center(), State::Failed), Action::Retry);
}
