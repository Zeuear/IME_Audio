    #pragma once
    #include <QObject>
    #include <QString>
    #include <QVector>

    struct TermItem {
        bool enabled = true;
        QString wrong;     
        QString correct;    
        QString aliases;    
        QString mode = "all"; 
        QString tags;       
        QString notes;
    };

    class TermsLibraryManager : public QObject {
        Q_OBJECT
    public:
        explicit TermsLibraryManager(QObject *parent = nullptr);

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
        void clear();

        static QString defaultPath();

        bool hasDuplicateCorrect(const QString& targetCorrect, int currentRow);
        bool hasDuplicateWrong(const QString& targetWrong, int currentRow);
        static QString applyReplaceRules(const QString& text, const QString& rules);

        static bool modeMatches(const QString& mode, const QString& target);
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
        QVector<TermItem> m_items;
    };