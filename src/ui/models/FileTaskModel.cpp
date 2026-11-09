#include "FileTaskModel.h"
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <algorithm>

QString FileTask::displayName() const
{
    if (isLink()) return title.isEmpty() ? url : title;
    return QFileInfo(path).fileName();
}

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
        return t.displayName();
    case Qt::ToolTipRole:
        if (t.state == FileTask::State::Failed) return t.error;
        return t.isLink() ? t.url : t.path;
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
        case FileTask::State::Downloading:
            return t.segTotal > 0 ? tr("Downloading %1%").arg(t.segDone) : tr("Preparing download...");
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

int FileTaskModel::addLink(const QString& url)
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks.at(i).url != url) continue;
        // 同一链接再次添加：上次下载失败的就当作重试，其余视为重复
        if (m_tasks.at(i).state != FileTask::State::Failed || !m_tasks.at(i).path.isEmpty()) return -1;
        restartDownload(i);
        return i;
    }
    FileTask t;
    t.url = url;
    t.state = FileTask::State::Downloading;
    const int row = m_tasks.size();
    beginInsertRows({}, row, row);
    m_tasks.append(t);
    endInsertRows();
    return row;
}

int FileTaskModel::rowOfLink(quint64 linkId) const
{
    if (linkId == 0) return -1;
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks.at(i).linkId == linkId) return i;
    }
    return -1;
}

void FileTaskModel::setLinkId(int row, quint64 linkId)
{
    m_tasks[row].linkId = linkId;
}

void FileTaskModel::setDownloadPercent(int row, int percent)
{
    FileTask& t = m_tasks[row];
    if (t.state != FileTask::State::Downloading) return;
    t.segDone = percent;
    t.segTotal = 100;
    touch(row);
}

void FileTaskModel::setTitle(int row, const QString& title)
{
    m_tasks[row].title = title;
    touch(row);
}

void FileTaskModel::setDownloaded(int row, const QString& path, const QString& title)
{
    FileTask& t = m_tasks[row];
    const QFileInfo info(path);
    t.path = info.absoluteFilePath();
    t.size = info.size();
    if (!title.isEmpty()) t.title = title;
    t.state = FileTask::State::Pending;
    t.segDone = 0;
    t.segTotal = 0;
    t.error.clear();
    touch(row);
}

void FileTaskModel::restartDownload(int row)
{
    FileTask& t = m_tasks[row];
    t.path.clear();
    t.size = 0;
    t.state = FileTask::State::Downloading;
    t.segDone = 0;
    t.segTotal = 0;
    t.text.clear();
    t.cues.clear();
    t.error.clear();
    touch(row);
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
    t.cues.clear();
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

void FileTaskModel::appendCue(int row, const SubtitleCue& cue)
{
    m_tasks[row].cues.append(cue);   // 不影响卡片显示，无需 touch
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
