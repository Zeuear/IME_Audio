#include "TermWidget.h"
#include "ui/controls/BatchImportDialog.h"
#include "ui/controls/DictionaryProfileDialog.h"
#include "ui/delegates/DictionaryProfileDelegate.h"
#include "ui/delegates/TermCardDelegate.h"
#include "ui/models/DictionaryProfileModel.h"
#include "ui/models/TermListModel.h"
#include "services/dictionary/DictionaryLanguages.h"
#include "services/dictionary/EntryCheck.h"
#include "services/dictionary/LanguageScript.h"
#include <QApplication>
#include <QButtonGroup>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include "utils/Logger.h"

TermWidget::TermWidget(QWidget *parent) : QWidget(parent) {
    setAcceptDrops(true);
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
    // 「批量导入」平时藏着，鼠标移到「添加」上才滑出来，不占平时的位置
    m_batchBtn = new QPushButton(normalBar);
    m_batchBtn->setCursor(Qt::PointingHandCursor);
    m_batchBtn->setFocusPolicy(Qt::NoFocus);
    m_batchBtn->setMaximumWidth(0);
    m_batchBtn->hide();
    m_addGroup = new QWidget(normalBar);
    auto* groupLayout = new QHBoxLayout(m_addGroup);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->setSpacing(6);
    groupLayout->addWidget(m_batchBtn);
    groupLayout->addWidget(m_addBtn);
    m_addGroup->installEventFilter(this);
    m_batchAnim = new QPropertyAnimation(m_batchBtn, "maximumWidth", this);
    m_batchAnim->setDuration(160);
    m_batchAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_batchAnim, &QPropertyAnimation::finished, this, [this]() {
        if (m_batchBtn->maximumWidth() == 0) m_batchBtn->hide();
    });
    // 稍等一下再收回，鼠标从「添加」移到刚滑出的按钮时不会闪
    m_batchCollapseTimer = new QTimer(this);
    m_batchCollapseTimer->setSingleShot(true);
    m_batchCollapseTimer->setInterval(350);
    connect(m_batchCollapseTimer, &QTimer::timeout, this, [this]() { setBatchExpanded(false); });
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
    normalLayout->addWidget(m_addGroup);
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
    connect(m_batchBtn, &QPushButton::clicked, this, &TermWidget::handleBatchImport);
    // 输入框默认会把拖进来的文件当文本插进去（路径），换成词典导入
    for (QWidget* w : { static_cast<QWidget*>(m_searchEdit), static_cast<QWidget*>(m_heardEdit),
                        static_cast<QWidget*>(m_correctEdit), m_list->viewport() }) {
        w->installEventFilter(this);
    }
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

void TermWidget::setBatchExpanded(bool expanded) {
    m_batchCollapseTimer->stop();
    m_batchAnim->stop();
    if (expanded) m_batchBtn->show();
    m_batchAnim->setStartValue(m_batchBtn->maximumWidth());
    m_batchAnim->setEndValue(expanded ? m_batchBtn->sizeHint().width() : 0);
    m_batchAnim->start();
}

void TermWidget::handleBatchImport() {
    setBatchExpanded(false);
    const DictionaryProfile* profile = m_profiles ? m_profiles->find(m_currentId) : nullptr;
    if (!profile) return;
    BatchImportDialog dialog(profile->kind == DictionaryKind::Translation, profile->langA, profile->langB, this);
    if (dialog.exec() != QDialog::Accepted) return;
    importEntries(*profile, dialog.entries(), dialog.swapColumns());
}

void TermWidget::importEntries(const DictionaryProfile& profile, const QList<GlossaryImport::Entry>& entries, bool swap) {
    TermsLibraryManager* library = m_profiles->library(profile.id);
    if (!library) return;
    const GlossaryImport::Summary summary = GlossaryImport::mergeInto(
        *library, entries, profile.kind == DictionaryKind::Translation, profile.langA, profile.langB, swap);
    LOG_INFO(QStringLiteral("Dictionary batch import: added=%1 duplicates=%2 invalid=%3 mismatched=%4")
                 .arg(summary.added).arg(summary.duplicates).arg(summary.invalid).arg(summary.mismatched));
    notifyImportResult(summary);
    if (summary.added > 0 && profile.id == m_shownId) m_list->scrollToBottom();
}

void TermWidget::notifyImportResult(const GlossaryImport::Summary& summary) {
    QStringList parts;
    parts << tr("新增 %1 条").arg(summary.added);
    if (summary.duplicates > 0) parts << tr("重复跳过 %1 条").arg(summary.duplicates);
    if (summary.invalid > 0) parts << tr("缺少必填栏跳过 %1 条").arg(summary.invalid);
    if (summary.mismatched > 0) parts << tr("%1 条的文字与词典语言不符（已导入，未标记语言）").arg(summary.mismatched);
    emit errorOccurred(summary.added > 0 ? tr("批量导入完成") : tr("没有新增词条"), parts.join(tr("，")));
}

// 只认词典相关的文本文件；先看后缀，不读内容
QString TermWidget::importableFile(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) return {};
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) continue;
        const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (suffix == QLatin1String("json") || suffix == QLatin1String("tsv")
            || suffix == QLatin1String("csv") || suffix == QLatin1String("txt")) {
            return url.toLocalFile();
        }
    }
    return {};
}

void TermWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_profiles && !importableFile(event->mimeData()).isEmpty()) event->acceptProposedAction();
    else QWidget::dragEnterEvent(event);
}

void TermWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (m_profiles && !importableFile(event->mimeData()).isEmpty()) event->acceptProposedAction();
    else QWidget::dragMoveEvent(event);
}

void TermWidget::dropEvent(QDropEvent* event) {
    const QString path = importableFile(event->mimeData());
    if (path.isEmpty() || !m_profiles) {
        QWidget::dropEvent(event);
        return;
    }
    event->acceptProposedAction();
    // 等拖放结束再弹窗，别在拖放事件里开模态对话框
    QTimer::singleShot(0, this, [this, path]() { importDroppedFile(path); });
}

bool TermWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_addGroup) {
        if (event->type() == QEvent::Enter) setBatchExpanded(true);
        else if (event->type() == QEvent::Leave && m_batchBtn->maximumWidth() > 0) m_batchCollapseTimer->start();
        return QWidget::eventFilter(watched, event);
    }
    const QEvent::Type type = event->type();
    if (type == QEvent::DragEnter || type == QEvent::DragMove || type == QEvent::Drop) {
        auto* drag = static_cast<QDropEvent*>(event);
        if (m_profiles && !importableFile(drag->mimeData()).isEmpty()) {
            if (type == QEvent::Drop) dropEvent(drag);
            else drag->acceptProposedAction();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TermWidget::importDroppedFile(const QString& path) {
    QString error;
    const GlossaryImport::Parsed parsed = GlossaryImport::parseFile(path, true, &error);
    if (!error.isEmpty()) {
        emit errorOccurred(tr("导入失败"), tr("无法读取文件，请检查路径与权限"));
        return;
    }
    if (parsed.format == QLatin1String("aurapro-glossary")) {
        importGlossaryPackage(parsed);
        return;
    }

    // 其它格式导入到当前词典；先放进批量导入窗口让用户看一眼识别得对不对
    const DictionaryProfile* profile = m_profiles->find(m_currentId);
    if (!profile) {
        emit errorOccurred(tr("导入失败"), tr("请先选择或新建一个词典，再拖入文件"));
        return;
    }
    BatchImportDialog dialog(profile->kind == DictionaryKind::Translation, profile->langA, profile->langB, this);
    dialog.setText(GlossaryImport::readTextFile(path));
    if (dialog.exec() != QDialog::Accepted) return;
    importEntries(*profile, dialog.entries(), dialog.swapColumns());
}

// aurapro-glossary 自带名称和语言对：同名同语言的翻译词典直接合并，否则新建一本
void TermWidget::importGlossaryPackage(const GlossaryImport::Parsed& parsed) {
    const QString a = DictionaryLanguages::codeFromName(parsed.sourceLang);
    const QString b = DictionaryLanguages::codeFromName(parsed.glossaryLang);
    if (a.isEmpty() || b.isEmpty() || a == b) {
        emit errorOccurred(tr("导入失败"),
            tr("无法识别词典语言「%1 → %2」。请先新建对应语言的翻译词典，再用「批量导入」导入。")
                .arg(parsed.sourceLang, parsed.glossaryLang));
        return;
    }
    if (parsed.entries.isEmpty()) {
        emit errorOccurred(tr("导入失败"), tr("文件里没有可导入的词条"));
        return;
    }

    const QString name = parsed.name.isEmpty() ? tr("Imported dictionary") : parsed.name;
    QString id;
    for (const DictionaryProfile& p : m_profiles->profiles(DictionaryKind::Translation)) {
        const QString pa = LanguageScript::family(p.langA);
        const QString pb = LanguageScript::family(p.langB);
        if (p.name == name && ((pa == a && pb == b) || (pa == b && pb == a))) {
            id = p.id;
            break;
        }
    }

    const QString question = id.isEmpty()
        ? tr("Import \"%1\" (%2 entries, %3 ↔ %4) as a new translation dictionary?")
              .arg(name).arg(parsed.entries.size()).arg(DictionaryLanguages::label(a), DictionaryLanguages::label(b))
        : tr("A dictionary named \"%1\" already exists. Merge %2 entries into it? Existing terms are kept.")
              .arg(name).arg(parsed.entries.size());
    if (QMessageBox::question(this, tr("Import Dictionary"), question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    if (id.isEmpty()) id = m_profiles->addProfile(DictionaryKind::Translation, name, a, b);
    m_lastId[int(DictionaryKind::Translation)] = id;
    selectKind(DictionaryKind::Translation);

    const DictionaryProfile* profile = m_profiles->find(id);
    if (!profile) return;
    const bool swap = GlossaryImport::looksSwapped(parsed.entries, profile->langA, profile->langB);
    importEntries(*profile, parsed.entries, swap);
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
    m_batchBtn->setText(tr("Batch Import"));
    m_moreBtn->setToolTip(tr("More"));
    m_recognitionTab->setText(tr("Recognition"));
    m_translationTab->setText(tr("Translation"));
    m_newBtn->setText(tr("+ New Dictionary"));

    QMenu* menu = m_moreBtn->menu();
    menu->clear();
    menu->addAction(tr("Reload Terms"), this, &TermWidget::handleReload);
    menu->addAction(tr("Check Conflicts"), this, &TermWidget::handleCheckConflicts);
    menu->addSeparator();
    menu->addAction(tr("Batch Import..."), this, &TermWidget::handleBatchImport);
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
