#include "TermWidget.h"
#include "ui/controls/DictionaryProfileDialog.h"
#include "ui/delegates/DictionaryProfileDelegate.h"
#include "ui/delegates/TermCardDelegate.h"
#include "ui/models/DictionaryProfileModel.h"
#include "ui/models/TermListModel.h"
#include "services/dictionary/DictionaryLanguages.h"
#include "services/dictionary/EntryCheck.h"
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include "utils/Logger.h"

TermWidget::TermWidget(QWidget *parent) : QWidget(parent) {
    setupUi();
}

QWidget* TermWidget::buildSidePanel() {
    auto* card = new QFrame(this);
    card->setProperty("card", true);
    card->setFixedWidth(220);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* tabs = new QFrame(card);
    tabs->setObjectName(QStringLiteral("dict_kind_tabs"));
    tabs->setFixedHeight(32);
    tabs->setStyleSheet(QStringLiteral(
        "QFrame#dict_kind_tabs { border: 1px solid palette(mid); border-radius: 6px; }"
        "QFrame#dict_kind_tabs QPushButton { border: none; border-radius: 0; background: transparent; padding: 0 6px; }"
        "QFrame#dict_kind_tabs QPushButton:hover { background: rgba(128, 128, 128, 45); }"
        "QFrame#dict_kind_tabs QPushButton:checked { background: palette(highlight); color: palette(highlighted-text); }"
        "QFrame#dict_kind_tabs QPushButton[seg=\"first\"] { border-top-left-radius: 5px; border-bottom-left-radius: 5px; }"
        "QFrame#dict_kind_tabs QPushButton[seg=\"last\"] { border-top-right-radius: 5px; border-bottom-right-radius: 5px; }"));
    auto* tabLayout = new QHBoxLayout(tabs);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    m_recognitionTab = new QPushButton(tabs);
    m_translationTab = new QPushButton(tabs);
    m_recognitionTab->setProperty("seg", "first");
    m_translationTab->setProperty("seg", "last");
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    for (QPushButton* b : { m_recognitionTab, m_translationTab }) {
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        group->addButton(b);
        tabLayout->addWidget(b, 1);
    }
    m_recognitionTab->setChecked(true);

    m_profileList = new QListView(card);
    m_profileList->setObjectName(QStringLiteral("dict_profile_list"));
    m_profileList->setFrameShape(QFrame::NoFrame);
    m_profileList->setStyleSheet(QStringLiteral("QListView#dict_profile_list { background: transparent; border: none; }"));
    m_profileList->viewport()->setAutoFillBackground(false);
    m_profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_profileList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_profileList->setMouseTracking(true);
    m_profileList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_profileList->setItemDelegate(new DictionaryProfileDelegate(m_profileList));

    m_newBtn = new QPushButton(card);
    m_newBtn->setFixedHeight(32);
    m_newBtn->setCursor(Qt::PointingHandCursor);

    layout->addWidget(tabs);
    layout->addWidget(m_profileList, 1);
    layout->addWidget(m_newBtn);

    connect(m_recognitionTab, &QPushButton::clicked, this, [this]() { selectKind(DictionaryKind::Recognition); });
    connect(m_translationTab, &QPushButton::clicked, this, [this]() { selectKind(DictionaryKind::Translation); });
    connect(m_newBtn, &QPushButton::clicked, this, &TermWidget::handleNewProfile);
    connect(m_profileList, &QWidget::customContextMenuRequested, this, &TermWidget::showProfileMenu);
    return card;
}

