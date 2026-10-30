#include "SnippetSearchPopup.h"

#include <QCursor>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QScreen>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <algorithm>

#include "services/SnippetPaster.h"
#include "snippets/SnippetStore.h"

namespace {

constexpr int kPreviewImageW = 230;
constexpr int kPreviewImageH = 150;
// 还完焦点后等目标窗口真正拿到前台，再发 Ctrl+V
constexpr int kPasteDelayMs = 150;
// 失焦后稍等再关，避免点击列表时的瞬时失焦把窗口关掉
constexpr int kBlurHideDelayMs = 150;

} // namespace

SnippetSearchPopup::SnippetSearchPopup(SnippetStore* store, SnippetPaster* paster, QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
      m_store(store), m_paster(paster) {
    setupUi();
    retranslateUi();

    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [this]() {
        if (isVisible() && !isActiveWindow()) hidePopup();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { refreshList(); });
    connect(m_listView, &QListView::clicked, this, &SnippetSearchPopup::triggerPaste);
    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) { updatePreview(current); });

    m_searchEdit->installEventFilter(this);
    m_listView->installEventFilter(this);
}

void SnippetSearchPopup::setupUi() {
    resize(760, 480);
    setAutoFillBackground(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 8);
    mainLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    mainLayout->addWidget(m_searchEdit);

    auto* midLayout = new QHBoxLayout();
    midLayout->setSpacing(10);

    m_model = new QStandardItemModel(this);
    m_listView = new QListView(this);
    m_listView->setModel(m_model);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    midLayout->addWidget(m_listView, 3);

    auto* previewLayout = new QVBoxLayout();
    previewLayout->setSpacing(6);
    m_previewBadge = new QLabel(this);
    m_previewPath = new QLabel(this);
    m_previewPath->setWordWrap(true);
    m_previewTitle = new QLabel(this);
    m_previewTitle->setWordWrap(true);
    QFont bold = m_previewTitle->font();
    bold.setBold(true);
    m_previewTitle->setFont(bold);
    m_previewBadge->setFont(bold);
    m_previewImage = new QLabel(this);
    m_previewImage->setFixedSize(kPreviewImageW, kPreviewImageH);
    m_previewImage->setAlignment(Qt::AlignCenter);
    m_previewImage->setFrameShape(QFrame::StyledPanel);
    m_previewText = new QLabel(this);
    m_previewText->setWordWrap(true);
    m_previewText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    previewLayout->addWidget(m_previewBadge);
    previewLayout->addWidget(m_previewPath);
    previewLayout->addWidget(m_previewTitle);
    previewLayout->addWidget(m_previewImage);
    previewLayout->addWidget(m_previewText, 1);
    midLayout->addLayout(previewLayout, 2);
    mainLayout->addLayout(midLayout, 1);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setEnabled(false);   // 用 disabled 配色表示次要信息，三种主题下都可读
    mainLayout->addWidget(m_hintLabel);
}

void SnippetSearchPopup::retranslateUi() {
    m_searchEdit->setPlaceholderText(tr("Search snippets (title / text / keyword / folder)"));
    m_hintLabel->setText(tr("↑↓ Select  |  Enter / Click = Paste  |  Esc = Close"));
}

void SnippetSearchPopup::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow())
        m_hideTimer.start(kBlurHideDelayMs);
}

void SnippetSearchPopup::showPopup() {
    // 本窗口已在前台时（用户在软件内按热键）不能把自己当成粘贴目标
    const ForegroundWindow::Handle foreground = ForegroundWindow::current();
    if (foreground && !isVisible()) m_target = foreground;

    // 出现在鼠标附近，限制在当前屏幕内
    const QPoint cursor = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursor);
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();
    move(qBound(area.left(), cursor.x(), area.right() - width()),
         qBound(area.top(), cursor.y(), area.bottom() - height()));

    m_hideTimer.stop();
    m_searchEdit->clear();
    refreshList();

    show();
    raise();
    activateWindow();
    ForegroundWindow::bringToFront(winId());
    m_searchEdit->setFocus();
}

