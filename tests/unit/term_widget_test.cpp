#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QListView>
#include <QPushButton>
#include <QTemporaryDir>

#include "services/dictionary/DictionaryProfiles.h"
#include "ui/pages/TermWidget.h"

// 词典页左侧「语音识别 / 翻译」两个标签各有自己的配置列表；切回标签时右侧必须显示该标签选中配置的词条。
// QApplication 由 support/widget_app_environment.cpp 提供。

namespace {

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
