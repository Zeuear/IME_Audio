#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

// @brief 本次运行的听写记录：最新在前，最多 kMaxEntries 条；仅存内存（涉及隐私），识别面板与托盘菜单共用。
class DictationHistoryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    static constexpr int kMaxEntries = 100;
    enum Roles { TimeRole = Qt::UserRole + 1 };

    explicit DictationHistoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // 上限默认 kMaxEntries；实时转录要保留整篇稿子，可调大。调小会立刻丢掉最早的记录
    int maxEntries() const { return m_maxEntries; }
    void setMaxEntries(int maxEntries);

    void add(const QString& text, const QDateTime& time = QDateTime::currentDateTime());
    void clear();
    QString textAt(int row) const;
    QString allText() const;

private:
    struct Entry {
        QDateTime time;
        QString text;
    };
    void trimToLimit();

    QList<Entry> m_entries;
    int m_maxEntries = kMaxEntries;
};
