#include "TermWidget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QSortFilterProxyModel>

TermWidget::TermWidget(QWidget *parent) : QWidget(parent) {

    setupUi();
}

void TermWidget::setupUi() {
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Search Area
    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_labelSearch = new QLabel(tr("Search Terms"), this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Enter terms..."));

    m_reloadBtn = new QPushButton(tr("Reload Terms"), this);
    m_checkTermsBtn = new QPushButton(tr("Check Terms"), this);

    searchLayout->addWidget(m_labelSearch);
    searchLayout->addWidget(m_searchEdit);
    searchLayout->addStretch(); 
    searchLayout->addWidget(m_reloadBtn);
    searchLayout->addWidget(m_checkTermsBtn);

    // Table Area
    QWidget* tableContainer = new QWidget(this);
    QVBoxLayout* tableContainerLayout = new QVBoxLayout(tableContainer);
    tableContainerLayout->setContentsMargins(0, 0, 0, 0);
    tableContainerLayout->setSpacing(4);

    m_termsTableView = new QTableView(this);
    m_termsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_termsTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_termsTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    tableContainerLayout->addWidget(m_termsTableView);

    // Selection & Add Row
    QHBoxLayout* actionRow1 = new QHBoxLayout();
    m_deleteSelectedBtn = new QPushButton(tr("Delete Selected"), this);
    m_selectAllBtn = new QPushButton(tr("Select All"), this);
    m_addRowBtn = new QPushButton(tr("Add Row"), this);
    actionRow1->addWidget(m_deleteSelectedBtn);
    actionRow1->addWidget(m_selectAllBtn);
    actionRow1->addWidget(m_addRowBtn);

    // Batch Operations
    QHBoxLayout* actionRow2 = new QHBoxLayout();
    m_reverseSelectBtn = new QPushButton(tr("Reverse Select"), this);
    m_batchEnableBtn = new QPushButton(tr("Batch Enable"), this);
    m_batchDisableBtn = new QPushButton(tr("Batch Disable"), this);
    actionRow2->addWidget(m_reverseSelectBtn);
    actionRow2->addWidget(m_batchEnableBtn);
    actionRow2->addWidget(m_batchDisableBtn);

    // Import/Export & Check 
    QHBoxLayout* actionRow3 = new QHBoxLayout();
    m_checkBtn = new QPushButton(tr("Check"), this);
    m_importBtn = new QPushButton(tr("Import TSV"), this);
    m_exportBtn = new QPushButton(tr("Export TSV"), this);
    actionRow3->addWidget(m_checkBtn);
    actionRow3->addWidget(m_importBtn);
    actionRow3->addWidget(m_exportBtn);

    // 将所有行添加到主布局
    mainLayout->addLayout(searchLayout);
    mainLayout->addWidget(tableContainer);
    mainLayout->addLayout(actionRow1);
    mainLayout->addLayout(actionRow2);
    mainLayout->addLayout(actionRow3);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &TermWidget::handleSearch);
    connect(m_reloadBtn, &QPushButton::clicked, this, &TermWidget::handleReload);
    connect(m_checkTermsBtn, &QPushButton::clicked, this, &TermWidget::handleCheckTerms);
    connect(m_deleteSelectedBtn, &QPushButton::clicked, this, &TermWidget::handleDeleteSelected);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &TermWidget::handleSelectAll);
    connect(m_addRowBtn, &QPushButton::clicked, this, &TermWidget::handleAddRow);
    connect(m_reverseSelectBtn, &QPushButton::clicked, this, &TermWidget::handleReverseSelect);
    connect(m_batchEnableBtn, &QPushButton::clicked, this, &TermWidget::handleBatchEnable);
    connect(m_batchDisableBtn, &QPushButton::clicked, this, &TermWidget::handleBatchDisable);
    connect(m_checkBtn, &QPushButton::clicked, this, &TermWidget::handleCheckAction);
    connect(m_importBtn, &QPushButton::clicked, this, &TermWidget::handleImport);
    connect(m_exportBtn, &QPushButton::clicked, this, &TermWidget::handleExport);
}

void TermWidget::setTermsManager(TermsLibraryManager* termsManager) {
    m_termsManager = termsManager;
    m_termsModel = new TermTableModel(m_termsManager, this);
    connect(m_termsModel, &TermTableModel::errorOccurred, this, &TermWidget::handleModelError);

    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_termsModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);
    m_termsTableView->setModel(m_proxyModel);
    retranslateUi();

}

void TermWidget::handleModelError(const QString& message) {
    QMessageBox::warning(this, tr("Input Error"), message);
}

void TermWidget::handleSearch() {
    QString searchText = m_searchEdit->text().trimmed();
    m_proxyModel->setFilterFixedString(searchText);
}

QList<int> TermWidget::selectedSourceRows() const {
    QSet<int> rowsSet;
    QItemSelectionModel* selection = m_termsTableView->selectionModel();
    if (!selection) return {};

    const QModelIndexList selectedProxyIndices = selection->selectedRows();
    for (const QModelIndex& proxyIndex : selectedProxyIndices) {
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid()) {
            rowsSet.insert(sourceIndex.row());
        }
    }

    QList<int> rows = rowsSet.values();
    std::sort(rows.begin(), rows.end());
    return rows;
}



