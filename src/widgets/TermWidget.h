#ifndef TERMWIDGET_H
#define TERMWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QLabel>
#include <QEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QAbstractTableModel>
#include "../TermsLibraryManager.h"

class TermTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TermTableModel(TermsLibraryManager* manager, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_manager(manager) {
        connect(m_manager, &TermsLibraryManager::termsReloaded, this, [this]() {
            beginResetModel();
            endResetModel();
         });
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return m_manager->items().size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return 7; 
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_manager->items().size()) return QVariant();

        const TermItem& item = m_manager->items().at(index.row());

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (index.column()) {
            case 0: return item.enabled ? "1" : "0";
            case 1: return item.wrong;
            case 2: return item.correct;
            case 3: return item.aliases;
            case 4: return item.mode;
            case 5: return item.tags;
            case 6: return item.notes;
            }
        }
        if (role == Qt::CheckStateRole && index.column() == 0) {
            return item.enabled ? Qt::Checked : Qt::Unchecked;
        }
        return QVariant();
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) {
        if (!index.isValid() || role != Qt::EditRole) {
            return false;
        }

        int row = index.row();
        int col = index.column();
        TermItem item = m_manager->items().at(row);
        bool changed = false;

        switch (col) {
        case 0: 
            item.enabled = value.toBool();
            changed = true;
            break;
        case 1:
            item.wrong = value.toString();
            changed = true;
            break;
        case 2:
            item.correct = value.toString();
            changed = true;
            break;
        case 3:
        {
            QString modeText = value.toString().trimmed().toLower();
            if (modeText == "all" || modeText == "replace" ||
                modeText == "ai" || modeText == "hotword") {
                item.mode = modeText;
                changed = true;
            }
            else {
                return false;
            }
            break;
        }
        case 4:
            item.aliases = value.toString();
            changed = true;
            break;
        case 5:
            item.tags = value.toString();
            changed = true;
            break;
        case 6:
            item.notes = value.toString();
            changed = true;
            break;
        default:
            return false;
        }

        if (!changed) return false;

        if (col == 1 || col == 2) {
            if (col == 1) { 
                if (m_manager->hasDuplicateWrong(item.wrong, row)) {
                    emit errorOccurred(tr("Error：Wrong Word [%1] is exist，Can't repeat input！").arg(item.wrong));
                    return false;
                }
            }
        }

        m_manager->updateItem(row, item);
        emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
        return true;
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        Qt::ItemFlags f = QAbstractTableModel::flags(index);
        if (index.column() == 0) f |= Qt::ItemIsUserCheckable; 
        if (index.column() > 0) f |= Qt::ItemIsEditable;     
        return f;
    }


    void retranslate() {
        emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole) return QVariant();

        if (orientation == Qt::Horizontal) {
            const QStringList headers = {
                tr("Enabled"), tr("Wrong"), tr("Correct"),
                tr("Aliases"), tr("Mode"), tr("Tags"), tr("Notes")
            };
            if (section >= 0 && section < headers.size()) {
                return headers.at(section);
            }
        }
        return section + 1;
    }

signals:
    void errorOccurred(const QString& message);

private:
    TermsLibraryManager* m_manager;
};

class TermWidget : public QWidget {
    Q_OBJECT

public:
    explicit TermWidget(QWidget *parent = nullptr);
    ~TermWidget() override = default;

    void setTermsManager(TermsLibraryManager* termsManager);

protected:
    virtual void changeEvent(QEvent* event) override;
    void retranslateUi();
        
private slots:
    void handleSearch();
    void handleReload();
    void handleDeleteSelected();
    void handleSelectAll();
    void handleAddRow();
    void handleReverseSelect();
    void handleBatchEnable();
    void handleBatchDisable();
    void handleCheckAction(); 
    void handleImport();
    void handleExport();
    void handleModelError(const QString& message);

private:
    void setupUi();
    QList<int> selectedSourceRows() const;

    // UI 组件
    QLabel* m_labelSearch;
    QLineEdit* m_searchEdit;
    QPushButton* m_reloadBtn;
    QPushButton* m_checkTermsBtn;

    QSortFilterProxyModel* m_proxyModel;
    QTableView* m_termsTableView;
    TermTableModel* m_termsModel;
    TermsLibraryManager* m_termsManager;  

    QPushButton* m_deleteSelectedBtn;
    QPushButton* m_selectAllBtn;
    QPushButton* m_addRowBtn;

    QPushButton* m_reverseSelectBtn;
    QPushButton* m_batchEnableBtn;
    QPushButton* m_batchDisableBtn;

    QPushButton* m_checkBtn;
    QPushButton* m_importBtn;
    QPushButton* m_exportBtn;

};

#endif // TERMWIDGET_H