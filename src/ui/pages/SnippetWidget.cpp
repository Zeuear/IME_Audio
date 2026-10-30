#include "SnippetWidget.h"

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>

#include <QTreeView>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <functional>

#include "snippets/GoogleSheetsService.h"
#include "snippets/SnippetStore.h"
#include "ui/controls/GoogleSheetsDialog.h"
#include "ui/controls/ShortcutEdit.h"
#include "ui/controls/SnippetContextMenu.h"
#include "ui/controls/ThemedIcon.h"
#include "ui/delegates/SnippetTreeDelegate.h"
#include "ui/models/SnippetTreeModel.h"

SnippetWidget::SnippetWidget(QWidget *parent) : QWidget(parent) {
    setupUi();
    retranslateUi();
}

void SnippetWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);   // 和 TermWidget、基本设置页的页边距一致
    mainLayout->setSpacing(8);

    // 图标按钮统一宽度，窄窗口下两行工具栏也放得下
    constexpr int kIconBtnWidth = 36;
    auto makeIconBtn = [this]() {
        auto* btn = new QPushButton(this);
        btn->setFixedWidth(kIconBtnWidth);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    // 第一行：来源管理 + 选项（低频设置收进弹出面板）
    auto* sourceRow = new QHBoxLayout();
    m_addBtn = new QPushButton(this);
    m_googleBtn = new QPushButton(this);
    m_refreshBtn = makeIconBtn();
    m_optionsBtn = makeIconBtn();
    sourceRow->addWidget(m_addBtn);
    sourceRow->addWidget(m_googleBtn);
    sourceRow->addWidget(m_refreshBtn);
    sourceRow->addStretch();
    sourceRow->addWidget(m_optionsBtn);
    mainLayout->addLayout(sourceRow);

    // 选项面板：勾选项、最多行数、搜索框热键。
    // ShortcutEdit 常驻在面板里，全局热键的注册不依赖面板是否显示
    m_voiceTriggerChk = new QCheckBox(this);
    m_imageFirstChk = new QCheckBox(this);
    m_linesLabel = new QLabel(this);
    m_maxLinesSpin = new QSpinBox(this);
    m_maxLinesSpin->setRange(1, 12);
    m_hotkeyLabel = new QLabel(this);
    m_hotkeyEdit = new ShortcutEdit(this);

    auto* panel = new QWidget(this);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(12, 10, 12, 10);
    panelLayout->setSpacing(8);
    panelLayout->addWidget(m_voiceTriggerChk);
    panelLayout->addWidget(m_imageFirstChk);
    auto* linesRow = new QHBoxLayout();
    linesRow->addWidget(m_linesLabel);
    linesRow->addWidget(m_maxLinesSpin);
    linesRow->addStretch();
    panelLayout->addLayout(linesRow);
    panelLayout->addWidget(m_hotkeyLabel);
    panelLayout->addWidget(m_hotkeyEdit);

    m_optionsMenu = new QMenu(this);
    auto* panelAction = new QWidgetAction(m_optionsMenu);
    panelAction->setDefaultWidget(panel);
    m_optionsMenu->addAction(panelAction);

    // 第二行：检索 + 视图与批量操作
    auto* toolRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(180);
    m_searchAction = m_searchEdit->addAction(QIcon(), QLineEdit::LeadingPosition);
    m_expandBtn = makeIconBtn();
    m_collapseBtn = makeIconBtn();
    m_multilineBtn = makeIconBtn();
    m_multilineBtn->setCheckable(true);
    m_deleteBtn = makeIconBtn();
    m_moreBtn = makeIconBtn();
    toolRow->addWidget(m_searchEdit, 1);
    toolRow->addWidget(m_expandBtn);
    toolRow->addWidget(m_collapseBtn);
    toolRow->addWidget(m_multilineBtn);
    toolRow->addWidget(m_deleteBtn);
    toolRow->addWidget(m_moreBtn);
    mainLayout->addLayout(toolRow);

    m_moreMenu = new QMenu(this);
    m_selectAllAct = m_moreMenu->addAction(QString());
    m_invertAct = m_moreMenu->addAction(QString());

    m_treeView = new QTreeView(this);
    m_treeView->setRootIsDecorated(false);   // 展开箭头由委托在行内自己画，根级不留空白的分支列
    m_treeView->setHeaderHidden(true);       // 整行只用第 0 列绘制，不需要表头
    m_treeView->setIndentation(22);          // 子级相对父级的缩进
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setUniformRowHeights(false);   // 条目行高随内容换行变化
    m_treeView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mainLayout->addWidget(m_treeView, 1);

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);

    // 菜单右对齐到按钮下沿，避免超出窗口右侧
    auto popupBelow = [](QMenu* menu, QWidget* button) {
        menu->popup(button->mapToGlobal(QPoint(button->width() - menu->sizeHint().width(), button->height())));
    };

    m_relayoutTimer.setSingleShot(true);
    connect(&m_relayoutTimer, &QTimer::timeout, this, &SnippetWidget::onResizeTreeLayout);

    connect(m_addBtn, &QPushButton::clicked, this, &SnippetWidget::onAddLevel1Folder);
    connect(m_googleBtn, &QPushButton::clicked, this, [this]() { onOpenGoogleSheets(); });
    connect(m_refreshBtn, &QPushButton::clicked, this, &SnippetWidget::onRefreshAllSheets);
    connect(m_optionsBtn, &QPushButton::clicked, this, [this, popupBelow]() { popupBelow(m_optionsMenu, m_optionsBtn); });
    connect(m_moreBtn, &QPushButton::clicked, this, [this, popupBelow]() { popupBelow(m_moreMenu, m_moreBtn); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SnippetWidget::onSearchTextChanged);
    connect(m_expandBtn, &QPushButton::clicked, this, [this]() { onExpandCollapseAll(true); });
    connect(m_collapseBtn, &QPushButton::clicked, this, [this]() { onExpandCollapseAll(false); });
    connect(m_selectAllAct, &QAction::triggered, this, &SnippetWidget::onSelectAll);
    connect(m_invertAct, &QAction::triggered, this, &SnippetWidget::onInvertSelection);
    connect(m_deleteBtn, &QPushButton::clicked, this, &SnippetWidget::onBatchDeleteSelected);
    connect(m_treeView, &QTreeView::doubleClicked, this, &SnippetWidget::onItemDoubleClicked);
    connect(m_treeView, &QTreeView::clicked, this, &SnippetWidget::onTreeClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &SnippetWidget::onContextMenu);
    connect(m_treeView, &QTreeView::expanded, this, [this](const QModelIndex& i) { onTreeExpandedChanged(i, true); });
    connect(m_treeView, &QTreeView::collapsed, this, [this](const QModelIndex& i) { onTreeExpandedChanged(i, false); });
}

