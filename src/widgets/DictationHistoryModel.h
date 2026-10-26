#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

// @brief 本次运行的听写记录：最新的在最上面，最多保留 kMaxEntries 条。
//        只存在内存里，关闭程序即清空（听写内容可能涉及隐私）。
//        "声音识别"面板和托盘的"最近听写"菜单共用这一份。
class DictationHistoryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    static constexpr int kMaxEntries = 100;
    enum Roles { TimeRole = Qt::UserRole + 1 };

    explicit DictationHistoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void add(const QString& text, const QDateTime& time = QDateTime::currentDateTime());
    void clear();
    QString textAt(int row) const;
    // 全部记录，按时间从早到晚、每条一行（列表里最新的在上，复制出去按说话顺序读更自然）
    QString allText() const;

private:
    struct Entry {
        QDateTime time;
        QString text;
    };
    QList<Entry> m_entries;
};
