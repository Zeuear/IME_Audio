#include <gtest/gtest.h>

#include "services/UpdateManager.h"
#include "ui/models/UpdateStatusModel.h"

// ---- 版本比较：只有真的更新才算有新版本 ----

TEST(UpdateVersion, NewerPatchMinorAndMajorAreNewer) {
    EXPECT_TRUE(UpdateManager::isNewer("1.2.1", "1.2.0"));
    EXPECT_TRUE(UpdateManager::isNewer("1.3.0", "1.2.9"));
    EXPECT_TRUE(UpdateManager::isNewer("2.0.0", "1.9.9"));
}

TEST(UpdateVersion, EqualOrOlderIsNotNewer) {
    EXPECT_FALSE(UpdateManager::isNewer("1.2.0", "1.2.0"));
    EXPECT_FALSE(UpdateManager::isNewer("1.1.9", "1.2.0"));
}

TEST(UpdateVersion, LeadingVIsIgnoredOnEitherSide) {
    EXPECT_TRUE(UpdateManager::isNewer("v1.2.1", "1.2.0"));
    EXPECT_TRUE(UpdateManager::isNewer("1.2.1", "v1.2.0"));
    EXPECT_FALSE(UpdateManager::isNewer("V1.2.0", "v1.2.0"));
}

TEST(UpdateVersion, ComparesNumericallyNotAsText) {
    // 按字符串比 "1.10.0" < "1.9.0"，按版本号比才对
    EXPECT_TRUE(UpdateManager::isNewer("1.10.0", "1.9.0"));
    EXPECT_FALSE(UpdateManager::isNewer("1.9.0", "1.10.0"));
}

TEST(UpdateVersion, UnparsableVersionsFallBackToTextComparison) {
    // 和原来的行为一致：解析不出版本号时按字符串比
    EXPECT_TRUE(UpdateManager::isNewer("beta-b", "beta-a"));
    EXPECT_FALSE(UpdateManager::isNewer("beta-a", "beta-b"));
}

// ---- 关于页的更新状态：文字、按钮、进度条 ----

using State = UpdateStatusModel::State;

TEST(UpdateStatus, StartsIdleWithNothingShown) {
    UpdateStatusModel m;
    const auto v = m.view();
    EXPECT_EQ(m.state(), State::Idle);
    EXPECT_TRUE(v.text.isEmpty());
    EXPECT_TRUE(v.checkEnabled);
    EXPECT_FALSE(v.installVisible);
    EXPECT_FALSE(v.progressVisible);
}

TEST(UpdateStatus, CheckingDisablesTheCheckButton) {
    UpdateStatusModel m;
    m.startCheck();
    EXPECT_EQ(m.state(), State::Checking);
    EXPECT_FALSE(m.view().checkEnabled);
    EXPECT_EQ(m.view().text, "Checking for updates...");
}

TEST(UpdateStatus, UpToDateShowsTheCurrentVersion) {
    UpdateStatusModel m;
    m.startCheck();
    m.upToDate("1.4.0");
    EXPECT_EQ(m.state(), State::UpToDate);
    EXPECT_TRUE(m.view().checkEnabled);
    EXPECT_FALSE(m.view().installVisible);
    EXPECT_EQ(m.view().text, "You are on the latest version (1.4.0)");
}

TEST(UpdateStatus, AvailableOffersInstallAndKeepsCheckUsable) {
    UpdateStatusModel m;
    m.startCheck();
    m.available("1.5.0");
    EXPECT_EQ(m.state(), State::Available);
    EXPECT_TRUE(m.view().installVisible);
    EXPECT_TRUE(m.view().checkEnabled);
    EXPECT_EQ(m.view().text, "New version 1.5.0 is available");
}

TEST(UpdateStatus, FailureShowsTheReasonAndAllowsRetry) {
    UpdateStatusModel m;
    m.startCheck();
    m.failed("Host not found");
    EXPECT_EQ(m.state(), State::Failed);
    EXPECT_TRUE(m.view().checkEnabled);
    EXPECT_FALSE(m.view().installVisible);
    EXPECT_EQ(m.view().text, "Check failed: Host not found");
}

