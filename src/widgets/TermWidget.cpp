#include "TermWidget.h"
#include "TermCardDelegate.h"
#include "TermListModel.h"
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include "../utils/Logger.h"

TermWidget::TermWidget(QWidget *parent) : QWidget(parent) {
    setupUi();
}

void TermWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 普通工具栏：搜索 + 添加
    auto* normalBar = new QWidget(this);
    auto* normalLayout = new QHBoxLayout(normalBar);
    normalLayout->setContentsMargins(0, 0, 0, 0);
    m_searchEdit = new QLineEdit(normalBar);
    m_searchEdit->setClearButtonEnabled(true);
    m_heardEdit = new QLineEdit(normalBar);
    m_correctEdit = new QLineEdit(normalBar);
    m_addBtn = new QPushButton(normalBar);
    m_moreBtn = new QToolButton(normalBar);
    m_moreBtn->setText(QStringLiteral("⋯"));
    m_moreBtn->setPopupMode(QToolButton::InstantPopup);
    auto* moreMenu = new QMenu(m_moreBtn);
    m_moreBtn->setMenu(moreMenu);
    normalLayout->addWidget(m_searchEdit, 1);
    normalLayout->addSpacing(12);
    normalLayout->addWidget(m_heardEdit, 1);
    normalLayout->addWidget(new QLabel(QStringLiteral("→"), normalBar));
    normalLayout->addWidget(m_correctEdit, 1);
    normalLayout->addWidget(m_addBtn);
    normalLayout->addWidget(m_moreBtn);

    // 多选工具栏：左边选择，右边操作
    auto* selectBar = new QWidget(this);
    auto* selectLayout = new QHBoxLayout(selectBar);
    selectLayout->setContentsMargins(0, 0, 0, 0);
    m_selectAllBtn = new QPushButton(selectBar);
    m_reverseBtn = new QPushButton(selectBar);
    m_countLabel = new QLabel(selectBar);
    m_enableBtn = new QPushButton(selectBar);
    m_disableBtn = new QPushButton(selectBar);
    m_deleteBtn = new QPushButton(selectBar);
    m_doneBtn = new QPushButton(selectBar);
    selectLayout->addWidget(m_selectAllBtn);
    selectLayout->addWidget(m_reverseBtn);
    selectLayout->addSpacing(8);
    selectLayout->addWidget(m_countLabel);
    selectLayout->addStretch();
    selectLayout->addWidget(m_enableBtn);
    selectLayout->addWidget(m_disableBtn);
    selectLayout->addWidget(m_deleteBtn);
    selectLayout->addWidget(m_doneBtn);

    m_barStack = new QStackedWidget(this);
    m_barStack->addWidget(normalBar);
    m_barStack->addWidget(selectBar);
    m_barStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_list = new TermListView(this);
    m_list->setObjectName("terms_list");
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);   // 双击编辑由 onDoubleClicked 自己处理
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_delegate = new TermCardDelegate(m_list);
    m_list->setItemDelegate(m_delegate);

    mainLayout->addWidget(m_barStack);
    mainLayout->addWidget(m_list, 1);

    m_selectAnim = new QVariantAnimation(this);
    m_selectAnim->setDuration(180);
    m_selectAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_selectAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_delegate->setSelectProgress(v.toReal());
        m_list->viewport()->update();
    });

    connect(m_addBtn, &QPushButton::clicked, this, &TermWidget::handleAdd);
    connect(m_heardEdit, &QLineEdit::returnPressed, this, &TermWidget::handleAdd);
    connect(m_correctEdit, &QLineEdit::returnPressed, this, &TermWidget::handleAdd);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &TermWidget::handleSelectAll);
    connect(m_reverseBtn, &QPushButton::clicked, this, &TermWidget::handleReverseSelect);
    connect(m_enableBtn, &QPushButton::clicked, this, [this]() { handleSetEnabled(true); });
    connect(m_disableBtn, &QPushButton::clicked, this, [this]() { handleSetEnabled(false); });
    connect(m_deleteBtn, &QPushButton::clicked, this, &TermWidget::handleDelete);
    connect(m_doneBtn, &QPushButton::clicked, this, [this]() { setSelectMode(false); });
    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    connect(m_clickTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingClick.isValid()) toggleChecked(m_pendingClick);
        m_pendingClick = QPersistentModelIndex();
    });
    connect(m_list, &QListView::clicked, this, &TermWidget::onClicked);
    connect(m_list, &QListView::doubleClicked, this, &TermWidget::onDoubleClicked);

    retranslateUi();
}