void SnippetWidget::retranslateUi() {
    m_addBtn->setText(tr("New Table"));
    m_googleBtn->setText(tr("Google Sheets Import"));
    m_refreshBtn->setToolTip(tr("Refresh Google Sheets"));
    m_optionsBtn->setToolTip(tr("Options"));
    m_voiceTriggerChk->setText(tr("Suggest when keyword is spoken"));
    m_voiceTriggerChk->setToolTip(tr("When a snippet's keyword appears in your dictation, the floating box shows a button to paste it."));
    m_imageFirstChk->setText(tr("Image before text"));
    m_searchEdit->setPlaceholderText(tr("Search title / text / keyword / folder..."));
    m_expandBtn->setToolTip(tr("Expand All"));
    m_collapseBtn->setToolTip(tr("Collapse All"));
    m_multilineBtn->setToolTip(tr("Multiline"));
    m_linesLabel->setText(tr("Max lines:"));
    m_deleteBtn->setToolTip(tr("Delete Selected"));
    m_moreBtn->setToolTip(tr("More"));
    m_selectAllAct->setText(tr("Select All"));
    m_invertAct->setText(tr("Invert"));
    m_hotkeyLabel->setText(tr("Search popup hotkey (leave empty to disable):"));
    if (m_model) {
        emit m_model->headerDataChanged(Qt::Horizontal, 0, 2);
        updateStatus();
    }
}

