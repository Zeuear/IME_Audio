#include "TermsLibraryManager.h"
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QMap>
#include <QApplication>
TermsLibraryManager::TermsLibraryManager(QObject *parent) : QObject(parent) {
    connect(this, &TermsLibraryManager::termsReloaded, this, [this]() {
        QString path = QApplication::applicationDirPath() + "/terms.tsv";
        saveToTsv(path);
    });

}

bool TermsLibraryManager::loadFromTsv(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    m_items.clear();
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        QStringList cols = in.readLine().split('\t');
        if (cols.size() < 2) continue;
        TermItem item;
        item.enabled = (cols.value(0) == "1");
        item.wrong = cols.value(1);
        item.correct = cols.value(2);
        item.aliases = cols.value(3);
        item.mode = cols.value(4).isEmpty() ? "all" : cols.value(4);
        item.tags = cols.value(5);
        item.notes = cols.value(6);
        m_items.append(item);
    }
    emit termsReloaded();
    return true;
}

bool TermsLibraryManager::saveToTsv(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const auto &item : m_items) {
        out << (item.enabled ? "1" : "0") << '\t'
            << item.wrong << '\t' << item.correct << '\t'
            << item.aliases << '\t' << item.mode << '\t'
            << item.tags << '\t' << item.notes << '\n';
    }
    return true;
}

bool TermsLibraryManager::importFromFile(const QString &path) { return loadFromTsv(path); }
bool TermsLibraryManager::exportToFile(const QString &path) const { return saveToTsv(path); }

QStringList TermsLibraryManager::checkConflicts() const {
    QStringList conflicts;
    // 1. 检查重复和无效
    QMap<QString, int> wrongMap;
    for (int i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        if (!item.enabled) continue;

        if (item.wrong == item.correct) {
            conflicts << QString("无效替换(相同): %1").arg(item.wrong);
        }

        // 2. 映射冲突检查 (同一个错词对应了不同的正词)
        if (wrongMap.contains(item.wrong)) {
            int otherIdx = wrongMap[item.wrong];
            if (m_items[otherIdx].correct != item.correct) {
                conflicts << QString("映射冲突: [%1] 分别指向 [%2] 和 [%3]")
                    .arg(item.wrong, m_items[otherIdx].correct, item.correct);
            }
        }
        else {
            wrongMap.insert(item.wrong, i);
        }

        // 3. 循环引用 & 链式依赖
        for (int j = 0; j < m_items.size(); ++j) {
            if (i == j || !m_items[j].enabled) continue;
            // 循环: A->B, B->A
            if (item.correct == m_items[j].wrong && m_items[j].correct == item.wrong) {
                conflicts << QString("循环引用: [%1]->[%2] <-> [%3]->[%4]")
                    .arg(item.wrong, item.correct, m_items[j].wrong, m_items[j].correct);
            }
            // 链式: A->B, B->C
            else if (item.correct == m_items[j].wrong) {
                conflicts << QString("链式依赖: [%1]->[%2]->[%3]")
                    .arg(item.wrong, item.correct, m_items[j].correct);
            }
        }
    }
    return conflicts; 
}

void TermsLibraryManager::updateItem(int index, const TermItem& newItem) {
    if (index >= 0 && index < m_items.size()) {
        m_items[index] = newItem;
        emit termsReloaded(); 
    }
}

void TermsLibraryManager::addItem(const TermItem& item) {
    m_items.append(item);
    emit termsReloaded();
}

void TermsLibraryManager::removeItem(int index) {
    if (index >= 0 && index < m_items.size()) {
        m_items.removeAt(index);
        emit termsReloaded();
    }
}

void TermsLibraryManager::clear() {
    m_items.clear();
    emit termsReloaded();
}

bool TermsLibraryManager::hasDuplicateWrong(const QString& targetWrong, int currentRow) {
    if (targetWrong.isEmpty()) return false;

    for (int i = 0; i < m_items.size(); ++i) {
        if (i == currentRow) continue; 

        if (m_items[i].enabled &&
            m_items[i].wrong.compare(targetWrong, Qt::CaseInsensitive) == 0) {
            return true; 
        }
    }
    return false;
}