TEST(UpdateStatus, AutomaticStartupCheckResultsStayInvisible) {
    // 启动时的自动检查没有经过关于页，结果到达时关于页什么都不该显示（也不能弹"已是最新"）
    UpdateStatusModel m;
    m.upToDate("1.4.0");
    EXPECT_EQ(m.state(), State::Idle);
    m.available("1.5.0");
    EXPECT_EQ(m.state(), State::Idle);
    m.failed("x");
    EXPECT_EQ(m.state(), State::Idle);
    EXPECT_TRUE(m.view().text.isEmpty());
}

TEST(UpdateStatus, IsManualCheckOnlyWhileTheUserIsDrivingIt) {
    // 宿主据此决定要不要再弹自动检查的那个对话框：页面自己在显示结果时就不弹，避免重复
    UpdateStatusModel m;
    EXPECT_FALSE(m.isUserDriven());
    m.startCheck();
    EXPECT_TRUE(m.isUserDriven());
    m.available("1.5.0");
    EXPECT_TRUE(m.isUserDriven());
    m.startDownload();
    EXPECT_TRUE(m.isUserDriven());
}

TEST(UpdateStatus, DownloadShowsProgressInPercent) {
    UpdateStatusModel m;
    m.startCheck();
    m.available("1.5.0");
    m.startDownload();
    EXPECT_EQ(m.state(), State::Downloading);
    EXPECT_TRUE(m.view().progressVisible);
    EXPECT_FALSE(m.view().checkEnabled);
    EXPECT_FALSE(m.view().installVisible);

    m.progress(25, 100);
    EXPECT_EQ(m.view().percent, 25);
    m.progress(1500, 6000);
    EXPECT_EQ(m.view().percent, 25);
    m.progress(6000, 6000);
    EXPECT_EQ(m.view().percent, 100);
}

TEST(UpdateStatus, UnknownTotalSizeMeansIndeterminateProgress) {
    UpdateStatusModel m;
    m.startCheck();
    m.available("1.5.0");
    m.startDownload();
    m.progress(500, 0);
    EXPECT_EQ(m.view().percent, -1);
    m.progress(500, -1);
    EXPECT_EQ(m.view().percent, -1);
}

TEST(UpdateStatus, ProgressNeverLeavesZeroToHundred) {
    UpdateStatusModel m;
    m.startCheck();
    m.available("1.5.0");
    m.startDownload();
    m.progress(200, 100);
    EXPECT_EQ(m.view().percent, 100);
    m.progress(-5, 100);
    EXPECT_EQ(m.view().percent, 0);
}

TEST(UpdateStatus, FinishingTheDownloadHidesTheProgressBar) {
    UpdateStatusModel m;
    m.startCheck();
    m.available("1.5.0");
    m.startDownload();
    m.progress(50, 100);
    m.downloadFinished();
    EXPECT_FALSE(m.view().progressVisible);
    EXPECT_TRUE(m.view().checkEnabled);
    EXPECT_EQ(m.view().text, "Update downloaded");
}

TEST(UpdateStatus, ADownloadErrorGoesToFailedAndAllowsRetry) {
    UpdateStatusModel m;
    m.startCheck();
    m.available("1.5.0");
    m.startDownload();
    m.failed("Connection reset");
    EXPECT_EQ(m.state(), State::Failed);
    EXPECT_FALSE(m.view().progressVisible);
    EXPECT_TRUE(m.view().checkEnabled);
}

TEST(UpdateStatus, CheckingAgainClearsThePreviousResult) {
    UpdateStatusModel m;
    m.startCheck();
    m.available("1.5.0");
    m.startCheck();
    EXPECT_EQ(m.state(), State::Checking);
    EXPECT_FALSE(m.view().installVisible);
}