// 图标是单色描边 SVG，按按钮当前文字色着色。
// QSS 主题切换后控件调色板才会更新，所以在 PaletteChange / StyleChange / 显示时都要重画
void SnippetWidget::updateIcons() {
    if (!m_searchAction) return;   // 构造期间 PaletteChange 可能先于控件创建完成到达
    const QColor color = m_addBtn->palette().buttonText().color();
    m_addBtn->setIcon(themedIcon("snippet_add", color));
    m_googleBtn->setIcon(themedIcon("snippet_cloud", color));
    m_refreshBtn->setIcon(themedIcon("snippet_refresh", color));
    m_optionsBtn->setIcon(themedIcon("snippet_options", color));
    m_expandBtn->setIcon(themedIcon("snippet_expand", color));
    m_collapseBtn->setIcon(themedIcon("snippet_collapse", color));
    m_multilineBtn->setIcon(themedIcon("snippet_multiline", color));
    m_deleteBtn->setIcon(themedIcon("snippet_trash", color));
    m_moreBtn->setIcon(themedIcon("snippet_more", color));
    m_searchAction->setIcon(themedIcon("snippet_search", m_searchEdit->palette().text().color(), 16));
}

void SnippetWidget::showEvent(QShowEvent* event) {
    updateIcons();
    QWidget::showEvent(event);
}

void SnippetWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    else if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) updateIcons();
    QWidget::changeEvent(event);
}

void SnippetWidget::setSnippetStore(SnippetStore* store) {
    m_store = store;
    m_model = new SnippetTreeModel(m_store, this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterRole(SnippetTreeModel::SearchRole);
    m_proxy->setRecursiveFilteringEnabled(true);
    m_proxy->setAutoAcceptChildRows(true);
    m_treeView->setModel(m_proxy);

    // 整行由委托画在第 0 列里，其余两列隐藏；第 0 列铺满视口，换行宽度才跟着窗口走
    m_treeView->setColumnHidden(1, true);
    m_treeView->setColumnHidden(2, true);
    QHeaderView* header = m_treeView->header();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    m_delegate = new SnippetTreeDelegate(m_store, m_treeView);
    m_treeView->setItemDelegate(m_delegate);

    const AppSettings& settings = m_store->settings();
    m_multilineBtn->setChecked(settings.multilineEnabled);
    m_maxLinesSpin->setValue(settings.maxDisplayLines);
    m_voiceTriggerChk->setChecked(settings.voiceTrigger);
    m_imageFirstChk->setChecked(settings.comboImageFirst);

    connect(m_multilineBtn, &QAbstractButton::toggled, this, [this](bool on) {
        m_store->settings().multilineEnabled = on;
        m_store->save();
        m_treeView->doItemsLayout();
    });
    connect(m_maxLinesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_store->settings().maxDisplayLines = v;
        m_store->save();
        m_treeView->doItemsLayout();
    });
    connect(m_voiceTriggerChk, &QCheckBox::toggled, this, [this](bool on) {
        m_store->settings().voiceTrigger = on;
        m_store->save();
        emit storeChanged();
    });
    connect(m_imageFirstChk, &QCheckBox::toggled, this, [this](bool on) {
        m_store->settings().comboImageFirst = on;
        m_store->save();
        emit storeChanged();
    });

    m_hotkeyEdit->setShortCut(QString::fromStdString(settings.hotkey));
    connect(m_hotkeyEdit, &ShortcutEdit::hotkeyActivated, this, &SnippetWidget::searchPopupRequested);
    connect(m_hotkeyEdit, &ShortcutEdit::shortcutChanged, this, [this](const QString& shortcut) {
        m_store->settings().hotkey = shortcut.toStdString();
        m_store->save();
    });

    m_contextMenu = new SnippetContextMenu(m_store, m_model, this);
    connect(m_contextMenu, &SnippetContextMenu::changed, this, [this]() {
        applyExpandState();
        updateStatus();
        emit storeChanged();
    });
    connect(m_contextMenu, &SnippetContextMenu::errorOccurred, this, &SnippetWidget::errorOccurred);
    connect(m_contextMenu, &SnippetContextMenu::requestGoogleSheetsDialog, this,
            [this](const QString& id) { onOpenGoogleSheets(id); });
    connect(m_contextMenu, &SnippetContextMenu::requestSelectAll, this, &SnippetWidget::onSelectAll);
    connect(m_contextMenu, &SnippetContextMenu::requestInvertSelection, this, &SnippetWidget::onInvertSelection);

    applyExpandState();
    updateStatus();
}

QModelIndex SnippetWidget::toSource(const QModelIndex& proxyIndex) const {
    return proxyIndex.isValid() ? m_proxy->mapToSource(proxyIndex) : QModelIndex();
}

