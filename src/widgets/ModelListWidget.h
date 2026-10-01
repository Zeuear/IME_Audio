#pragma once
#include <QWidget>

class QListView;
class QLabel;
class ModelInfoModel;

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

private:
    void setupUi();
    void retranslateUi();
    void fitHeight();

    ModelInfoModel* m_model = nullptr;
    QWidget* m_header = nullptr;
    QListView* m_list = nullptr;
    QLabel* m_note = nullptr;
};