QWidget* TermWidget::buildContentPanel() {
    auto* content = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    auto* normalBar = new QWidget(content);
    normalBar->setObjectName("term_normal_bar");
    normalBar->setStyleSheet(QStringLiteral("QWidget#term_normal_bar { background: transparent; }"));
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
    auto* addArrow = new QLabel(QStringLiteral("→"), normalBar);
    addArrow->setAlignment(Qt::AlignCenter);
    addArrow->setFixedWidth(28);
    normalLayout->addWidget(addArrow);
    normalLayout->addWidget(m_correctEdit, 1);
    normalLayout->addWidget(m_addBtn);
    normalLayout->addWidget(m_moreBtn);

    auto* selectBar = new QWidget(content);
    selectBar->setObjectName("term_select_bar");
    selectBar->setStyleSheet(QStringLiteral("QWidget#term_select_bar { background: transparent; }"));
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

    m_barStack = new QStackedWidget(content);
    m_barStack->setObjectName("term_bar_stack");
    m_barStack->setStyleSheet(QStringLiteral("QStackedWidget#term_bar_stack { background: transparent; }"));
    m_barStack->addWidget(normalBar);
    m_barStack->addWidget(selectBar);
    m_barStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_list = new TermListView(content);
    m_list->setObjectName("terms_list");
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet(QStringLiteral("QListView#terms_list { background: transparent; border: none; }"));
    m_list->viewport()->setAutoFillBackground(false);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_delegate = new TermCardDelegate(m_list);
    m_list->setItemDelegate(m_delegate);

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_list->setModel(m_proxy);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxy->setFilterFixedString(text.trimmed());
    });

    auto* toolCard = new QFrame(content);
    toolCard->setProperty("card", true);
    auto* toolLayout = new QVBoxLayout(toolCard);
    toolLayout->setContentsMargins(14, 12, 14, 12);
    toolLayout->addWidget(m_barStack);

    m_statsLabel = new QLabel(content);
    m_noteLabel = new QLabel(content);
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setEnabled(false);

    auto* listCard = new QFrame(content);
    listCard->setProperty("card", true);
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(14, 12, 14, 8);
    listLayout->setSpacing(8);
    listLayout->addWidget(m_statsLabel);
    listLayout->addWidget(m_noteLabel);
    listLayout->addWidget(m_list, 1);

    mainLayout->addWidget(toolCard);
    mainLayout->addWidget(listCard, 1);
    return content;
}

void TermWidget::setupUi() {
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    QWidget* side = buildSidePanel();
    QWidget* content = buildContentPanel();

    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setEnabled(false);

    m_rightStack = new QStackedWidget(this);
    m_rightStack->addWidget(content);
    m_rightStack->addWidget(m_hintLabel);

    rootLayout->addWidget(side);
    rootLayout->addWidget(m_rightStack, 1);

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

void TermWidget::setDictionaryProfiles(DictionaryProfiles* profiles) {
    m_profiles = profiles;
    m_profileModel = new DictionaryProfileModel(profiles, this);
    m_profileList->setModel(m_profileModel);
    connect(m_profileList->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        showProfile(current.isValid() ? current.data(DictionaryProfileModel::IdRole).toString() : QString());
    });
    connect(m_profileModel, &QAbstractItemModel::modelReset, this, &TermWidget::restoreProfileSelection);
    selectKind(DictionaryKind::Recognition);
}

void TermWidget::selectKind(DictionaryKind kind) {
    m_kind = kind;
    m_recognitionTab->setChecked(kind == DictionaryKind::Recognition);
    m_translationTab->setChecked(kind == DictionaryKind::Translation);
    m_currentId = m_lastId[int(kind)];
    m_profileModel->setKind(kind);
}

void TermWidget::restoreProfileSelection() {
    int row = m_profileModel->rowOfId(m_currentId);
    if (row < 0) row = m_profileModel->rowCount() > 0 ? 0 : -1;
    const QString id = row >= 0 ? m_profileModel->index(row, 0).data(DictionaryProfileModel::IdRole).toString() : QString();
    {
        const QSignalBlocker blocker(m_profileList->selectionModel());
        if (row >= 0) m_profileList->setCurrentIndex(m_profileModel->index(row, 0));
        else m_profileList->clearSelection();
    }
    m_profileList->viewport()->update();
    if (id != m_shownId || !m_model) showProfile(id);
    else updateBarTexts();
}

void TermWidget::showProfile(const QString& id) {
    if (m_model) setSelectMode(false);
    m_currentId = id;
    m_lastId[int(m_kind)] = id;
    m_shownId.clear();

    const DictionaryProfile* profile = id.isEmpty() ? nullptr : m_profiles->find(id);
    TermsLibraryManager* library = profile ? m_profiles->library(id) : nullptr;
    if (!library) {
        m_proxy->setSourceModel(nullptr);
        m_model = nullptr;
        m_termsManager = nullptr;
        m_rightStack->setCurrentIndex(1);
        updateBarTexts();
        return;
    }

    TermListModel*& model = m_models[id];
    if (!model) {
        model = new TermListModel(library, this);
        connect(model, &TermListModel::errorOccurred, this, [this](const QString& cause) {
            emit errorOccurred(tr("输入有误"), cause);
        });
        connect(model, &TermListModel::checkedChanged, this, &TermWidget::updateSelectBar);
        connect(model, &QAbstractItemModel::rowsInserted, this, &TermWidget::updateStats);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &TermWidget::updateStats);
        connect(model, &QAbstractItemModel::modelReset, this, &TermWidget::updateStats);
        connect(model, &QAbstractItemModel::dataChanged, this, &TermWidget::updateStats);
    }
    model->setRequireHeard(profile->kind == DictionaryKind::Translation);
    m_termsManager = library;
    m_model = model;
    m_proxy->setSourceModel(model);
    m_shownId = id;
    m_rightStack->setCurrentIndex(0);
    m_list->scrollToTop();
    updateBarTexts();
    updateStats();
}

