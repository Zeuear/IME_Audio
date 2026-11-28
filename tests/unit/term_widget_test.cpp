#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QDialogButtonBox>
#include <QEnterEvent>
#include <QListView>
#include <QPushButton>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include "services/dictionary/DictionaryLanguages.h"
#include "services/dictionary/DictionaryProfiles.h"
#include "ui/controls/BatchImportDialog.h"
#include "ui/pages/TermWidget.h"

// 词典页左侧「语音识别 / 翻译」两个标签各有自己的配置列表；切回标签时右侧必须显示该标签选中配置的词条。
// QApplication 由 support/widget_app_environment.cpp 提供。

namespace {

// 不引入 Qt6::Test：只需要让动画和计时器跑一会儿
// 动画按真实时间推进，CI 机器忙时固定等待时长不稳，所以改成最多等 timeoutMs、条件成立就返回
template <typename Pred>
bool waitUntil(Pred done, int timeoutMs = 4000) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    return done();
}

QPushButton* tabButton(TermWidget& w, const QString& text) {
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b->text() == text) return b;
    }
    return nullptr;
}

// 右侧词条列表挂着代理模型，左侧配置列表直接挂 DictionaryProfileModel
int visibleTermRows(TermWidget& w) {
    for (QListView* v : w.findChildren<QListView*>()) {
        if (v->model() && v->model()->inherits("QSortFilterProxyModel")) return v->model()->rowCount();
    }
    return -1;
}

void addTerms(TermsLibraryManager* lib, int n, const QString& prefix) {
    for (int i = 0; i < n; ++i) {
        TermItem item;
        item.wrong = QStringLiteral("%1w%2").arg(prefix).arg(i);
        item.correct = QStringLiteral("%1c%2").arg(prefix).arg(i);
        lib->addItem(item);
    }
}

}

TEST(TermWidgetTest, SwitchingBackToRecognitionShowsDefaultDictionaryTerms) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    DictionaryProfiles profiles(dir.filePath("dictionaries"), dir.filePath("terms.tsv"));
    addTerms(profiles.library(DictionaryProfiles::defaultId()), 3, "r");
    const QString translationId = profiles.addProfile(DictionaryKind::Translation, "es", "zh", "es");
    addTerms(profiles.library(translationId), 5, "t");

    TermWidget widget;
    widget.setDictionaryProfiles(&profiles);
    QPushButton* recognition = tabButton(widget, QStringLiteral("Recognition"));
    QPushButton* translation = tabButton(widget, QStringLiteral("Translation"));
    ASSERT_NE(recognition, nullptr);
    ASSERT_NE(translation, nullptr);

    EXPECT_EQ(visibleTermRows(widget), 3);
    translation->click();
    EXPECT_EQ(visibleTermRows(widget), 5);
    recognition->click();
    EXPECT_EQ(visibleTermRows(widget), 3) << "切回语音识别后仍显示翻译词典的词条";
    translation->click();
    EXPECT_EQ(visibleTermRows(widget), 5);
}

TEST(BatchImportDialogTest, RecognisesLinesAndEnablesImportOnlyWithEntries) {
    BatchImportDialog dialog(true, QStringLiteral("zh"), QStringLiteral("en"));
    auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    EXPECT_FALSE(ok->isEnabled());
    dialog.setText(QStringLiteral("显卡 => GPU\n处理器 -> CPU\nbad line"));
    EXPECT_EQ(dialog.entries().size(), 2);
    EXPECT_TRUE(ok->isEnabled());
}

TEST(BatchImportDialogTest, SuggestsSwappingWhenColumnsAreBackwards) {
    BatchImportDialog dialog(true, QStringLiteral("zh"), QStringLiteral("bg"));
    dialog.setText(QStringLiteral("Бог => 神\nИсус => 耶稣\nСветия Дух => 圣灵"));
    EXPECT_TRUE(dialog.swapColumns());
    dialog.setText(QStringLiteral("神 => Бог\n耶稣 => Исус"));
    EXPECT_FALSE(dialog.swapColumns());
}

TEST(DictionaryLanguagesTest, CodeFromNameAcceptsChineseEnglishAndCodes) {
    EXPECT_EQ(DictionaryLanguages::codeFromName(QStringLiteral("中文")), "zh");
    EXPECT_EQ(DictionaryLanguages::codeFromName(QStringLiteral("保加利亚语")), "bg");
    EXPECT_EQ(DictionaryLanguages::codeFromName(QStringLiteral("保加利亚文")), "bg");
    EXPECT_EQ(DictionaryLanguages::codeFromName(QStringLiteral("Bulgarian")), "bg");
    EXPECT_EQ(DictionaryLanguages::codeFromName(QStringLiteral("bg")), "bg");
    EXPECT_EQ(DictionaryLanguages::codeFromName(QStringLiteral("西班牙语")), "es");
    EXPECT_TRUE(DictionaryLanguages::codeFromName(QStringLiteral("火星语")).isEmpty());
}

// 鼠标移到「添加」上才滑出「批量导入」，移开后收回
TEST(TermWidgetTest, BatchImportButtonSlidesOutOnHoverOnly) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    DictionaryProfiles profiles(dir.filePath("dictionaries"), dir.filePath("terms.tsv"));
    TermWidget widget;
    widget.setDictionaryProfiles(&profiles);
    widget.resize(900, 500);
    widget.show();

    QPushButton* batch = tabButton(widget, QStringLiteral("Batch Import"));
    ASSERT_NE(batch, nullptr);
    EXPECT_FALSE(batch->isVisible());

    QWidget* group = batch->parentWidget();
    QEnterEvent enter(QPointF(5, 5), QPointF(5, 5), QPointF(5, 5));
    QCoreApplication::sendEvent(group, &enter);
    EXPECT_TRUE(waitUntil([&] { return batch->width() > 20; }))
        << "width=" << batch->width() << " maxWidth=" << batch->maximumWidth() << " visible=" << batch->isVisible();
    EXPECT_TRUE(batch->isVisible());

    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(group, &leave);
    EXPECT_TRUE(waitUntil([&] { return !batch->isVisible(); }))
        << "width=" << batch->width() << " maxWidth=" << batch->maximumWidth();
}