void SnippetWidget::updateStatus() {
    if (!m_store) return;
    int itemCount = 0;
    for (const auto& root : m_store->roots()) itemCount += m_store->countItemsInFolder(*root);
    m_statusLabel->setText(tr("%1 snippets · %2 Google Sheets synced")
                               .arg(itemCount).arg(m_store->googleSheets().size()));
}

void SnippetWidget::refreshTree() {
    m_model->refresh();
    applyExpandState();
    updateStatus();
    emit storeChanged();
}

// 把 FolderNode::collapsed 应用到视图；过滤状态下全部展开
void SnippetWidget::applyExpandState() {
    m_applyingExpand = true;
    if (!m_proxy->filterRegularExpression().pattern().isEmpty()) {
        m_treeView->expandAll();
    } else {
        std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& srcParent) {
            const int rows = m_model->rowCount(srcParent);
            for (int r = 0; r < rows; ++r) {
                QModelIndex src = m_model->index(r, 0, srcParent);
                if (!m_model->isFolder(src)) continue;
                const FolderNode* f = static_cast<const FolderNode*>(src.internalPointer());
                m_treeView->setExpanded(m_proxy->mapFromSource(src), !f->collapsed);
                walk(src);
            }
        };
        walk(QModelIndex());
    }
    m_applyingExpand = false;
}

void SnippetWidget::onTreeExpandedChanged(const QModelIndex& proxyIndex, bool expanded) {
    if (m_applyingExpand || !m_proxy->filterRegularExpression().pattern().isEmpty()) return;
    QModelIndex src = toSource(proxyIndex);
    if (!src.isValid() || !m_model->isFolder(src)) return;
    FolderNode* f = static_cast<FolderNode*>(src.internalPointer());
    f->collapsed = !expanded;
    m_store->save();
}

void SnippetWidget::onSearchTextChanged(const QString& text) {
    m_proxy->setFilterFixedString(text.trimmed().toLower());
    applyExpandState();
}

void SnippetWidget::onAddLevel1Folder() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Table"), tr("Table name:"),
                                               QLineEdit::Normal, tr("New Table"), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_store->addFolder(QString(), name, 1);
        refreshTree();
    }
}

void SnippetWidget::onOpenGoogleSheets(const QString& treeId) {
    GoogleSheetsDialog dlg(m_store, this);
    connect(&dlg, &GoogleSheetsDialog::errorOccurred, this, &SnippetWidget::errorOccurred);
    if (!treeId.isEmpty()) dlg.selectTree(treeId);
    dlg.exec();
    refreshTree();
}

void SnippetWidget::onRefreshAllSheets() {
    if (m_store->googleSheets().isEmpty()) {
        emit errorOccurred(tr("没有已保存的 Google 表格"), tr("请先导入一个 Google 表格"));
        return;
    }
    GoogleSheetsService service;
    QStringList errors;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const int okCount = m_store->refreshAllGoogleSheets(&service, &errors);
    QApplication::restoreOverrideCursor();
    refreshTree();

    if (!errors.isEmpty()) {
        emit errorOccurred(tr("部分 Google 表格刷新失败"), errors.join('\n'));
    } else {
        emit errorOccurred(tr("刷新成功"), tr("已刷新 %1 个 Google 表格").arg(okCount));
    }
}

void SnippetWidget::onExpandCollapseAll(bool expand) {
    m_store->setAllCollapsed(!expand, 3);
    applyExpandState();
}

void SnippetWidget::onBatchDeleteSelected() {
    QStringList ids;
    for (const QModelIndex& pi : m_treeView->selectionModel()->selectedRows(0))
        ids << m_model->nodeIdFromIndex(toSource(pi));
    ids.removeAll(QString());

    if (ids.isEmpty()) {
        emit errorOccurred(tr("未选择条目"), tr("请先选择要删除的条目"));
        return;
    }
    if (QMessageBox::question(this, tr("Confirm Delete"),
                              tr("Delete the %1 selected nodes? Folders are deleted with their contents.").arg(ids.size()))
        != QMessageBox::Yes)
        return;

    m_store->deleteNodes(ids);
    refreshTree();
}

void SnippetWidget::onSelectAll() {
    m_treeView->selectAll();
}

void SnippetWidget::onInvertSelection() {
    QItemSelectionModel* sm = m_treeView->selectionModel();
    std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& parent) {
        const int rows = m_proxy->rowCount(parent);
        for (int r = 0; r < rows; ++r) {
            QModelIndex idx = m_proxy->index(r, 0, parent);
            sm->select(idx, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
            walk(idx);
        }
    };
    walk(QModelIndex());
}