void TermWidget::handleReload() {
    if (m_termsManager->loadFromTsv("terms.tsv")) {
        qDebug() << "Terms reloaded successfully.";
    }
    else {
        QMessageBox::warning(this, "Error", "Failed to load terms.tsv");
    }
}

void TermWidget::handleCheckTerms() {
    QString searchText = m_searchEdit->text().trimmed();
    if (searchText.isEmpty()) {
    }
    else {
        qDebug() << "Searching for:" << searchText;
    }
}

void TermWidget::handleDeleteSelected() {
    QList<int> sortedRows = selectedSourceRows();
    if (sortedRows.isEmpty()) return;

    auto res = QMessageBox::question(this, tr("Confirm"),
        tr("Delete %1 selected rows?").arg(sortedRows.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    for (int i = sortedRows.size() - 1; i >= 0; --i) {
        m_termsManager->removeItem(sortedRows.at(i));
    }
}

void TermWidget::handleSelectAll() {
    m_termsTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QItemSelectionModel* selection = m_termsTableView->selectionModel();
    if (!selection) return;

    int rowCount = m_proxyModel->rowCount();
    if (rowCount == 0) return;

    QItemSelection all(m_proxyModel->index(0, 0),
        m_proxyModel->index(rowCount - 1, m_proxyModel->columnCount() - 1));
    selection->select(all, QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void TermWidget::handleAddRow() {
    TermItem newItem;
    m_termsManager->addItem(newItem);
    m_termsModel->layoutChanged();
}

void TermWidget::handleReverseSelect() {
    QItemSelectionModel* selection = m_termsTableView->selectionModel();
    if (!selection) return;

    int rowCount = m_proxyModel->rowCount();
    if (rowCount == 0) return;

    QModelIndexList selectedIndices = selection->selectedRows();
    QSet<int> selectedRowsSet;
    for (const QModelIndex& idx : selectedIndices) {
        selectedRowsSet.insert(idx.row());
    }

    QItemSelection unselectedSelection;
    for (int i = 0; i < rowCount; ++i) {
        if (!selectedRowsSet.contains(i)) {
            unselectedSelection.select(m_proxyModel->index(i, 0), m_proxyModel->index(i, 0)); // 改成 proxy
        }
    }
    selection->clear();
    selection->select(unselectedSelection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void TermWidget::handleBatchEnable() {
    const QList<int> rows = selectedSourceRows();
    for (int row : rows) {
        TermItem item = m_termsManager->items().at(row);
        item.enabled = true;
        m_termsManager->updateItem(row, item);
    }
}

void TermWidget::handleBatchDisable() {
    const QList<int> rows = selectedSourceRows();
    for (int row : rows) {
        TermItem item = m_termsManager->items().at(row);
        item.enabled = false;
        m_termsManager->updateItem(row, item);
    }
}

void TermWidget::handleCheckAction() {
    // 调用 Manager 的冲突检查功能
    QStringList conflicts = m_termsManager->checkConflicts();
    if (conflicts.isEmpty()) {
        QMessageBox::information(this, "Check Result", "No conflicts found! ✅");
    }
    else {
        QString msg = "Found conflicts:\n\n" + conflicts.join("\n");
        QMessageBox::warning(this, "Conflict Detected", msg);
    }
}

void TermWidget::handleImport() {
    QString path = QFileDialog::getOpenFileName(this, "Import TSV", "", "TSV Files (*.tsv)");
    if (!path.isEmpty()) {
        if (m_termsManager->importFromFile(path)) {
            qDebug() << "Import successful";
        }
        else {
            QMessageBox::critical(this, "Error", "Import failed");
        }
    }
}

void TermWidget::handleExport() {
    QString path = QFileDialog::getSaveFileName(this, "Export TSV", "", "TSV Files (*.tsv)");
    if (!path.isEmpty()) {
        if (m_termsManager->exportToFile(path)) {
            QMessageBox::information(this, "Success", "Exported successfully");
        }
        else {
            QMessageBox::critical(this, "Error", "Export failed");
        }
    }
}

void TermWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }
    QWidget::changeEvent(event);
}

void TermWidget::retranslateUi()
{
    m_labelSearch->setText(tr("Search Terms:"));
    m_searchEdit->setPlaceholderText(tr("Enter terms..."));

    // 2. 重新设置底部按钮组
    m_reloadBtn->setText(tr("Reload Terms"));
    m_checkTermsBtn->setText(tr("Check Terms"));
    m_deleteSelectedBtn->setText(tr("Delete Selected"));
    m_selectAllBtn->setText(tr("Select All"));
    m_addRowBtn->setText(tr("Add Row"));
    m_reverseSelectBtn->setText(tr("Reverse Select"));
    m_batchEnableBtn->setText(tr("Batch Enable"));
    m_batchDisableBtn->setText(tr("Batch Disable"));

    // 3. 重新设置底部按钮组
    m_checkBtn->setText(tr("Check Conflicts"));
    m_importBtn->setText(tr("Import TSV"));
    m_exportBtn->setText(tr("Export TSV"));

    // 4. 重新设置表格的表头
    m_termsModel->retranslate();
}