void TermWidget::setTermsManager(TermsLibraryManager* termsManager) {
    m_termsManager = termsManager;
    m_model = new TermListModel(m_termsManager, this);
    connect(m_model, &TermListModel::errorOccurred, this, [this](const QString& cause) {
        emit errorOccurred(tr("输入有误"), cause);
    });
    connect(m_model, &TermListModel::checkedChanged, this, &TermWidget::updateSelectBar);

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_list->setModel(m_proxy);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxy->setFilterFixedString(text.trimmed());
    });
}

void TermWidget::onClicked(const QModelIndex& proxyIndex) {
    // 双击的第二下松开时还会再来一次 clicked，此时编辑器已打开，忽略
    if (!proxyIndex.isValid() || m_list->indexWidget(proxyIndex)) return;

    // 多选中单击就是勾选，立即响应；平时单击等一个双击间隔，看是不是双击
    if (m_selectMode) {
        toggleChecked(proxyIndex);
        return;
    }
    m_pendingClick = proxyIndex;
    m_clickTimer->start(QApplication::doubleClickInterval());
}

void TermWidget::onDoubleClicked(const QModelIndex& proxyIndex) {
    // 多选中只做勾选，不进入编辑
    if (!proxyIndex.isValid() || m_selectMode) return;
    m_clickTimer->stop();
    m_pendingClick = QPersistentModelIndex();
    m_list->edit(proxyIndex);
}

void TermWidget::toggleChecked(const QModelIndex& proxyIndex) {
    const int row = m_proxy->mapToSource(proxyIndex).row();
    const QList<int> before = m_model->checkedRows();
    QSet<int> checked(before.begin(), before.end());
    if (!checked.remove(row)) checked.insert(row);
    m_model->setChecked(checked);
    setSelectMode(!checked.isEmpty());
}

void TermWidget::setSelectMode(bool on) {
    if (on == m_selectMode) return;
    m_selectMode = on;
    if (!on) m_model->setChecked({});
    m_barStack->setCurrentIndex(on ? 1 : 0);

    m_selectAnim->stop();
    m_selectAnim->setStartValue(m_selectAnim->currentValue().isValid() ? m_selectAnim->currentValue() : QVariant(on ? 0.0 : 1.0));
    m_selectAnim->setEndValue(on ? 1.0 : 0.0);
    m_selectAnim->start();
}

void TermWidget::updateSelectBar() {
    const int n = m_model->checkedRows().size();
    m_countLabel->setText(tr("%1 selected").arg(n));
    for (QPushButton* b : { m_enableBtn, m_disableBtn, m_deleteBtn }) b->setEnabled(n > 0);
}

QList<int> TermWidget::visibleSourceRows() const {
    QList<int> rows;
    for (int i = 0; i < m_proxy->rowCount(); ++i) rows << m_proxy->mapToSource(m_proxy->index(i, 0)).row();
    return rows;
}

void TermWidget::handleAdd() {
    if (m_model->addTerm(m_heardEdit->text(), m_correctEdit->text())) {
        m_heardEdit->clear();
        m_correctEdit->clear();
        m_heardEdit->setFocus();
        m_list->scrollToBottom();
    }
}

