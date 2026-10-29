    #pragma once
    #include <QObject>
    #include <QString>
    #include <QVector>

    // 一条词条 = "听成"（可多个）→ "写成"。听成留空表示只提示识别器有这个词。
    // wrong / aliases 分两列只是为了兼容 terms.tsv：第一个听成写进 wrong，其余写进 aliases。
    // mode 已不再区分，每条词条都同时用于替换、AI 词库和热词，读写时保留以兼容旧文件。
    struct TermItem {
        bool enabled = true;
        QString wrong;
        QString correct;
        QString aliases;
        QString mode = "all";
        QString tags;
        QString notes;

        QStringList heard() const;
        void setHeard(const QStringList& words);
    };

    class TermsLibraryManager : public QObject {
        Q_OBJECT
    public:
        explicit TermsLibraryManager(QObject *parent = nullptr);
        // 词库文件路径，改动后自动写回这里；测试用临时文件，避免读写用户的词库
        explicit TermsLibraryManager(const QString &path, QObject *parent = nullptr);

        bool loadFromTsv(const QString &path);
        bool saveToTsv(const QString &path) const;

        bool importFromFile(const QString &path);
        bool exportToFile(const QString &path) const;

        QStringList checkConflicts() const;

        QVector<TermItem> &items() { return m_items; }
        const QVector<TermItem> &items() const { return m_items; }

        void updateItem(int index, const TermItem& newItem);
        void addItem(const TermItem& item);
        void removeItem(int index);
        // 批量操作只发一次 termsReloaded，避免每条都重建规则、写一次文件
        void setEnabled(const QList<int>& rows, bool enabled);
        void removeItems(QList<int> rows);
        void clear();

        static QString defaultPath();

        // 返回 words 中已被其它启用词条占用的第一个听成，没有则为空
        QString duplicateHeard(const QStringList& words, int excludeRow = -1) const;
        static QString applyReplaceRules(const QString& text, const QString& rules);

        // 中英文逗号、顿号、分号、斜杠都可作分隔
        static QStringList splitWords(const QString& text);

        struct TermsRuleSet {
            QString replaceRules;
            QString aiVocab;
            QStringList hotwords;
            int enabledCount = 0;
            int replaceCount = 0;
            int aiCount = 0;
            int hotwordCount = 0;
        };
        TermsRuleSet buildAllRules() const;

    signals:
        void termsReloaded();

    private:
        QString m_path;
        QVector<TermItem> m_items;
    };