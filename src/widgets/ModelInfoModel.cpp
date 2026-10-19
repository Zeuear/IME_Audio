#include "ModelInfoModel.h"
#include "../sherpa/SherpaConfig.h"
#include "../sherpa/SherpaManager.h"
#include "../utils/Logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
#include <iterator>

namespace {
const QJsonObject& benchmarks()
{
    static const QJsonObject langs = [] {
        QFile f(QStringLiteral(":/model_benchmarks.json"));
        if (!f.open(QIODevice::ReadOnly)) {
            LOG_WARN("Model benchmark data not found in resources");
            return QJsonObject{};
        }
        return QJsonDocument::fromJson(f.readAll()).object().value("languages").toObject();
    }();
    return langs;
}

// 实测得分低于它的模型视为不可用（错误率 60% 以上，听写基本是乱码），不放进列表。
// 抽查过原始输出：这些模型在对应语种上确实输出乱码或串成别的语言，不是测试出错
constexpr double kMinUsableScore = 40.0;

double clamp(double v, double lo, double hi)
{
    return qBound(lo, v, hi);
}

double roundTenth(double v)
{
    return qRound(v * 10.0) / 10.0;
}
}

ModelInfoModel::ModelInfoModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ModelInfoModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ModelInfoModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) return {};
    const ModelInfo& m = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:     return m.displayName;
    case RepoIdRole:          return m.repoId;
    case LanguagesRole:       return m.languages;
    case InstalledRole:       return m.installed;
    case BuiltinPunctRole:    return m.builtinPunct;
    case BenchmarkedRole:     return m.benchmarked;
    case ScoreRole:           return m.score;
    case RtfRole:             return m.rtf;
    case MetricRole:          return m.metric;
    case AccuracyRatingRole:  return m.benchmarked ? accuracyRating(m.score) : 0.0;
    case SpeedRatingRole:     return m.benchmarked ? speedRating(m.rtf) : 0.0;
    case AccuracyLevelRole:   return m.benchmarked ? qRound(accuracyRating(m.score)) : 0;
    case SpeedLevelRole:      return m.benchmarked ? qRound(speedRating(m.rtf)) : 0;
    default:                  return {};
    }
}

void ModelInfoModel::setLanguage(const QString& language)
{
    beginResetModel();
    m_language = language;
    m_items.clear();
    const QJsonObject scores = benchmarks().value(language).toObject();
    // 某模型在某语种下是否因得分过低被排除；没有基准数据的不排除
    auto tooPoor = [](const QJsonObject& langScores, const QString& repoId) {
        const QJsonObject s = langScores.value(repoId).toObject();
        return !s.isEmpty() && s.value("score").toDouble() < kMinUsableScore;
    };
    QList<ModelInfo> poor;
    for (const ModelEntry& e : ModelRegistry::GetModelEntriesByLanguage(language)) {
        // 注册表里有空白占位的 repoId，跳过
        if (e.repoPath.trimmed().isEmpty()) continue;
        ModelInfo m;
        m.displayName = e.displayName;
        m.repoId = e.repoPath;
        for (const QString& lang : ModelRegistry::GetLanguagesByRepo(e.repoPath)) {
            if (lang == language || !tooPoor(benchmarks().value(lang).toObject(), e.repoPath)) m.languages << lang;
        }
        m.installed = SherpaInstaller::isInstalled(e.repoPath);
        if (const ModelDescriptor* d = ModelRegistry::Find(e.repoPath)) m.builtinPunct = d->hasBuiltinPunctuation;
        const QJsonObject s = scores.value(e.repoPath).toObject();
        if (!s.isEmpty()) {
            m.benchmarked = true;
            m.score = s.value("score").toDouble();
            m.rtf = s.value("rtf").toDouble();
            m.metric = s.value("metric").toString();
        }
        if (tooPoor(scores, e.repoPath)) {
            poor.push_back(m);
            continue;
        }
        m.rank = m_items.size() + poor.size();
        m_items.push_back(m);
    }
    if (m_items.isEmpty() && !poor.isEmpty()) {
        m_items.push_back(*std::max_element(poor.begin(), poor.end(),
                                            [](const ModelInfo& a, const ModelInfo& b) { return a.score < b.score; }));
    }
    endResetModel();
}

void ModelInfoModel::setSort(SortKey key, Qt::SortOrder order)
{
    beginResetModel();
    // 先回到原始顺序，再稳定排序：并列的模型保持注册表里的先后
    std::sort(m_items.begin(), m_items.end(), [](const ModelInfo& a, const ModelInfo& b) { return a.rank < b.rank; });
    if (key != SortKey::Default) {
        const bool bestFirst = order == Qt::DescendingOrder;
        std::stable_sort(m_items.begin(), m_items.end(), [&](const ModelInfo& a, const ModelInfo& b) {
            if (a.benchmarked != b.benchmarked) return a.benchmarked;
            if (!a.benchmarked) return false;
            // 速度用 RTF 比较：越小越快，所以“最好在前”对应升序
            const double x = key == SortKey::Accuracy ? a.score : a.rtf;
            const double y = key == SortKey::Accuracy ? b.score : b.rtf;
            const bool higherIsBetter = key == SortKey::Accuracy;
            return (higherIsBetter == bestFirst) ? x > y : x < y;
        });
    }
    endResetModel();
}

void ModelInfoModel::refreshInstalled()
{
    for (int i = 0; i < m_items.size(); ++i) {
        const bool installed = SherpaInstaller::isInstalled(m_items[i].repoId);
        if (installed == m_items[i].installed) continue;
        m_items[i].installed = installed;
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, {InstalledRole});
    }
}

int ModelInfoModel::rowOfDisplayName(const QString& displayName) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].displayName == displayName) return i;
    }
    return -1;
}

// 准确度：按错误率折线映射到 1~5。锚点沿用原来的分档（60/75/85 分对应 2/3/4 档），
// 95 分以上封顶；低于 30 分按 1.0。折线而非整档，同档里的模型也能比出高低
double ModelInfoModel::accuracyRating(double score)
{
    struct Anchor { double score, rating; };
    static const Anchor kAnchors[] = {{30, 1.0}, {60, 2.0}, {75, 3.0}, {85, 4.0}, {95, 5.0}};
    if (score <= kAnchors[0].score) return 1.0;
    for (size_t i = 1; i < std::size(kAnchors); ++i) {
        if (score <= kAnchors[i].score) {
            const Anchor& a = kAnchors[i - 1];
            const Anchor& b = kAnchors[i];
            return roundTenth(a.rating + (b.rating - a.rating) * (score - a.score) / (b.score - a.score));
        }
    }
    return 5.0;
}

// 速度：RTF 越小越快，按对数映射——RTF 0.02（10 秒语音 0.2 秒出字）为 5.0，RTF 1（实时）为 1.0，
// 对数是因为模型速度跨度达几十倍，线性映射会把大部分模型挤在同一端
double ModelInfoModel::speedRating(double rtf)
{
    constexpr double kFastRtf = 0.02;
    if (rtf <= kFastRtf) return 5.0;
    return roundTenth(clamp(1.0 + 4.0 * std::log(1.0 / rtf) / std::log(1.0 / kFastRtf), 1.0, 5.0));
}
