#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

class ModelDownloadTracker;


struct ModelInfo {
    QString displayName;
    QString repoId;
    QStringList languages;      // 该模型所在的全部语言分组
    bool installed = false;
    bool builtinPunct = false;
    bool autoLanguage = false;  // 多语种模型且无法指定语种，由模型自己判断
    bool benchmarked = false;   // 有无该语言的基准数据
    double score = 0;           // 100 × (1 − CER/WER)
    double rtf = 0;             // 识别耗时 / 音频时长
    QString metric;             // "cer" / "wer"
    int rank = 0;               // 注册表里的原始顺序（质量从高到低），用于恢复默认排序
    QString gpuRepoId;          // 该模型的 GPU 变体（隐藏组件，不是列表行），没有则空
    bool gpuInstalled = false;  // GPU 变体是否已下载
    int gpuSizeMb = 0;          // GPU 变体的下载体积，只用于提示
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
        AccuracyLevelRole,      // 信号格数 1..5，0 表示无数据
        SpeedLevelRole,         // 信号格数 1..5，0 表示无数据
        AccuracyRatingRole,     // 1.0..5.0（一位小数），界面显示用
        SpeedRatingRole,        // 1.0..5.0（一位小数），界面显示用
        DownloadStateRole,      // ModelDownloadTracker::State（int），0 = 没有下载任务
        DownloadPercentRole,    // 下载或解压的百分比
        DownloadTextRole,       // 下载中："文件名 · 第几个/共几个 · 百分比"；失败：原因
        GpuStateRole,           // GpuVariant::GpuState（int）；CPU 模式下恒为 -1（不显示）
        GpuSizeMbRole,          // GPU 变体的下载体积（MB），0 = 没有
        DownloadRepoIdRole,     // 取消/重试下载时对应的仓库：GPU 版在下载时是它自己的仓库名
    };

    // 排序依据；Default 为注册表原始顺序
    enum class SortKey { Default, Accuracy, Speed };

    explicit ModelInfoModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // 下载状态来自 tracker：状态放在列表之外，切换语言重建列表时下载进度不会丢。
    // tracker 变化时只刷新对应那一行，装好时顺带更新"已安装"
    void setTracker(ModelDownloadTracker* tracker);

    void setLanguage(const QString& language);
    // GPU 模式只影响每行"特性"里的 GPU 提示，列表内容两种模式下完全一样
    void setGpuMode(bool gpuMode);
    void refreshInstalled();
    // Descending 表示“最好的在前”（准确度高 / 速度快）；没有基准数据的模型始终排在最后
    void setSort(SortKey key, Qt::SortOrder order);
    int rowOfDisplayName(const QString& displayName) const;

    // 实测数据折算成 1.0~5.0 的评分，保留一位小数
    static double accuracyRating(double score);
    static double speedRating(double rtf);

private:
    // 该行当前要显示哪个仓库的下载状态：GPU 模式下 GPU 版正在下载就显示它，否则是 CPU 版本身
    QString downloadRepoOf(const ModelInfo& m) const;

    bool m_gpuMode = false;
    QString m_language;
    QList<ModelInfo> m_items;
    ModelDownloadTracker* m_tracker = nullptr;
};
