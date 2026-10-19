#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>


struct ModelInfo {
    QString displayName;
    QString repoId;
    QStringList languages;      // 该模型所在的全部语言分组
    bool installed = false;
    bool builtinPunct = false;
    bool benchmarked = false;   // 有无该语言的基准数据
    double score = 0;           // 100 × (1 − CER/WER)
    double rtf = 0;             // 识别耗时 / 音频时长
    QString metric;             // "cer" / "wer"
    int rank = 0;               // 注册表里的原始顺序（质量从高到低），用于恢复默认排序
};

class ModelInfoModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        RepoIdRole = Qt::UserRole + 1,
        LanguagesRole,         
        InstalledRole,
        BuiltinPunctRole,
        BenchmarkedRole,
        ScoreRole,
        RtfRole,
        MetricRole,
        AccuracyLevelRole,      // 信号格数 1..5，0 表示无数据
        SpeedLevelRole,         // 信号格数 1..5，0 表示无数据
        AccuracyRatingRole,     // 1.0..5.0（一位小数），界面显示用
        SpeedRatingRole,        // 1.0..5.0（一位小数），界面显示用
    };

    // 排序依据；Default 为注册表原始顺序
    enum class SortKey { Default, Accuracy, Speed };

    explicit ModelInfoModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setLanguage(const QString& language);
    void refreshInstalled();
    // Descending 表示“最好的在前”（准确度高 / 速度快）；没有基准数据的模型始终排在最后
    void setSort(SortKey key, Qt::SortOrder order);
    int rowOfDisplayName(const QString& displayName) const;

    // 实测数据折算成 1.0~5.0 的评分，保留一位小数
    static double accuracyRating(double score);
    static double speedRating(double rtf);

private:
    QString m_language;
    QList<ModelInfo> m_items;
};
