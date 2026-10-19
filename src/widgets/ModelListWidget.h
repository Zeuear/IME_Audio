#pragma once
#include "ModelInfoModel.h"
#include <QWidget>

class QListView;

// @brief
// 本地识别页的模型列表：显示所选语言下各模型的准确度、速度与特性，点击即选中该模型。
class ModelListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelListWidget(QWidget* parent = nullptr);

    void setLanguage(const QString& language);
    bool setCurrentModel(const QString& displayName);
    QString currentModel() const;
    void refreshInstalled();

signals:
    // 仅用户点击时发出；程序同步选中不会触发
    void modelActivated(const QString& displayName);

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void fitHeight();
    void onHeaderSortClicked(ModelInfoModel::SortKey key);
    void applySort();

    ModelInfoModel* m_model = nullptr;
    QWidget* m_header = nullptr;
    QListView* m_list = nullptr;
    ModelInfoModel::SortKey m_sortKey = ModelInfoModel::SortKey::Default;
    Qt::SortOrder m_sortOrder = Qt::DescendingOrder;
};
