#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

class ModelDownloadTracker;


struct ModelInfo {
    QString displayName;
    QString repoId;
    QStringList languages;
    bool installed = false;
    bool builtinPunct = false;
    bool autoLanguage = false;  // 多语种模型且无法指定语种，由模型自己判断
    bool benchmarked = false;
    double score = 0;
    double rtf = 0;
    QString metric;
    int rank = 0;
    QString gpuRepoId;          // 该模型的 GPU 变体（隐藏组件，不是列表行），没有则空
    bool gpuInstalled = false;
    int gpuSizeMb = 0;
};

class ModelInfoModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        RepoIdRole = Qt::UserRole + 1,
        LanguagesRole,         
        InstalledRole,
        BuiltinPunctRole,
        AutoLanguageRole,
        BenchmarkedRole,
        ScoreRole,
        RtfRole,
        MetricRole,
        AccuracyLevelRole,
        SpeedLevelRole,
        AccuracyRatingRole,
        SpeedRatingRole,
        DownloadStateRole,
        DownloadPercentRole,
        DownloadTextRole,
        GpuStateRole,           // GpuVariant::GpuState（int）；CPU 模式下恒为 -1（不显示）
        GpuSizeMbRole,
        DownloadRepoIdRole,     // 取消/重试下载时对应的仓库：GPU 版在下载时是它自己的仓库名
    };

    enum class SortKey { Default, Accuracy, Speed };

    explicit ModelInfoModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // 下载状态放在 tracker 里，切换语言重建列表时进度不丢；tracker 变化只刷新对应行
    void setTracker(ModelDownloadTracker* tracker);

    void setLanguage(const QString& language);
    // GPU 模式只影响每行"特性"里的 GPU 提示，列表内容两种模式下完全一样
    void setGpuMode(bool gpuMode);
    void refreshInstalled();
    // Descending 表示“最好的在前”（准确度高 / 速度快）；没有基准数据的模型始终排在最后
    void setSort(SortKey key, Qt::SortOrder order);
    int rowOfDisplayName(const QString& displayName) const;

    static double accuracyRating(double score);
    static double speedRating(double rtf);

private:
    QString downloadRepoOf(const ModelInfo& m) const;

    bool m_gpuMode = false;
    QString m_language;
    QList<ModelInfo> m_items;
    ModelDownloadTracker* m_tracker = nullptr;
};