void SnippetSearchPopup::hidePopup() {
    m_hideTimer.stop();
    hide();
}

void SnippetSearchPopup::refreshList() {
    m_model->clear();

    auto hits = m_store->search(m_searchEdit->text(), true);
    if (m_searchEdit->text().trimmed().isEmpty()) {
        // 没有关键词时常用的排前面
        std::stable_sort(hits.begin(), hits.end(), [](const SnippetStore::SearchHit& a, const SnippetStore::SearchHit& b) {
            return a.item->useCount > b.item->useCount;
        });
    }

    for (const auto& hit : hits) {
        const QString title = QString::fromStdString(hit.item->title);
        const QString keyword = QString::fromStdString(hit.item->keyword);
        auto* row = new QStandardItem(keyword.isEmpty() ? title : QStringLiteral("%1   [%2]").arg(title, keyword));
        row->setData(QString::fromStdString(hit.item->id), IdRole);
        row->setData(hit.trail, TrailRole);
        row->setToolTip(hit.trail);
        row->setEditable(false);
        m_model->appendRow(row);
    }

    if (m_model->rowCount() == 0) {
        auto* empty = new QStandardItem(tr("No matching snippets"));
        empty->setEditable(false);
        empty->setSelectable(false);
        m_model->appendRow(empty);
    }

    const QModelIndex first = m_model->index(0, 0);
    m_listView->setCurrentIndex(first);
    updatePreview(first);
}

void SnippetSearchPopup::updatePreview(const QModelIndex& index) {
    m_previewImage->clear();
    m_previewText->clear();
    m_previewPath->clear();
    m_previewTitle->clear();

    const QString id = index.isValid() ? index.data(IdRole).toString() : QString();
    SnippetItem* item = id.isEmpty() ? nullptr : m_store->findItemById(id);
    if (!item) {
        m_previewBadge->setText(tr("Empty"));
        return;
    }

    const QString text = QString::fromStdString(item->text);
    const QString imagePath = m_store->resolveImagePath(QString::fromStdString(item->imagePath));
    const bool remote = imagePath.startsWith("http://") || imagePath.startsWith("https://");
    const bool hasImage = !imagePath.isEmpty() && (remote || QFileInfo::exists(imagePath));
    const bool hasText = !text.trimmed().isEmpty();

    if (hasImage && hasText) m_previewBadge->setText(tr("Text + Image"));
    else if (hasImage) m_previewBadge->setText(tr("Image"));
    else m_previewBadge->setText(tr("Text"));

    m_previewPath->setText(index.data(TrailRole).toString());
    m_previewTitle->setText(QString::fromStdString(item->title));

    if (hasImage) {
        if (remote) {
            m_previewImage->setText(tr("Remote image, downloaded when pasted"));
        } else {
            const QPixmap pixmap(imagePath);
            if (pixmap.isNull()) m_previewImage->setText(tr("Failed to load image"));
            else m_previewImage->setPixmap(pixmap.scaled(kPreviewImageW, kPreviewImageH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    m_previewText->setText(text.left(380));
}

void SnippetSearchPopup::moveSelection(int delta) {
    const int rows = m_model->rowCount();
    if (rows == 0) return;
    const int current = m_listView->currentIndex().isValid() ? m_listView->currentIndex().row() : 0;
    const QModelIndex next = m_model->index((current + delta + rows) % rows, 0);
    m_listView->setCurrentIndex(next);
    m_listView->scrollTo(next);
}

void SnippetSearchPopup::triggerPaste(const QModelIndex& index) {
    if (!index.isValid()) return;
    const QString id = index.data(IdRole).toString();
    if (id.isEmpty()) return;

    hidePopup();
    ForegroundWindow::restore(m_target);
    QTimer::singleShot(kPasteDelayMs, m_paster, [paster = m_paster, id]() { paster->paste(id); });
}

bool SnippetSearchPopup::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == m_searchEdit || watched == m_listView) && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Down:
            moveSelection(1);
            return true;
        case Qt::Key_Up:
            moveSelection(-1);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            triggerPaste(m_listView->currentIndex());
            return true;
        case Qt::Key_Escape:
            hidePopup();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}
