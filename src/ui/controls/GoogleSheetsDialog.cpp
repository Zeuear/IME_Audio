#include "ui/controls/GoogleSheetsDialog.h"
#include "snippets/GoogleSheetsService.h"
#include <QApplication>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

GoogleSheetsDialog::GoogleSheetsDialog(SnippetStore* store, QWidget* parent)
    : QDialog(parent), m_store(store) {
    setWindowTitle(tr("Google Sheets Import"));
    setMinimumSize(880, 540);
    resize(1020, 660);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 第一步：链接与别名
    auto* topBox = new QGroupBox(tr("Step 1: paste the Google Sheets link and read its sheet list"), this);
    auto* topLayout = new QHBoxLayout(topBox);
    topLayout->addWidget(new QLabel(tr("URL:"), topBox));
    m_urlEdit = new QLineEdit(topBox);
    m_urlEdit->setPlaceholderText("https://docs.google.com/spreadsheets/d/...");
    topLayout->addWidget(m_urlEdit, 1);
    topLayout->addWidget(new QLabel(tr("Name (optional):"), topBox));
    m_aliasEdit = new QLineEdit(topBox);
    m_aliasEdit->setFixedWidth(150);
    topLayout->addWidget(m_aliasEdit);
    auto* fetchBtn = new QPushButton(tr("Read Sheets"), topBox);
    connect(fetchBtn, &QPushButton::clicked, this, &GoogleSheetsDialog::onFetchSheets);
    topLayout->addWidget(fetchBtn);
    mainLayout->addWidget(topBox);

    auto* midLayout = new QHBoxLayout();
    midLayout->setSpacing(12);

    // 左：已保存的表
    auto* leftBox = new QGroupBox(tr("Saved tables"), this);
    leftBox->setFixedWidth(390);
    auto* leftLayout = new QVBoxLayout(leftBox);
    m_savedList = new QListWidget(leftBox);
    connect(m_savedList, &QListWidget::currentRowChanged, this, &GoogleSheetsDialog::onSelectSaved);
    leftLayout->addWidget(m_savedList);
    auto* leftButtons = new QHBoxLayout();
    auto* btnNew = new QPushButton(tr("Add"), leftBox);
    connect(btnNew, &QPushButton::clicked, this, &GoogleSheetsDialog::onNewSheet);
    auto* btnRemove = new QPushButton(tr("Remove"), leftBox);
    connect(btnRemove, &QPushButton::clicked, this, &GoogleSheetsDialog::onRemoveSaved);
    auto* btnRefresh = new QPushButton(tr("Refresh"), leftBox);
    btnRefresh->setToolTip(tr("Refresh using the sheets that were checked last time"));
    connect(btnRefresh, &QPushButton::clicked, this, &GoogleSheetsDialog::onRefreshSaved);
    leftButtons->addWidget(btnNew);
    leftButtons->addWidget(btnRemove);
    leftButtons->addWidget(btnRefresh);
    leftLayout->addLayout(leftButtons);
    midLayout->addWidget(leftBox);

    // 右：勾选要同步的 Sheet
    auto* rightBox = new QGroupBox(tr("Step 2: check the sheets to import and sync"), this);
    auto* rightLayout = new QVBoxLayout(rightBox);
    auto* rightButtons = new QHBoxLayout();
    rightButtons->addStretch();
    auto* btnCheckAll = new QPushButton(tr("Select All"), rightBox);
    connect(btnCheckAll, &QPushButton::clicked, this, [this]() { onSetAllChecked(true); });
    auto* btnInvert = new QPushButton(tr("Invert"), rightBox);
    connect(btnInvert, &QPushButton::clicked, this, &GoogleSheetsDialog::onInvertAll);
    auto* btnUncheckAll = new QPushButton(tr("Select None"), rightBox);
    connect(btnUncheckAll, &QPushButton::clicked, this, [this]() { onSetAllChecked(false); });
    rightButtons->addWidget(btnCheckAll);
    rightButtons->addWidget(btnInvert);
    rightButtons->addWidget(btnUncheckAll);
    rightLayout->addLayout(rightButtons);
    m_sheetList = new QListWidget(rightBox);
    rightLayout->addWidget(m_sheetList);
    midLayout->addWidget(rightBox, 1);
    mainLayout->addLayout(midLayout, 1);

    // 底部：状态与操作
    auto* bottomLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Paste a Google Sheets link, then click \"Read Sheets\"."), this);
    bottomLayout->addWidget(m_statusLabel, 1);
    auto* btnClose = new QPushButton(tr("Close"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(btnClose);
    m_importBtn = new QPushButton(tr("Import Checked Sheets"), this);
    m_importBtn->setDefault(true);
    connect(m_importBtn, &QPushButton::clicked, this, &GoogleSheetsDialog::onImportChecked);
    bottomLayout->addWidget(m_importBtn);
    mainLayout->addLayout(bottomLayout);

    reloadSavedList();
}

GoogleSheetsDialog::~GoogleSheetsDialog() = default;

void GoogleSheetsDialog::reloadSavedList() {
    QSignalBlocker blocker(m_savedList);
    m_savedList->clear();
    for (const auto& gs : m_store->googleSheets()) {
        QString label = gs.title.isEmpty() ? gs.url : gs.title;
        if (!gs.lastSync.isEmpty()) label += "\n  " + tr("Last sync: %1").arg(gs.lastSync);
        auto* item = new QListWidgetItem(label, m_savedList);
        item->setData(Qt::UserRole, gs.treeId);
    }
}

void GoogleSheetsDialog::selectTree(const QString& treeId) {
    for (int i = 0; i < m_savedList->count(); ++i) {
        if (m_savedList->item(i)->data(Qt::UserRole).toString() == treeId) {
            m_savedList->setCurrentRow(i);
            return;
        }
    }
}

void GoogleSheetsDialog::fillSheetList(const QStringList& names, const QStringList& checked, bool allChecked) {
    m_sheetList->clear();
    for (const QString& name : names) {
        auto* item = new QListWidgetItem(name, m_sheetList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState((allChecked || checked.contains(name)) ? Qt::Checked : Qt::Unchecked);
    }
}

QStringList GoogleSheetsDialog::checkedNames() const {
    QStringList names;
    for (int i = 0; i < m_sheetList->count(); ++i) {
        if (m_sheetList->item(i)->checkState() == Qt::Checked) names << m_sheetList->item(i)->text();
    }
    return names;
}

void GoogleSheetsDialog::onNewSheet() {
    m_urlEdit->clear();
    m_aliasEdit->clear();
    m_sheetList->clear();
    m_activeTreeId.clear();
    m_savedList->clearSelection();
    m_savedList->setCurrentRow(-1);
    m_statusLabel->setText(tr("Paste the link of a new Google Sheet."));
}

void GoogleSheetsDialog::onSelectSaved(int row) {
    if (row < 0 || row >= m_savedList->count()) return;
    const QString treeId = m_savedList->item(row)->data(Qt::UserRole).toString();
    auto* gs = m_store->findGoogleSheetByTreeId(treeId);
    if (!gs) return;

    m_activeTreeId = treeId;
    m_urlEdit->setText(gs->url);
    m_aliasEdit->setText(gs->title);
    const QStringList names = gs->allSheets.isEmpty() ? gs->selectedSheets : gs->allSheets;
    fillSheetList(names, gs->selectedSheets, false);
    m_statusLabel->setText(tr("Saved selection loaded. Adjust it and import, or click \"Refresh\" to reuse it."));
}

void GoogleSheetsDialog::onFetchSheets() {
    const QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) {
        m_statusLabel->setText(tr("Paste a Google Sheets link first."));
        return;
    }

    const QStringList previouslyChecked = checkedNames();
    setEnabled(false);
    m_statusLabel->setText(tr("Reading the sheet list..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    m_service.listSheetNames(url, [this, previouslyChecked](bool ok, QString docTitle, QString,
                                                            QVector<QPair<QString, QString>> sheets, QString error) {
        QApplication::restoreOverrideCursor();
        setEnabled(true);

        if (!ok) {
            m_statusLabel->setText(tr("Read failed: %1").arg(error));
            emit errorOccurred(tr("读取 Google 表格失败"), error);
            return;
        }

        QStringList names;
        for (const auto& p : sheets) names << p.second;
        // 已有保存勾选的表（m_activeTreeId 非空）保留原勾选，新表默认全选
        fillSheetList(names, previouslyChecked, m_activeTreeId.isEmpty() || previouslyChecked.isEmpty());
        if (m_aliasEdit->text().trimmed().isEmpty()) m_aliasEdit->setText(docTitle);
        m_statusLabel->setText(tr("Found %1 sheets. Check the ones you need, then click \"Import Checked Sheets\".").arg(names.size()));
    });
}

void GoogleSheetsDialog::onImportChecked() {
    const QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) {
        m_statusLabel->setText(tr("Paste a Google Sheets link first."));
        return;
    }
    const QStringList names = checkedNames();
    if (names.isEmpty()) {
        m_statusLabel->setText(tr("Click \"Read Sheets\" and check at least one sheet first."));
        return;
    }

    setEnabled(false);
    m_statusLabel->setText(tr("Importing %1 sheets, please wait...").arg(names.size()));
    QApplication::setOverrideCursor(Qt::WaitCursor);

    m_store->importGoogleSheet(&m_service, url, m_aliasEdit->text().trimmed(), m_activeTreeId, names,
        [this, count = names.size()](const QString& treeId, const QString& error) {
            QApplication::restoreOverrideCursor();
            setEnabled(true);

            if (treeId.isEmpty()) {
                m_statusLabel->setText(tr("Import failed: %1").arg(error));
                emit errorOccurred(tr("导入 Google 表格失败"), error);
                return;
            }

            m_activeTreeId = treeId;
            FolderNode* root = m_store->findFolderById(treeId);
            const int items = root ? m_store->countItemsInFolder(*root) : 0;
            reloadSavedList();
            selectTree(treeId);
            m_statusLabel->setText(tr("Imported %1 sheets, %2 snippets in total.").arg(count).arg(items));
        });
}

