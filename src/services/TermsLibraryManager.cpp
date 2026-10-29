#include "TermsLibraryManager.h"
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QMap>
#include <algorithm>
#include <functional>
#include <QCoreApplication>
#include "utils/AppPaths.h"
TermsLibraryManager::TermsLibraryManager(QObject *parent) : TermsLibraryManager(defaultPath(), parent) {}

TermsLibraryManager::TermsLibraryManager(const QString &path, QObject *parent) : QObject(parent), m_path(path) {
    connect(this, &TermsLibraryManager::termsReloaded, this, [this]() {
        saveToTsv(m_path);
    });

    loadFromTsv(m_path);
}

QString TermsLibraryManager::defaultPath() {
    return AppPaths::termsFile();
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
    QMap<QString, int> heardMap;   // 听成 → 所在词条
    for (int i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        if (!item.enabled) continue;

        for (const QString& w : item.heard()) {
            if (w == item.correct) {
                conflicts << QString("无效替换(相同): %1").arg(w);
                continue;
            }
            // 同一个听成对应了不同的写成
            if (heardMap.contains(w)) {
                const QString& other = m_items[heardMap[w]].correct;
                if (other != item.correct) {
                    conflicts << QString("映射冲突: [%1] 分别指向 [%2] 和 [%3]").arg(w, other, item.correct);
                }
            } else {
                heardMap.insert(w, i);
            }
        }

        // 写成又是别的词条的听成：A->B, B->C 会链式替换，A->B, B->A 互相抵消
        for (int j = 0; j < m_items.size(); ++j) {
            if (i == j || !m_items[j].enabled || !m_items[j].heard().contains(item.correct)) continue;
            if (item.heard().contains(m_items[j].correct)) {
                if (i < j) {
                    conflicts << QString("循环引用: [%1] <-> [%2]").arg(item.correct, m_items[j].correct);
                }
            } else {
                conflicts << QString("链式依赖: [%1]->[%2]").arg(item.correct, m_items[j].correct);
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

void TermsLibraryManager::setEnabled(const QList<int>& rows, bool enabled) {
    for (int row : rows) {
        if (row >= 0 && row < m_items.size()) m_items[row].enabled = enabled;
    }
    emit termsReloaded();
}

void TermsLibraryManager::removeItems(QList<int> rows) {
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < m_items.size()) m_items.removeAt(row);
    }
    emit termsReloaded();
}

void TermsLibraryManager::clear() {
    m_items.clear();
    emit termsReloaded();
}

QString TermsLibraryManager::duplicateHeard(const QStringList& words, int excludeRow) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (i == excludeRow || !m_items[i].enabled) continue;
        const QStringList existing = m_items[i].heard();
        for (const QString& w : words) {
            if (existing.contains(w, Qt::CaseInsensitive)) return w;
        }
    }
    return {};
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

QStringList TermsLibraryManager::splitWords(const QString& text) {
    QString normalized = text;
    normalized.replace("，", ",").replace("、", ",").replace(";", ",").replace("；", ",").replace("/", ",");
    QStringList list = normalized.split(',', Qt::SkipEmptyParts);
    for (QString& s : list) s = s.trimmed();
    list.removeAll(QString());
    return list;
}

QStringList TermItem::heard() const {
    QStringList words;
    if (!wrong.trimmed().isEmpty()) words << wrong.trimmed();
    words << TermsLibraryManager::splitWords(aliases);
    return words;
}

void TermItem::setHeard(const QStringList& words) {
    wrong = words.value(0);
    aliases = words.mid(1).join(", ");
}

TermsLibraryManager::TermsRuleSet TermsLibraryManager::buildAllRules() const {
    TermsRuleSet result;
    QStringList replaceRules, aiRules;
    QSet<QString> hotwordSeen;

    for (const auto& item : m_items) {
        if (!item.enabled) continue;
        const QString correct = item.correct.trimmed();
        if (correct.isEmpty()) continue;

        result.enabledCount++;
        const QStringList heard = item.heard();

        for (const QString& w : heard) {
            if (w != correct) replaceRules << QString("%1=>%2").arg(w, correct);
        }
        if (!heard.isEmpty()) result.replaceCount++;

        aiRules << (heard.isEmpty() ? correct : QString("%1=%2").arg(heard.join('/'), correct));
        result.aiCount++;

        // 热词只放正确写法：放进听错的词等于让识别器更倾向于识别出错词
        if (!hotwordSeen.contains(correct)) {
            hotwordSeen.insert(correct);
            result.hotwords << correct;
            result.hotwordCount++;
        }
    }

    result.replaceRules = replaceRules.join(';');
    result.aiVocab = aiRules.join(';');
    return result;
}