bool TermsLibraryManager::hasDuplicateCorrect(const QString& targetCorrect, int currentRow) {
    if (targetCorrect.isEmpty()) return false;

    for (int i = 0; i < m_items.size(); ++i) {
        if (i == currentRow) continue;

        if (m_items[i].enabled &&
            m_items[i].correct.compare(targetCorrect, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString TermsLibraryManager::applyReplaceRules(const QString& text, const QString& rules) {
    if (rules.isEmpty()) return text;

    QVector<QPair<QString, QString>> ruleList;
    const auto pairs = rules.split(';', Qt::SkipEmptyParts);
    for (const QString& pair : pairs) {
        QStringList kv = pair.split("=>");
        if (kv.size() != 2 || kv[0].isEmpty()) continue;
        ruleList.append({ kv[0], kv[1] });
    }
    if (ruleList.isEmpty()) return text;

    // 按 wrong 长度降序排列：优先匹配更长的词，避免短词提前吃掉长词的一部分
    std::sort(ruleList.begin(), ruleList.end(), [](const auto& a, const auto& b) {
        return a.first.length() > b.first.length();
        });

    QString result = text;
    QMap<QString, QString> placeholderMap;
    int placeholderIndex = 0;

    for (const auto& rule : ruleList) {
        QString placeholder = QString("\x01%1\x02").arg(placeholderIndex++);
        if (result.contains(rule.first)) {
            result.replace(rule.first, placeholder);
            placeholderMap.insert(placeholder, rule.second);
        }
    }

    // 统一把占位符替换回真正的目标词，此时不会再触发二次替换
    for (auto it = placeholderMap.constBegin(); it != placeholderMap.constEnd(); ++it) {
        result.replace(it.key(), it.value());
    }

    return result;
}

static QStringList splitAliases(const QString& aliases) {
    QString normalized = aliases;
    normalized.replace("，", ",").replace("、", ",").replace(";", ",").replace("；", ",").replace("/", ",");
    QStringList list = normalized.split(',', Qt::SkipEmptyParts);
    for (QString& s : list) s = s.trimmed();
    list.removeAll(QString());
    return list;
}

bool TermsLibraryManager::modeMatches(const QString& mode, const QString& target) {
    if (mode.isEmpty() || mode == "all") return true;
    return mode.contains(target);
}

TermsLibraryManager::TermsRuleSet TermsLibraryManager::buildAllRules() const {
    TermsRuleSet result;
    QStringList replaceRules, aiRules;
    QSet<QString> hotwordSeen;

    for (const auto& item : m_items) {
        if (!item.enabled) continue;
        if (item.wrong.isEmpty() || item.correct.isEmpty()) continue;

        result.enabledCount++;
        const auto aliasList = splitAliases(item.aliases);

        if (modeMatches(item.mode, "replace")) {
            replaceRules << QString("%1=>%2").arg(item.wrong, item.correct);
            for (const QString& alias : aliasList) {
                replaceRules << QString("%1=>%2").arg(alias, item.correct);
            }
            result.replaceCount++;
        }

        if (modeMatches(item.mode, "ai")) {
            aiRules << QString("%1=%2").arg(item.wrong, item.correct);
            result.aiCount++;
        }

        if (modeMatches(item.mode, "hotword")) {
            auto addWord = [&](const QString& w) {
                if (w.isEmpty() || hotwordSeen.contains(w)) return;
                hotwordSeen.insert(w);
                result.hotwords << w;
                };
            addWord(item.wrong);
            addWord(item.correct);
            for (const QString& alias : aliasList) addWord(alias);
            result.hotwordCount++;
        }
    }

    result.replaceRules = replaceRules.join(';');
    result.aiVocab = aiRules.join(';');
    return result;
}
