#pragma once
#include <QWidget>
#include <QHash>

class QLabel;
class QProgressBar;
class QVBoxLayout;
class QScrollArea;
class SherpaInstaller;
class CudaInstaller;

class ProjectGroupCard : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectGroupCard(const QString& repoId, const QString& displayName, int totalFiles, QWidget* parent = nullptr);

    QString repoId() const { return m_repoId; }

    void updateFileProgress(const QString& filename, qint64 received, qint64 total, int overallPercent);
    void markFileFinished(const QString& filename);
    void markFileError(const QString& filename, const QString& error);
    void updateGroupFinished(bool success, const QString& msg);

private:
    struct FileRow {
        QWidget* container = nullptr;
        QLabel* nameLabel = nullptr;
        QLabel* statusLabel = nullptr;
        QProgressBar* progressBar = nullptr;
    };

    FileRow& ensureFileRow(const QString& filename);

    QString m_repoId;
    int m_totalFiles = 0;
    int m_finishedCount = 0;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_overallLabel = nullptr;
    QProgressBar* m_overallBar = nullptr;
    QVBoxLayout* m_fileListLayout = nullptr;

    QHash<QString, FileRow> m_fileRows;
};


class DownloadListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DownloadListWidget(QWidget* parent = nullptr);
    void setSherpaInstaller(SherpaInstaller* installer);
    void setCudaInstaller(CudaInstaller* installer);

private slots:
    void onInstallGroupStarted(const QString& repoId, const QString& displayName, int totalFiles);
    void onInstallFileProgress(const QString& repoId, const QString& filename, qint64 recv, qint64 total, int overallPercent);
    void onInstallFileFinished(const QString& repoId, const QString& filename);
    void onInstallFileError(const QString& repoId, const QString& filename, const QString& error);
    void onInstallGroupFinished(const QString& repoId, bool success, const QString& msg);

private:
    ProjectGroupCard* ensureCard(const QString& repoId, const QString& displayName, int totalFiles);

	CudaInstaller* m_cudaInstaller = nullptr;
    SherpaInstaller* m_installer = nullptr;
    QVBoxLayout* m_cardListLayout = nullptr;
    QHash<QString, ProjectGroupCard*> m_cards;
};