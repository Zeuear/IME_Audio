#include "FileTaskModel.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <algorithm>

QString FileTask::displayName() const
{
    if (isLink()) return title.isEmpty() ? url : title;
    return QFileInfo(path).fileName();
}

bool FileTask::hasTranslation() const
{
    return std::any_of(translations.begin(), translations.end(), [](const QString& s) { return !s.isEmpty(); });
}

QList<SubtitleCue> FileTask::translatedCues() const
{
    QList<SubtitleCue> out;
    for (int i = 0; i < cues.size() && i < translations.size(); ++i) {
        if (translations.at(i).isEmpty()) continue;
        out.append({cues.at(i).startMs, cues.at(i).endMs, translations.at(i)});
    }
    return out;
}

QString FileTask::translatedText() const
{
    QStringList lines;
    for (const QString& s : translations) {
        if (!s.isEmpty()) lines << s;
    }
    return lines.join(QLatin1Char('\n'));
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
    case TimingTextRole:
        return timingText(t);
    case StatusTextRole:
        switch (t.state) {
        case FileTask::State::Pending:
            return tr("Pending · %1").arg(QLocale().formattedDataSize(t.size));
        case FileTask::State::Decoding:
            return tr("Decoding...");
        case FileTask::State::DownloadingModel:
            return tr("Downloading vocal model... %1%").arg(t.segDone);
        case FileTask::State::Separating:
            return t.segTotal > 0 ? tr("Separating vocals %1%").arg(t.segDone * 100 / t.segTotal)
                                  : tr("Separating vocals...");
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

QString FileTaskModel::formatDuration(qint64 ms)
{
    const qint64 s = qMax<qint64>(0, ms) / 1000;
    const qint64 h = s / 3600;
    const qint64 m = (s / 60) % 60;
    const qint64 sec = s % 60;
    return h > 0 ? QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'))
                 : QStringLiteral("%1:%2").arg(m).arg(sec, 2, 10, QLatin1Char('0'));
}

qint64 FileTaskModel::estimateRemainingMs(qint64 transcribeElapsedMs, int done, int total)
{
    // 一段都没完成时速度未知，不给估计；段长不一，所以这只是粗略值
    if (done <= 0 || total <= 0 || done > total) return -1;
    return transcribeElapsedMs * (total - done) / done;
}

QString FileTaskModel::timingText(const FileTask& t)
{
    using State = FileTask::State;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    switch (t.state) {
    case State::Decoding:
    case State::DownloadingModel:
    case State::Separating:
        if (t.startMs <= 0) return {};
        return tr("Elapsed %1").arg(formatDuration(now - t.startMs));
    case State::Transcribing: {
        if (t.startMs <= 0) return {};
        const QString elapsed = formatDuration(now - t.startMs);
        const qint64 left = t.transcribeStartMs > 0
                                ? estimateRemainingMs(now - t.transcribeStartMs, t.segDone, t.segTotal) : -1;
        return left < 0 ? tr("Elapsed %1").arg(elapsed) : tr("Elapsed %1 · ~%2 left").arg(elapsed, formatDuration(left));
    }
    case State::Done:
    case State::Failed:
    case State::NoSpeech:
        return t.elapsedMs > 0 ? tr("Took %1").arg(formatDuration(t.elapsedMs)) : QString();
    default:
        return {};
    }
}

void FileTaskModel::markStarted(int row)
{
    FileTask& t = m_tasks[row];
    t.startMs = QDateTime::currentMSecsSinceEpoch();
    t.transcribeStartMs = 0;
    t.elapsedMs = 0;
    touch(row);
}

static void freezeElapsed(FileTask& t)
{
    if (t.startMs > 0) t.elapsedMs = QDateTime::currentMSecsSinceEpoch() - t.startMs;
    t.startMs = 0;
    t.transcribeStartMs = 0;
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
        // 行一创建就带令牌：转录中才并入批次的行不会经过 reset()，令牌为 0 时译文会被整体丢弃
        t.runToken = ++m_nextToken;
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
    t.runToken = ++m_nextToken;
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
    t.translations.clear();
    t.translationLang.clear();
    t.startMs = t.transcribeStartMs = t.elapsedMs = 0;
    t.runToken = ++m_nextToken;
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

int FileTaskModel::dropCachedFiles(const QString& linkDownloadsDir)
{
    const QString prefix = QDir::cleanPath(linkDownloadsDir) + QLatin1Char('/');
    int affected = 0;
    for (int row = 0; row < m_tasks.size(); ++row) {
        FileTask& t = m_tasks[row];
        bool changed = false;
        if (!t.vocalsPath.isEmpty()) {
            t.vocalsPath.clear();
            changed = true;
        }
        if (t.isLink() && !t.path.isEmpty()
            && QDir::cleanPath(t.path).startsWith(prefix, Qt::CaseInsensitive)) {
            t.path.clear();
            t.size = 0;
            changed = true;
        }
        if (!changed) continue;
        touch(row);
        ++affected;
    }
    return affected;
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
    t.vocalsPath.clear();
    t.translations.clear();
    t.translationLang.clear();
    t.startMs = t.transcribeStartMs = t.elapsedMs = 0;
    t.runToken = ++m_nextToken;
    touch(row);
}

void FileTaskModel::setState(int row, FileTask::State state)
{
    FileTask& t = m_tasks[row];
    t.state = state;
    if (state == FileTask::State::Done || state == FileTask::State::NoSpeech) freezeElapsed(t);
    touch(row);
}

void FileTaskModel::setProgress(int row, int done, int total)
{
    FileTask& t = m_tasks[row];
    if (t.state != FileTask::State::Transcribing && t.startMs > 0)
        t.transcribeStartMs = QDateTime::currentMSecsSinceEpoch();   // 预计剩余只看转录这一段的速度
    t.state = FileTask::State::Transcribing;
    t.segDone = done;
    t.segTotal = total;
    touch(row);
}

void FileTaskModel::setStageProgress(int row, FileTask::State state, int done, int total)
{
    FileTask& t = m_tasks[row];
    t.state = state;
    t.segDone = done;
    t.segTotal = total;
    touch(row);
}

void FileTaskModel::setVocalsPath(int row, const QString& path)
{
    m_tasks[row].vocalsPath = path;
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
    m_tasks[row].cues.append(cue);
    m_tasks[row].translations.append(QString());
}

void FileTaskModel::setCueTranslation(quint64 runToken, int cue, const QString& lang, const QString& text)
{
    if (runToken == 0) return;
    for (int row = 0; row < m_tasks.size(); ++row) {
        FileTask& t = m_tasks[row];
        if (t.runToken != runToken) continue;
        if (cue < 0 || cue >= t.translations.size()) return;
        t.translations[cue] = text;
        t.translationLang = lang;
        touch(row);
        return;
    }
}

void FileTaskModel::setFailed(int row, const QString& error)
{
    FileTask& t = m_tasks[row];
    t.state = FileTask::State::Failed;
    t.error = error;
    freezeElapsed(t);
    touch(row);
}

void FileTaskModel::touch(int row)
{
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}