void TermWidget::showProfileMenu(const QPoint& pos) {
    const QModelIndex index = m_profileList->indexAt(pos);
    QMenu menu(this);
    if (!index.isValid()) {
        menu.addAction(tr("+ New Dictionary"), this, &TermWidget::handleNewProfile);
        menu.exec(m_profileList->viewport()->mapToGlobal(pos));
        return;
    }
    const QString id = index.data(DictionaryProfileModel::IdRole).toString();
    const bool enabled = index.data(DictionaryProfileModel::EnabledRole).toBool();
    menu.addAction(tr("Edit..."), this, [this, id]() { handleEditProfile(id); });
    menu.addAction(enabled ? tr("Disable") : tr("Enable"), this, [this, id, enabled]() { m_profiles->setEnabled(id, !enabled); });
    if (index.data(DictionaryProfileModel::RemovableRole).toBool()) {
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, [this, id]() { handleDeleteProfile(id); });
    }
    menu.exec(m_profileList->viewport()->mapToGlobal(pos));
}

void TermWidget::handleNewProfile() {
    if (!m_profiles) return;
    DictionaryProfileDialog dialog(m_kind, false, this);
    if (dialog.exec() != QDialog::Accepted) return;
    m_lastId[int(m_kind)] = m_profiles->addProfile(m_kind, dialog.name(), dialog.langA(), dialog.langB());
    selectKind(m_kind);
}

void TermWidget::handleEditProfile(const QString& id) {
    const DictionaryProfile* profile = m_profiles->find(id);
    if (!profile) return;
    DictionaryProfileDialog dialog(profile->kind, true, this);
    dialog.setValues(profile->name, profile->langA, profile->langB);
    if (dialog.exec() != QDialog::Accepted) return;
    m_profiles->renameProfile(id, dialog.name());
    m_profiles->setLanguages(id, dialog.langA(), dialog.langB());
}

void TermWidget::handleDeleteProfile(const QString& id) {
    const DictionaryProfile* profile = m_profiles->find(id);
    if (!profile) return;
    const auto res = QMessageBox::question(this, tr("Confirm"),
                                           tr("Delete dictionary \"%1\" and all its entries?").arg(profile->name),
                                           QMessageBox::Yes | QMessageBox::No);
    if (res != QMessageBox::Yes) return;
    if (id == m_currentId) {
        setSelectMode(false);
        m_proxy->setSourceModel(nullptr);
        m_model = nullptr;
        m_termsManager = nullptr;
        m_currentId.clear();
    }
    delete m_models.take(id);
    for (QString& last : m_lastId) {
        if (last == id) last.clear();
    }
    m_profiles->removeProfile(id);
}

void TermWidget::onClicked(const QModelIndex& proxyIndex) {
    if (!proxyIndex.isValid() || m_list->indexWidget(proxyIndex)) return;

    if (m_selectMode) {
        toggleChecked(proxyIndex);
        return;
    }
    m_pendingClick = proxyIndex;
    m_clickTimer->start(QApplication::doubleClickInterval());
}

void TermWidget::onDoubleClicked(const QModelIndex& proxyIndex) {
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
    if (!on && m_model) m_model->setChecked({});
    m_barStack->setCurrentIndex(on ? 1 : 0);

    m_selectAnim->stop();
    m_selectAnim->setStartValue(m_selectAnim->currentValue().isValid() ? m_selectAnim->currentValue() : QVariant(on ? 0.0 : 1.0));
    m_selectAnim->setEndValue(on ? 1.0 : 0.0);
    m_selectAnim->start();
}

void TermWidget::updateSelectBar() {
    if (!m_model) return;
    const int n = m_model->checkedRows().size();
    m_countLabel->setText(tr("%1 selected").arg(n));
    for (QPushButton* b : { m_enableBtn, m_disableBtn, m_deleteBtn }) b->setEnabled(n > 0);
}

void TermWidget::updateStats() {
    if (!m_statsLabel) return;
    if (!m_model) {
        m_statsLabel->clear();
        return;
    }
    const int total = m_model->rowCount();
    int enabled = 0;
    for (int i = 0; i < total; ++i) {
        if (m_model->data(m_model->index(i, 0), TermListModel::EnabledRole).toBool()) ++enabled;
    }
    const DictionaryProfile* profile = m_profiles ? m_profiles->find(m_currentId) : nullptr;
    const bool translation = profile && profile->kind == DictionaryKind::Translation;
    if (total == 0) {
        m_statsLabel->setText(translation ? tr("No terms yet. Add one above, e.g. source term → translation.")
                                          : tr("No terms yet. Add one above, e.g. heard as → write as."));
    } else {
        m_statsLabel->setText(tr("%1 terms · %2 enabled").arg(total).arg(enabled));
    }
}

