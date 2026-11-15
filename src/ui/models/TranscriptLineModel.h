#pragma once
#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QTime>

// @brief 实时转录稿子：每句一行，按时间从早到晚追加，不设上限。
class TranscriptLineModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { TimeRole = Qt::UserRole + 1 };

    explicit TranscriptLineModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // 空白句忽略；句内换行并成空格，保证一句只占一行
    void add(const QString& text, const QTime& time = QTime::currentTime());
    void clear();
    QString textAt(int row) const;

    // 每条一行，withTime 时行首带 [hh:mm:ss]
    QString linesText(const QList<int>& rows, bool withTime) const;
    QString allText(bool withTime) const;

    // 存盘格式即 allText(true)；读回时没有时间前缀的行（旧版稿子）原样作为一句
    QString serialize() const { return allText(true); }
    void restore(const QString& serialized);

private:
    struct Entry {
        QTime time;
        QString text;
    };
    static QString lineOf(const Entry& e, bool withTime);

    QList<Entry> m_entries;
};
