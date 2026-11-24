#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QFile>
#include <QSet>

#include "services/hotword/HotwordSelector.h"
#include "services/hotword/PinyinTable.h"
#include "sherpa/HotwordPrompt.h"
#include "ui/models/LiveCaptionModel.h"

// 内置拼音表直接读源文件（和打进资源的是同一份），不依赖 Qt 资源系统
static const PinyinTable& realTable() {
    static const PinyinTable table = []() {
        QFile f(QStringLiteral(IME_AUDIO_RESOURCES_DIR "/data/pinyin_table.txt"));
        EXPECT_TRUE(f.open(QIODevice::ReadOnly));
        return PinyinTable::fromText(QString::fromUtf8(f.readAll()));
    }();
    return table;
}

TEST(PinyinTable, CoversCommonAndRareNameCharacters) {
    const PinyinTable& t = realTable();
    EXPECT_GT(t.charCount(), 20000);
    EXPECT_EQ(t.syllable(QChar(u'王')), "wang");
    EXPECT_EQ(t.syllable(QChar(u'韬')), "tao");
    EXPECT_EQ(t.syllable(QChar(u'仉')), "zhang");   // 生僻姓氏也要收
    // 多音字：主读音之外的读音也要留着。隗做姓读 wěi，字典里主读音是 kuí
    EXPECT_TRUE(t.syllable(QChar(u'隗')) == "wei" || t.alternates(QChar(u'隗')).contains("wei"));
    EXPECT_TRUE(t.alternates(QChar(u'缪')).contains("miao") || t.syllable(QChar(u'缪')) == "miao");
    EXPECT_TRUE(t.syllable(QChar(u'a')).isEmpty()); // 不是汉字
    EXPECT_TRUE(t.syllable(QChar(u'，')).isEmpty());
}

TEST(PinyinTable, FromTextParsesSyllableLines) {
    const auto t = PinyinTable::fromText("wang 王汪\nli 李\n");
    EXPECT_EQ(t.syllable(QChar(u'汪')), "wang");
    EXPECT_EQ(t.syllable(QChar(u'李')), "li");
    EXPECT_TRUE(t.syllable(QChar(u'张')).isEmpty());
    EXPECT_EQ(t.charCount(), 3);
}

namespace {
HotwordSelector makeSelector(const QList<HotwordSelector::Entry>& entries) {
    HotwordSelector s(&realTable());
    s.setEntries(entries);
    return s;
}
}

TEST(HotwordSelector, PicksTermWhoseSoundMatchesAMisheardSentence) {
    auto s = makeSelector({ {"邝泳琪", {}}, {"华夏龙芯", {}}, {"蔚来中心", {}}, {"曹俪娜", {}}, {"讯飞星火", {}} });
    // 流式模型把「邝泳琪」听成「匡永琪」，把「蔚来」听成「未来」
    const QStringList got = s.select(QStringLiteral("请把这份报告发给匡永琪他负责华夏龙芯的项目"));
    EXPECT_EQ(QSet<QString>(got.begin(), got.end()), (QSet<QString>{"华夏龙芯", "邝泳琪"}));   // 两个都命中，没有多选
    EXPECT_EQ(s.select(QStringLiteral("会议室预定在鄂尔多斯路的未来中心")), (QStringList{"蔚来中心"}));
}

TEST(HotwordSelector, ToleratesFrontBackNasalAndRetroflexConfusions) {
    auto s = makeSelector({ {"赛迪思", {}}, {"潍柴动力", {}} });
    EXPECT_EQ(s.select(QStringLiteral("请检查一下赛迪斯的订单")), (QStringList{"赛迪思"}));   // si / si
    EXPECT_EQ(s.select(QStringLiteral("确认为柴动力的交付时间")), (QStringList{"潍柴动力"}));  // 少听一个字也算
}

TEST(HotwordSelector, ReturnsNothingWhenNothingSoundsLike) {
    auto s = makeSelector({ {"邝泳琪", {}}, {"Kubernetes", {}}, {"澜起科技", {}} });
    EXPECT_TRUE(s.select(QStringLiteral("明天早上九点在三楼会议室开周会")).isEmpty());
    EXPECT_TRUE(s.select(QString()).isEmpty());
}

TEST(HotwordSelector, MatchesLatinWordsCaseInsensitivelyAndTolerantOfOneLetter) {
    auto s = makeSelector({ {"Kubernetes", {}}, {"PyTorch", {}}, {"Redis", {}} });
    EXPECT_EQ(s.select(QStringLiteral("服务迁移到 KUBERNETES 集群上")), (QStringList{"Kubernetes"}));
    EXPECT_EQ(s.select(QStringLiteral("服务迁移到 Kubernetis 集群上")), (QStringList{"Kubernetes"}));
    EXPECT_TRUE(s.select(QStringLiteral("今天天气不错")).isEmpty());
}

