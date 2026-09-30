#include "FileTaskModel.h"
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <algorithm>

FileTaskModel::FileTaskModel(QObject* parent) : QAbstractListModel(parent) {}

int FileTaskModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_tasks.size();
}

QVariant FileTaskModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_tasks.size()) return {};
    const FileTask& t = m_tasks.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return QFileInfo(t.path).fileName();
    case Qt::ToolTipRole:
        return t.state == FileTask::State::Failed ? t.error : t.path;
    case StateRole:
        return static_cast<int>(t.state);
    case ProgressRole:
        return t.segDone;
    case TotalRole:
        return t.segTotal;
    case TextRole:
        return t.text;
    case StatusTextRole:
        switch (t.state) {
        case FileTask::State::Pending:
            return tr("Pending · %1").arg(QLocale().formattedDataSize(t.size));
        case FileTask::State::Decoding:
            return tr("Decoding...");
        case FileTask::State::Transcribing:
            return tr("Transcribing %1/%2").arg(t.segDone).arg(t.segTotal);
        case FileTask::State::Done:
            return tr("Done");
        case FileTask::State::Failed:
            return tr("Failed: %1").arg(t.error);
        case FileTask::State::NoSpeech:
            return tr("No speech detected");
        }
        break;
    }
    return {};
}

int FileTaskModel::count(FileTask::State state) const
{
    return static_cast<int>(std::count_if(m_tasks.begin(), m_tasks.end(),
                                          [state](const FileTask& t) { return t.state == state; }));
}

int FileTaskModel::addPaths(const QStringList& paths)
{
    QSet<QString> known;
    for (const FileTask& t : std::as_const(m_tasks)) known.insert(t.path);

    QList<FileTask> added;
    for (const QString& raw : paths) {
        const QFileInfo info(raw);
        const QString path = info.absoluteFilePath();
        if (!info.isFile() || known.contains(path)) continue;
        known.insert(path);
        FileTask t;
        t.path = path;
        t.size = info.size();
        added.append(t);
    }
    if (added.isEmpty()) return 0;

    beginInsertRows({}, m_tasks.size(), m_tasks.size() + added.size() - 1);
    m_tasks.append(added);
    endInsertRows();
    return added.size();
}

void FileTaskModel::removeRows(QList<int> rows)
{
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int row : std::as_const(rows)) {
        if (row < 0 || row >= m_tasks.size()) continue;
        beginRemoveRows({}, row, row);
        m_tasks.removeAt(row);
        endRemoveRows();
    }
}

void FileTaskModel::clear()
{
    beginResetModel();
    m_tasks.clear();
    endResetModel();
}

void FileTaskModel::reset(int row)
{
    FileTask& t = m_tasks[row];
    t.state = FileTask::State::Pending;
    t.segDone = 0;
    t.segTotal = 0;
    t.text.clear();
    t.error.clear();
    touch(row);
}

void FileTaskModel::setState(int row, FileTask::State state)
{
    m_tasks[row].state = state;
    touch(row);
}

void FileTaskModel::setProgress(int row, int done, int total)
{
    FileTask& t = m_tasks[row];
    t.state = FileTask::State::Transcribing;
    t.segDone = done;
    t.segTotal = total;
    touch(row);
}

void FileTaskModel::appendText(int row, const QString& text)
{
    FileTask& t = m_tasks[row];
    if (!t.text.isEmpty()) t.text += QLatin1Char('\n');
    t.text += text;
    touch(row);
}

void FileTaskModel::setFailed(int row, const QString& error)
{
    FileTask& t = m_tasks[row];
    t.state = FileTask::State::Failed;
    t.error = error;
    touch(row);
}

void FileTaskModel::touch(int row)
{
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}
