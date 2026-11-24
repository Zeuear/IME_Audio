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
    enum Roles { TimeRole = Qt::UserRole + 1, TranslationRole };

    explicit TranscriptLineModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // 空白句忽略；句内换行并成空格，保证一句只占一行
    // translation 是同一句的译文（没开翻译时为空，右栏对应空行），随稿子一起存盘
    void add(const QString& text, const QTime& time = QTime::currentTime(), const QString& translation = {});
    void clear();
    QString textAt(int row) const;

    // 要翻译的句子先把原文放进来（key 是识别服务里的序号，按序号排位，保证顺序不乱），
    // 译文好了用 completePending 补上；找不到这一行（如期间被清空）返回 false
    void addPending(quint64 key, const QString& text, const QTime& time = QTime::currentTime());
    bool completePending(quint64 key, const QString& text, const QString& translation);

    // 每条一行，withTime 时行首带 [hh:mm:ss]
    QString linesText(const QList<int>& rows, bool withTime) const;
    QString allText(bool withTime) const;
    bool hasTranslation() const;
    // 只取译文（原文、译文是两份独立的文本）：没有译文的句子跳过，格式与 linesText / allText 相同
    QString translationLinesText(const QList<int>& rows, bool withTime) const;
    QString allTranslationText(bool withTime) const;
    QString translationAt(int row) const;
    // 用户手动修正某一句：原文不允许改成空（会让这一行消失），译文可以清空；返回是否真的改动了
    bool setText(int row, const QString& text);
    bool setTranslation(int row, const QString& translation);

    // 存盘格式：每句 "[hh:mm:ss] 原文"，有译文的在下一行接 ">> 译文"；没有译文时与 allText(true) 相同，
    // 所以老版本存的稿子照常读回。读回时没有时间前缀的行（旧版稿子）原样作为一句
    QString serialize() const;
    void restore(const QString& serialized);

private:
    struct Entry {
        QTime time;
        QString text;
        QString translation;
        quint64 key = 0;          // 等译文的句子：识别序号；0 表示不是等待中（不存盘）
        QString pendingText;      // 先显示的原文；补译文时用户已手动改过原文就不再覆盖
    };
    static QString lineOf(const Entry& e, bool withTime);
    static QString singleLine(const QString& text);

    QList<Entry> m_entries;
};
