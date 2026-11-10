#pragma once
#include "ui/models/ModelInfoModel.h"
#include <QWidget>

class QListView;

// @brief
// 本地识别页的模型列表：显示所选语言下各模型的准确度、速度与特性，点击即选中该模型。
class ModelListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelListWidget(QWidget* parent = nullptr);

    void setLanguage(const QString& language);
    // GPU 模式只列 GPU 变体；当前选中的模型若被过滤掉，选中状态不动（只是列表里看不到）
    void setGpuMode(bool gpuMode);
    bool setCurrentModel(const QString& displayName);
    QString currentModel() const;
    void refreshInstalled();
    // 行内下载进度来自 tracker（SherpaInstaller 的信号由宿主喂给它）
    void setDownloadTracker(ModelDownloadTracker* tracker);

signals:
    // 仅用户点击时发出；程序同步选中不会触发
    void modelActivated(const QString& displayName);
    // 点了行里的"取消"或"重试"；不会同时选中该行
    void cancelRequested(const QString& repoId);
    void retryRequested(const QString& repoId);

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void fitHeight();
    void onHeaderSortClicked(ModelInfoModel::SortKey key);
    void applySort();

    ModelInfoModel* m_model = nullptr;
    QString m_hiddenSelection;   // 当前模型被模式过滤掉时的显示名
    ModelDownloadTracker* m_tracker = nullptr;
    QWidget* m_header = nullptr;
    QListView* m_list = nullptr;
    ModelInfoModel::SortKey m_sortKey = ModelInfoModel::SortKey::Default;
    Qt::SortOrder m_sortOrder = Qt::DescendingOrder;
};
