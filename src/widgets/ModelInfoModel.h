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
        AccuracyLevelRole,      // 1..5，0 表示无数据
        SpeedLevelRole,         // 1..5，0 表示无数据
    };

    explicit ModelInfoModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setLanguage(const QString& language);
    void refreshInstalled();
    int rowOfDisplayName(const QString& displayName) const;

    static int accuracyLevel(double score);
    static int speedLevel(double rtf);

private:
    QString m_language;
    QList<ModelInfo> m_items;
};
