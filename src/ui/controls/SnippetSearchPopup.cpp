#include "SnippetSearchPopup.h"

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMimeData>
#include <QMouseEvent>
#include <QImageReader>
#include <QPixmap>
#include <QScreen>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <algorithm>

#include "services/SnippetPaster.h"
#include "snippets/SnippetStore.h"

namespace {

constexpr int kShadowMargin = 24;   // 阴影的绘制空间，也是 blur 半径的一半
constexpr int kKeywordRole = Qt::UserRole + 7;

// 结果行：左边标题，右边快捷码；选中/悬停用高亮色圆角底。颜色全取 palette
class ResultDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(0, 36); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect row = option.rect.adjusted(2, 2, -2, -2);
        const bool selectable = index.flags() & Qt::ItemIsSelectable;
        const bool selected = selectable && (option.state & QStyle::State_Selected);

        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(option.palette.color(QPalette::Highlight));
            painter->drawRoundedRect(row, 8, 8);
        }
        const QColor text = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        QColor muted = text;
        muted.setAlpha(selected ? 200 : 140);

        QRect inner = row.adjusted(12, 0, -12, 0);
        const QString keyword = index.data(kKeywordRole).toString();
        if (!keyword.isEmpty()) {
            QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            mono.setPointSizeF(option.font.pointSizeF() - 1);
            painter->setFont(mono);
            painter->setPen(muted);
            const int w = painter->fontMetrics().horizontalAdvance(keyword);
            painter->drawText(QRect(inner.right() - w, inner.top(), w, inner.height()),
                              Qt::AlignVCenter | Qt::AlignRight, keyword);
            inner.setRight(inner.right() - w - 12);
        }
        painter->setFont(option.font);
        painter->setPen(selectable ? text : muted);
        painter->drawText(inner, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                                            inner.width()));
        painter->restore();
    }
};

constexpr int kPreviewImageW = 250;
constexpr int kPreviewImageH = 160;
// 列表最多展示条数：QStandardItem 逐条创建，话术上千时每次按键都全量重建会卡；超出的靠继续输入收窄
constexpr int kMaxResults = 200;
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
        if (isVisible() && !isActiveWindow() && !m_dragging) hidePopup();   // 拖动时焦点在系统的拖放循环里
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { refreshList(); });
    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex& index) {
        if (!m_dragged) triggerPaste(index);
    });
    // 鼠标悬停切换选中项，预览随之更新（和方向键共用同一条路径）
    connect(m_listView, &QListView::entered, this, [this](const QModelIndex& index) {
        if (index.flags() & Qt::ItemIsSelectable) m_listView->setCurrentIndex(index);
    });
    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) { updatePreview(current); });

    m_searchEdit->installEventFilter(this);
    m_listView->installEventFilter(this);
    m_listView->viewport()->installEventFilter(this);
}

void SnippetSearchPopup::setupUi() {
    // 外层窗口透明，只用来给卡片的阴影留出绘制空间
    setAttribute(Qt::WA_TranslucentBackground);
    resize(720 + 2 * kShadowMargin, 440 + 2 * kShadowMargin);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);

    // 卡片：颜色全取 palette，light / gray / dark 主题下都可读
    auto* card = new QFrame(this);
    card->setObjectName("snippetCard");
    card->setStyleSheet("#snippetCard { background: palette(window); border: 1px solid palette(midlight);"
                        " border-radius: 12px; }");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(kShadowMargin * 2);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 95));
    card->setGraphicsEffect(shadow);
    outer->addWidget(card);

    auto* mainLayout = new QVBoxLayout(card);
    mainLayout->setContentsMargins(14, 12, 14, 10);
    mainLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(card);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet("QLineEdit { border: none; background: transparent; padding: 6px 4px; }");
    QFont searchFont = m_searchEdit->font();
    searchFont.setPointSizeF(searchFont.pointSizeF() + 2);
    m_searchEdit->setFont(searchFont);
    mainLayout->addWidget(m_searchEdit);

    auto* divider = new QFrame(card);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background: palette(midlight);");
    mainLayout->addWidget(divider);

    auto* midLayout = new QHBoxLayout();
    midLayout->setSpacing(12);

    m_model = new QStandardItemModel(this);
    m_listView = new QListView(card);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(new ResultDelegate(m_listView));
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setStyleSheet("QListView { background: transparent; outline: none; }");
    m_listView->setUniformItemSizes(true);
    // 鼠标悬停即预览（见 entered 连接），所以必须开鼠标跟踪
    m_listView->setMouseTracking(true);
    m_listView->viewport()->setMouseTracking(true);
    midLayout->addWidget(m_listView, 5);

    // 右侧预览：只保留 目录路径 / 标题 / 配图 / 内容
    auto* preview = new QFrame(card);
    preview->setObjectName("snippetPreview");
    preview->setStyleSheet("#snippetPreview { background: palette(base); border-radius: 10px; }");
    auto* previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(14, 12, 14, 12);
    previewLayout->setSpacing(8);

    m_previewPath = new QLabel(preview);
    m_previewPath->setWordWrap(true);
    m_previewPath->setEnabled(false);   // disabled 配色 = 次要信息
    m_previewTitle = new QLabel(preview);
    m_previewTitle->setWordWrap(true);
    QFont bold = m_previewTitle->font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() + 1);
    m_previewTitle->setFont(bold);
    m_previewImage = new QLabel(preview);
    m_previewImage->setFixedSize(kPreviewImageW, kPreviewImageH);
    m_previewImage->setAlignment(Qt::AlignCenter);
    m_previewImage->hide();   // 没有配图时不占位
    m_previewText = new QLabel(preview);
    m_previewText->setWordWrap(true);
    m_previewText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    previewLayout->addWidget(m_previewPath);
    previewLayout->addWidget(m_previewTitle);
    previewLayout->addWidget(m_previewImage, 0, Qt::AlignLeft);
    previewLayout->addWidget(m_previewText, 1);
    midLayout->addWidget(preview, 4);
    mainLayout->addLayout(midLayout, 1);

    m_hintLabel = new QLabel(card);
    m_hintLabel->setEnabled(false);
    mainLayout->addWidget(m_hintLabel);
}