void TermWidget::handleSelectAll() {
    const QList<int> rows = visibleSourceRows();
    m_model->setChecked(QSet<int>(rows.begin(), rows.end()));
}

void TermWidget::handleReverseSelect() {
    QSet<int> next;
    for (int row : visibleSourceRows()) {
        if (!m_model->isChecked(row)) next.insert(row);
    }
    m_model->setChecked(next);
}

void TermWidget::handleSetEnabled(bool enabled) {
    m_termsManager->setEnabled(m_model->checkedRows(), enabled);
}

void TermWidget::handleDelete() {
    const QList<int> rows = m_model->checkedRows();
    if (rows.isEmpty()) return;
    auto res = QMessageBox::question(this, tr("Confirm"), tr("Delete %1 selected rows?").arg(rows.size()),
                                     QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;
    m_termsManager->removeItems(rows);
    setSelectMode(false);
}

void TermWidget::handleReload() {
    if (m_termsManager->importFromFile(m_termsManager->defaultPath())) {
        LOG_INFO("Terms reloaded successfully.");
    }
    else {
        emit errorOccurred(tr("术语库加载失败"), tr("未找到 terms.tsv"));
    }
}

void TermWidget::handleCheckConflicts() {
    QStringList conflicts = m_termsManager->checkConflicts();
    if (conflicts.isEmpty()) {
        emit errorOccurred(tr("术语冲突检查"), tr("未发现冲突"));
    }
    else {
        emit errorOccurred(tr("检测到术语冲突"), conflicts.join("\n"));
    }
}

void TermWidget::handleImport() {
    QString path = QFileDialog::getOpenFileName(this, tr("Import TSV"), "", "TSV Files (*.tsv)");
    if (path.isEmpty()) return;
    if (m_termsManager->importFromFile(path)) {
        emit errorOccurred(tr("术语库导入成功"));
    }
    else {
        emit errorOccurred(tr("术语库导入失败"), tr("文件读写失败，请检查路径与权限"));
    }
}

void TermWidget::handleExport() {
    QString path = QFileDialog::getSaveFileName(this, tr("Export TSV"), "", "TSV Files (*.tsv)");
    if (path.isEmpty()) return;
    if (m_termsManager->exportToFile(path)) {
        emit errorOccurred(tr("术语库导出成功"));
    }
    else {
        emit errorOccurred(tr("术语库导出失败"), tr("文件读写失败，请检查路径与权限"));
    }
}

void TermWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && m_selectMode) {
        setSelectMode(false);
        return;
    }
    QWidget::keyPressEvent(event);
}

void TermWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void TermWidget::retranslateUi()
{
    m_searchEdit->setPlaceholderText(tr("Search terms..."));
    m_heardEdit->setPlaceholderText(tr("Heard as (optional)"));
    m_heardEdit->setToolTip(tr("What the recognizer mistakenly outputs; separate several with commas. "
                               "Leave empty to just teach the recognizer a new word, e.g. a name."));
    m_correctEdit->setPlaceholderText(tr("Write as"));
    m_addBtn->setText(tr("+ Add"));
    m_moreBtn->setToolTip(tr("More"));

    QMenu* menu = m_moreBtn->menu();
    menu->clear();
    menu->addAction(tr("Reload Terms"), this, &TermWidget::handleReload);
    menu->addAction(tr("Check Conflicts"), this, &TermWidget::handleCheckConflicts);
    menu->addSeparator();
    menu->addAction(tr("Import TSV"), this, &TermWidget::handleImport);
    menu->addAction(tr("Export TSV"), this, &TermWidget::handleExport);

    m_selectAllBtn->setText(tr("Select All"));
    m_reverseBtn->setText(tr("Reverse Select"));
    m_enableBtn->setText(tr("Enable"));
    m_disableBtn->setText(tr("Disable"));
    m_deleteBtn->setText(tr("Delete"));
    m_doneBtn->setText(tr("Done"));
    if (m_model) updateSelectBar();
}