void SnippetWidget::onContextMenu(const QPoint& pos) {
    QModelIndex idx = m_treeView->indexAt(pos);
    m_contextMenu->showAt(m_treeView->viewport()->mapToGlobal(pos), m_treeView, toSource(idx));
}

// 光标在这一行的哪个区域（标题 / 内容 / 快捷码 / 配图）
SnippetTreeDelegate::Zone SnippetWidget::zoneUnderCursor(const QModelIndex& proxyIndex) const {
    QStyleOptionViewItem option;
    option.initFrom(m_treeView);
    option.font = m_treeView->font();   // 和绘制时一致，换行位置才算得准
    option.widget = m_treeView;
    option.rect = m_treeView->visualRect(proxyIndex.siblingAtColumn(0));
    return m_delegate->zoneAt(option, proxyIndex.siblingAtColumn(0),
                              m_treeView->viewport()->mapFromGlobal(QCursor::pos()));
}

void SnippetWidget::onResizeTreeLayout() {
    // 内容换行宽度随窗口变化，行高要重新算
    if (m_treeView) m_treeView->doItemsLayout();
}

void SnippetWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_relayoutTimer.start(50);
}

void SnippetWidget::onTreeClicked(const QModelIndex& proxyIndex) {
    if (!proxyIndex.isValid()) return;
    QModelIndex src = toSource(proxyIndex);
    if (!m_model->isFolder(src)) {
        // 点配图区：弹出该条目的右键菜单（选文件 / 粘贴剪贴板图 / 清除）
        if (m_delegate && zoneUnderCursor(proxyIndex) == SnippetTreeDelegate::Zone::Image)
            m_contextMenu->showAt(QCursor::pos(), m_treeView, src);
        return;
    }
    // 单击文件夹行 = 展开/收起（仅在没有多选修饰键时）
    if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) return;
    const QModelIndex first = proxyIndex.siblingAtColumn(0);
    m_treeView->setExpanded(first, !m_treeView->isExpanded(first));
}

void SnippetWidget::onItemDoubleClicked(const QModelIndex& proxyIndex) {
    if (!proxyIndex.isValid()) return;
    QModelIndex src = toSource(proxyIndex);
    const QString nodeId = m_model->nodeIdFromIndex(src);
    // 整行只有第 0 列，要编辑哪个字段由双击的位置决定
    int col = 0;
    switch (zoneUnderCursor(proxyIndex)) {
    case SnippetTreeDelegate::Zone::Text:    col = 1; break;
    case SnippetTreeDelegate::Zone::Keyword: col = 2; break;
    case SnippetTreeDelegate::Zone::Image:   return;   // 单击已弹出配图菜单
    default: break;
    }
    bool ok = false;

    if (m_model->isFolder(src)) {
        FolderNode* f = m_store->findFolderById(nodeId);
        if (!f) return;
        const QString v = QInputDialog::getText(this, tr("Rename Folder"), tr("New name:"),
                                                QLineEdit::Normal, QString::fromStdString(f->title), &ok);
        if (ok && !v.trimmed().isEmpty()) {
            m_store->updateNode(nodeId, "title", v.trimmed());
            refreshTree();
        }
        return;
    }

    SnippetItem* it = m_store->findItemById(nodeId);
    if (!it) return;
    if (col == 0) {
        const QString v = QInputDialog::getText(this, tr("Edit Title"), tr("New title:"),
                                                QLineEdit::Normal, QString::fromStdString(it->title), &ok);
        if (ok && !v.trimmed().isEmpty()) {
            m_store->updateNode(nodeId, "title", v.trimmed());
            refreshTree();
        }
    } else if (col == 1) {
        const QString v = QInputDialog::getMultiLineText(this, tr("Edit Text"), tr("Text to paste:"),
                                                         QString::fromStdString(it->text), &ok);
        if (ok) {
            m_store->updateNode(nodeId, "text", v);
            refreshTree();
        }
    } else {
        const QString v = QInputDialog::getText(this, tr("Edit Keyword"), tr("Spoken keyword, e.g. quote:"),
                                                QLineEdit::Normal, QString::fromStdString(it->keyword), &ok);
        if (ok) {
            m_store->updateNode(nodeId, "keyword", v.trimmed());
            refreshTree();
        }
    }
}