TEST(HotwordSelector, HeardAliasInTheSentenceSelectsTheCorrectSpelling) {
    auto s = makeSelector({ {"Kubernetes", {"开八斯", "k8s"}} });
    EXPECT_EQ(s.select(QStringLiteral("我们用 k8s 部署")), (QStringList{"Kubernetes"}));
    EXPECT_EQ(s.select(QStringLiteral("我们用开八斯部署")), (QStringList{"Kubernetes"}));
}

TEST(HotwordSelector, CapsCountAndKeepsBestFirst) {
    QList<HotwordSelector::Entry> es;
    // 尼涛：ni/li 只差 n/l 不分，扣 0.3；李涛读音完全一致。词典里尼涛在前，结果仍要把更像的排前面
    for (const QString& w : {"尼涛", "李涛", "李党"}) es.push_back({w, {}});
    auto s = makeSelector(es);
    EXPECT_EQ(s.select(QStringLiteral("请联系李涛")), (QStringList{"李涛", "尼涛"}));
    EXPECT_EQ(s.select(QStringLiteral("请联系李涛"), 1), (QStringList{"李涛"}));
    EXPECT_TRUE(s.select(QStringLiteral("请联系李涛"), 0).isEmpty());
}

TEST(HotwordSelector, SingleChineseCharacterTermsAreIgnoredAsTooNoisy) {
    auto s = makeSelector({ {"李", {}} });
    EXPECT_TRUE(s.select(QStringLiteral("请联系李先生")).isEmpty());
}

TEST(HotwordSelector, LargeDictionaryStaysFast) {
    // 3000 条随机常用字组成的 2~4 字词：比"全是生造词"更接近真实词典，粗筛挡不住多少
    const QString pool = QStringLiteral("王李张刘陈杨黄赵吴周徐孙马朱胡郭何高林罗郑梁谢宋唐许韩冯邓曹彭曾萧田董潘袁蔡蒋余于杜叶程魏苏吕丁任沈姚卢姜崔钟谭陆汪范金石廖贾夏韦付方白邹孟熊秦邱江尹薛闫段雷侯龙史陶黎贺顾毛郝龚邵万钱严覃武戴莫孔向汤");
    QList<HotwordSelector::Entry> es;
    quint32 seed = 12345;
    auto next = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    for (int i = 0; i < 3000; ++i) {
        QString w;
        for (int k = 0, n = 2 + int(next() % 3); k < n; ++k) w += pool[int(next() % pool.size())];
        es.push_back({ w, {} });
    }
    es.push_back({"邝泳琪", {}});
    auto s = makeSelector(es);
    QElapsedTimer t; t.start();
    const QStringList r = s.select(QStringLiteral("请把这份报告发给匡永琪他负责华夏龙芯的项目王李张刘陈杨黄赵吴周徐孙马朱"));
    // 一句话在 UI 线程上挑词，必须是几十毫秒量级，不能拖慢听写
    EXPECT_LT(t.elapsed(), 150) << "elapsed ms: " << t.elapsed();
    EXPECT_TRUE(r.contains("邝泳琪"));
}

TEST(HotwordPrompt, BuildFromRankedListKeepsOrderAndCaps) {
    EXPECT_EQ(HotwordPrompt::build(QStringList{"邝泳琪", "华夏龙芯"}), "邝泳琪,华夏龙芯");
    QStringList many;
    for (int i = 0; i < 50; ++i) many << QString("词%1").arg(i);
    EXPECT_EQ(HotwordPrompt::count(HotwordPrompt::build(many)), HotwordPrompt::kMaxWords);
}

TEST(LiveCaptionPending, ReturnsOnlyTheSentenceJustCutForTranscription) {
    LiveCaptionModel m;
    EXPECT_TRUE(m.pendingText().isEmpty());
    m.setSpeaking(true);
    m.setPartial(m.epoch(), QStringLiteral("请联系匡永琪"));
    EXPECT_TRUE(m.pendingText().isEmpty());       // 还在说，没切出
    m.onUtteranceReady(1);
    EXPECT_EQ(m.pendingText(), QStringLiteral("请联系匡永琪"));
    m.onTranscribed(1, QStringLiteral("请联系邝泳琪"));
    EXPECT_TRUE(m.pendingText().isEmpty());       // 出了正式结果就不再是待转录的那句
}

TEST(HotwordSelector, HeteronymSurnameMatchesItsRareReading) {
    // 「隗」主读音 kui，姓氏读 wei；流式识别按姓氏听成「韦博士」时词条仍要命中
    auto s = makeSelector({ {"隗博士", {}} });
    EXPECT_EQ(s.select(QStringLiteral("我想预约韦博士下午的门诊")), (QStringList{"隗博士"}));
}