void SnippetSearchPopup::retranslateUi() {
    m_searchEdit->setPlaceholderText(tr("Search snippets (title / text / keyword / folder)"));
    m_hintLabel->setText(tr("↑↓ / Hover to preview  ·  Enter / Click to paste  ·  Drag to another window  ·  Esc to close"));
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

    if (hits.size() > kMaxResults) hits.resize(kMaxResults);

    for (const auto& hit : hits) {
        const QString title = hit.item->title;
        const QString keyword = hit.item->keyword;
        auto* row = new QStandardItem(title);
        row->setData(keyword, kKeywordRole);
        row->setData(hit.item->id, IdRole);
        row->setData(hit.trail, TrailRole);
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
    m_previewImage->hide();
    m_previewText->clear();
    m_previewPath->clear();
    m_previewTitle->clear();

    const QString id = index.isValid() ? index.data(IdRole).toString() : QString();
    SnippetItem* item = id.isEmpty() ? nullptr : m_store->findItemById(id);
    if (!item) return;

    const QString imagePath = m_store->resolveImagePath(item->imagePath);
    const bool remote = imagePath.startsWith("http://") || imagePath.startsWith("https://");
    const bool hasImage = !imagePath.isEmpty() && (remote || QFileInfo::exists(imagePath));

    m_previewPath->setText(index.data(TrailRole).toString());
    m_previewTitle->setText(item->title);

    if (hasImage) {
        m_previewImage->show();
        if (remote) {
            m_previewImage->setText(tr("Remote image, downloaded when pasted"));
        } else {
            // 悬停会频繁切换，解码结果按路径缓存
            auto cached = m_previewCache.constFind(imagePath);
            if (cached == m_previewCache.constEnd()) {
                // 解码时直接缩到预览框大小，不把大图整张载入再缩小
                const qreal dpr = devicePixelRatioF();
                QImageReader reader(imagePath);
                reader.setAutoTransform(true);
                const QSize src = reader.size();
                if (src.isValid()) reader.setScaledSize(src.scaled(QSize(kPreviewImageW, kPreviewImageH) * dpr, Qt::KeepAspectRatio));
                const QImage image = reader.read();
                QPixmap pixmap;
                if (!image.isNull()) {
                    pixmap = QPixmap::fromImage(image);
                    pixmap.setDevicePixelRatio(dpr);
                }
                if (m_previewCache.size() > 40) m_previewCache.clear();
                cached = m_previewCache.insert(imagePath, pixmap);
            }
            if (cached->isNull()) m_previewImage->setText(tr("Failed to load image"));
            else m_previewImage->setPixmap(*cached);
        }
    }
    QString preview = item->text.left(380);
    if (!item->filePath.isEmpty()) {
        const QString attachment = tr("File: %1").arg(QFileInfo(item->filePath).fileName());
        preview = preview.isEmpty() ? attachment : attachment + '\n' + preview;
    }
    m_previewText->setText(preview);
    m_previewText->setToolTip(item->filePath);
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

void SnippetSearchPopup::startDrag(const QModelIndex& index) {
    const QString id = index.data(IdRole).toString();
    if (id.isEmpty()) return;
    QMimeData* mime = m_paster->createDragMime(id);
    if (!mime) return;

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    const QRect rowRect = m_listView->visualRect(index);
    drag->setPixmap(m_listView->viewport()->grab(rowRect));
    drag->setHotSpot(m_dragStartPos - rowRect.topLeft());
    m_dragged = true;

    // 拖动期间不能 hide：源窗口被隐藏后拖动可能当场结束，用户只看到弹窗消失。
    // 弹窗又大又置顶会盖住放置目标，所以只留 1 像素可见（空区域等于清除遮罩，不能用）
    m_dragging = true;
    setMask(QRegion(0, 0, 1, 1));
    const Qt::DropAction result = drag->exec(Qt::CopyAction);
    m_dragging = false;
    clearMask();

    if (result == Qt::IgnoreAction) {
        // 没放到任何地方（Esc 或放在不接受的区域）：弹窗留着，用户可以接着选
        activateWindow();
        m_searchEdit->setFocus();
        return;
    }
    m_store->incrementUseCount(id);
    hidePopup();
}

bool SnippetSearchPopup::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_listView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            m_dragged = false;
            m_dragStartPos = mouse->position().toPoint();
            m_dragIndex = mouse->button() == Qt::LeftButton ? QPersistentModelIndex(m_listView->indexAt(m_dragStartPos))
                                                            : QPersistentModelIndex();
        } else if (event->type() == QEvent::MouseMove && m_dragIndex.isValid()) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if ((mouse->buttons() & Qt::LeftButton)
                && (mouse->position().toPoint() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                const QModelIndex index = m_dragIndex;
                m_dragIndex = QPersistentModelIndex();
                startDrag(index);
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }
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