void GoogleSheetsDialog::onSetAllChecked(bool checked) {
    for (int i = 0; i < m_sheetList->count(); ++i)
        m_sheetList->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

void GoogleSheetsDialog::onInvertAll() {
    for (int i = 0; i < m_sheetList->count(); ++i) {
        auto* item = m_sheetList->item(i);
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    }
}

void GoogleSheetsDialog::onRemoveSaved() {
    auto* cur = m_savedList->currentItem();
    if (!cur) {
        m_statusLabel->setText(tr("Select a saved table on the left first."));
        return;
    }
    const QString treeId = cur->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("Confirm Remove"),
                              tr("This also deletes everything of this table from the tree. Continue?")) != QMessageBox::Yes)
        return;

    m_store->deleteNode(treeId);     // 同步移除 google_sheets 配置
    m_store->removeGoogleSheet(treeId);
    reloadSavedList();
    onNewSheet();
}

void GoogleSheetsDialog::onRefreshSaved() {
    auto* cur = m_savedList->currentItem();
    if (!cur) {
        m_statusLabel->setText(tr("Select a saved table on the left first."));
        return;
    }
    auto* gs = m_store->findGoogleSheetByTreeId(cur->data(Qt::UserRole).toString());
    if (!gs) return;
    const QString url = gs->url, title = gs->title, treeId = gs->treeId;
    const QStringList selected = gs->selectedSheets;

    setEnabled(false);
    m_statusLabel->setText(tr("Refreshing with the saved selection..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_store->importGoogleSheet(&m_service, url, title, treeId, selected,
        [this](const QString& id, const QString& error) {
            QApplication::restoreOverrideCursor();
            setEnabled(true);

            if (id.isEmpty()) {
                m_statusLabel->setText(tr("Refresh failed: %1").arg(error));
                emit errorOccurred(tr("刷新 Google 表格失败"), error);
                return;
            }
            reloadSavedList();
            selectTree(id);
            m_statusLabel->setText(tr("Refreshed."));
        });
}