QList<int> TermWidget::visibleSourceRows() const {
    QList<int> rows;
    for (int i = 0; i < m_proxy->rowCount(); ++i) rows << m_proxy->mapToSource(m_proxy->index(i, 0)).row();
    return rows;
}

void TermWidget::updateBarTexts() {
    const DictionaryProfile* profile = m_profiles ? m_profiles->find(m_currentId) : nullptr;
    const bool translation = profile ? profile->kind == DictionaryKind::Translation : m_kind == DictionaryKind::Translation;
    if (translation) {
        m_heardEdit->setPlaceholderText(tr("Source term"));
        m_heardEdit->setToolTip(tr("The term in the source language; separate aliases with commas or /."));
        m_correctEdit->setPlaceholderText(tr("Translation"));
    } else {
        m_heardEdit->setPlaceholderText(tr("Heard as (optional)"));
        m_heardEdit->setToolTip(tr("What the recognizer mistakenly outputs; separate several with commas. "
                                   "Leave empty to just teach the recognizer a new word, e.g. a name."));
        m_correctEdit->setPlaceholderText(tr("Write as"));
    }
    m_noteLabel->setVisible(translation);
    m_noteLabel->setText(tr("Used by AI translation (cloud and Ollama). The offline translator and Open WebUI ignore it."));
    m_hintLabel->setText(profile ? QString() : tr("No dictionary yet. Click \"New Dictionary\" to create one."));
    updateStats();
}

void TermWidget::handleAdd() {
    if (!m_model) return;
    const DictionaryProfile* profile = m_profiles->find(m_currentId);
    if (!profile) return;

    QString heard = m_heardEdit->text();
    QString correct = m_correctEdit->text();
    const bool translation = profile->kind == DictionaryKind::Translation;
    auto check = [&]() {
        return translation ? EntryCheck::translation(heard, correct, profile->langA, profile->langB)
                           : EntryCheck::recognition(heard, correct, profile->langA);
    };

    EntryCheck::Result result = check();
    if (result.verdict == EntryCheck::Verdict::Mismatch) {
        const auto res = QMessageBox::question(this, tr("Language mismatch"),
            tr("These words do not look like %1. Add anyway?").arg(DictionaryLanguages::label(profile->langA)),
            QMessageBox::Yes | QMessageBox::No);
        if (res != QMessageBox::Yes) return;
    } else if (result.verdict == EntryCheck::Verdict::SwapSuggested) {
        QMessageBox box(QMessageBox::Question, tr("Columns look swapped"),
            tr("The source term looks like %1 and the translation like %2. Swap the two columns?")
                .arg(DictionaryLanguages::label(profile->langB), DictionaryLanguages::label(profile->langA)),
            QMessageBox::NoButton, this);
        QPushButton* swapBtn = box.addButton(tr("Swap"), QMessageBox::AcceptRole);
        QPushButton* keepBtn = box.addButton(tr("Add anyway"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == swapBtn) {
            std::swap(heard, correct);
            result = check();
        } else if (box.clickedButton() != keepBtn) {
            return;
        } else {
            result.tag.clear();
        }
    }

    if (m_model->addTerm(heard, correct, result.tag)) {
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
    if (!m_termsManager) return;
    if (m_termsManager->importFromFile(m_termsManager->path())) {
        LOG_INFO("Terms reloaded successfully.");
    }
    else {
        emit errorOccurred(tr("术语库加载失败"), tr("未找到词典文件"));
    }
}

void TermWidget::handleCheckConflicts() {
    if (!m_termsManager) return;
    QStringList conflicts = m_termsManager->checkConflicts();
    if (conflicts.isEmpty()) {
        emit errorOccurred(tr("术语冲突检查"), tr("未发现冲突"));
    }
    else {
        emit errorOccurred(tr("检测到术语冲突"), conflicts.join("\n"));
    }
}

void TermWidget::handleImport() {
    if (!m_termsManager) return;
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
    if (!m_termsManager) return;
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
    m_addBtn->setText(tr("+ Add"));
    m_moreBtn->setToolTip(tr("More"));
    m_recognitionTab->setText(tr("Recognition"));
    m_translationTab->setText(tr("Translation"));
    m_newBtn->setText(tr("+ New Dictionary"));

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
    updateBarTexts();
}
