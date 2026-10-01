#include "ModelInfoModel.h"
#include "../sherpa/SherpaConfig.h"
#include "../sherpa/SherpaManager.h"
#include "../utils/Logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

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
    case AccuracyLevelRole:   return m.benchmarked ? accuracyLevel(m.score) : 0;
    case SpeedLevelRole:      return m.benchmarked ? speedLevel(m.rtf) : 0;
    default:                  return {};
    }
}

void ModelInfoModel::setLanguage(const QString& language)
{
    beginResetModel();
    m_language = language;
    m_items.clear();
    const QJsonObject scores = benchmarks().value(language).toObject();
    for (const ModelEntry& e : ModelRegistry::GetModelEntriesByLanguage(language)) {
        // 注册表里有空白占位的 repoId，跳过
        if (e.repoPath.trimmed().isEmpty()) continue;
        ModelInfo m;
        m.displayName = e.displayName;
        m.repoId = e.repoPath;
        m.languages = ModelRegistry::GetLanguagesByRepo(e.repoPath);
        m.installed = SherpaInstaller::isInstalled(e.repoPath);
        if (const ModelDescriptor* d = ModelRegistry::Find(e.repoPath)) m.builtinPunct = d->hasBuiltinPunctuation;
        const QJsonObject s = scores.value(e.repoPath).toObject();
        if (!s.isEmpty()) {
            m.benchmarked = true;
            m.score = s.value("score").toDouble();
            m.rtf = s.value("rtf").toDouble();
            m.metric = s.value("metric").toString();
        }
        m_items.push_back(m);
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

// 分档阈值按 FLEURS 实测分布定：92 分以上的模型在同语种里已属第一梯队
int ModelInfoModel::accuracyLevel(double score)
{
    if (score >= 92) return 5;
    if (score >= 85) return 4;
    if (score >= 75) return 3;
    if (score >= 60) return 2;
    return 1;
}

// RTF 0.04 约等于 10 秒语音 0.4 秒出字，听写几乎无感
int ModelInfoModel::speedLevel(double rtf)
{
    if (rtf <= 0.04) return 5;
    if (rtf <= 0.1) return 4;
    if (rtf <= 0.25) return 3;
    if (rtf <= 0.5) return 2;
    return 1;
}
