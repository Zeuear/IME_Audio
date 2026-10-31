#include "ui/delegates/SnippetTreeDelegate.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTreeView>

#include "snippets/SnippetStore.h"
#include "ui/models/SnippetTreeModel.h"

namespace {

constexpr int kFolderHeight = 40;
constexpr int kItemMinHeight = 48;
constexpr int kPad = 10;           // 行内左右留白
constexpr int kVPad = 8;           // 条目上下留白
constexpr int kGap = 10;
constexpr int kBadge = 20;         // 标题/内容徽章边长
constexpr int kKeywordW = 84;
constexpr int kImageW = 58;
constexpr int kImageH = 36;
constexpr int kChipH = kImageH;    // 快捷码芯片与配图同高，右侧两块才齐整
constexpr int kRadius = 6;
constexpr int kWrapFlags = Qt::TextWordWrap | Qt::TextWrapAnywhere;   // 中文没有空格，也要能断行

QColor mix(const QColor& a, const QColor& b, int percentOfB) {
    return QColor((a.red() * (100 - percentOfB) + b.red() * percentOfB) / 100,
                  (a.green() * (100 - percentOfB) + b.green() * percentOfB) / 100,
                  (a.blue() * (100 - percentOfB) + b.blue() * percentOfB) / 100);
}

QColor withAlpha(QColor c, int alpha) {
    c.setAlpha(alpha);
    return c;
}

void drawBadge(QPainter* p, const QRect& r, const QString& text, const QColor& color) {
    p->setPen(Qt::NoPen);
    p->setBrush(withAlpha(color, 46));
    p->drawRoundedRect(r, 4, 4);
    p->setPen(color);
    QFont f = p->font();
    f.setBold(true);
    p->setFont(f);
    p->drawText(r, Qt::AlignCenter, text);
}

void drawGhostChip(QPainter* p, const QRect& r, const QString& text, const QColor& color) {
    p->setBrush(Qt::NoBrush);
    QPen pen(withAlpha(color, 140), 1, Qt::DashLine);
    p->setPen(pen);
    p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
    p->setPen(withAlpha(color, 190));
    p->drawText(r, Qt::AlignCenter, text);
}

int depthOf(QModelIndex index) {
    int depth = 0;
    while ((index = index.parent()).isValid()) ++depth;
    return depth;
}

} // namespace

// 目录行各块的位置：绘制和点击命中共用，保证“看到的标题区”就是“能触发重命名的区域”
SnippetTreeDelegate::FolderGeom SnippetTreeDelegate::folderGeometry(const QStyleOptionViewItem& option,
                                                                    const QModelIndex& index) const {
    FolderGeom g;
    const int level = index.data(SnippetTreeModel::LevelRole).toInt();
    const QRect row = option.rect.adjusted(0, 1, 0, -1);
    const int cy = row.center().y();
    int x = row.left() + kPad + 16 + 26;   // 箭头 + 文件夹图标

    g.badgeFont = option.font;
    g.badgeFont.setBold(true);
    g.badgeFont.setPointSizeF(qMax(7.0, option.font.pointSizeF() - 1));
    g.badgeText = SnippetTreeModel::levelBadge(level);
    g.badge = QRect(x, cy - 10, QFontMetrics(g.badgeFont).horizontalAdvance(g.badgeText) + 16, 20);
    x = g.badge.right() + 10;

    g.countText = QCoreApplication::translate("SnippetTreeDelegate", "%1 snippets")
                      .arg(index.data(SnippetTreeModel::CountRole).toInt());
    const int countW = QFontMetrics(option.font).horizontalAdvance(g.countText);
    g.count = QRect(row.right() - kPad - countW, row.top(), countW, row.height());
    g.title = QRect(x, row.top(), qMax(20, g.count.left() - kGap - x), row.height());
    return g;
}

SnippetTreeDelegate::SnippetTreeDelegate(SnippetStore* store, QObject* parent)
    : QStyledItemDelegate(parent), m_store(store) {}

// 行内各块的位置。条目行的高度由换行后的标题/内容决定，所以绘制、sizeHint、点击命中共用这一个函数。
SnippetTreeDelegate::ItemLayout SnippetTreeDelegate::layoutFor(const QRect& row, const QFont& font,
                                                               const QModelIndex& index) const {
    QFont bold = font;
    bold.setBold(true);
    const QFontMetrics fm(font);
    const QFontMetrics fmBold(bold);

    const bool multiline = m_store->settings().multilineEnabled;
    const int textLines = multiline ? qMax(1, m_store->settings().maxDisplayLines) : 1;
    const int titleLines = multiline ? 2 : 1;

    ItemLayout l;
    const int left = row.left() + kPad;
    const int right = row.right() - kPad;

    // 右侧固定宽度的两块，剩下的宽度按 38 : 62 分给标题和内容
    l.image = QRect(right - kImageW + 1, 0, kImageW, kImageH);
    l.keyword = QRect(l.image.left() - kGap - kKeywordW, 0, kKeywordW, kChipH);
    const int contentRight = l.keyword.left() - kGap;
    const int contentW = qMax(120, contentRight - left);
    const int titleW = contentW * 38 / 100;

    l.badgeTitle = QRect(left, 0, kBadge, kBadge);
    l.title = QRect(l.badgeTitle.right() + 7, 0, titleW - kBadge - 7, 0);
    l.badgeText = QRect(left + titleW + kGap, 0, kBadge, kBadge);
    l.text = QRect(l.badgeText.right() + 7, 0, qMax(40, contentRight - (l.badgeText.right() + 7)), 0);

    const QString title = index.data(Qt::DisplayRole).toString();
    const QString text = index.data(SnippetTreeModel::TextRole).toString();
    const quintptr key = reinterpret_cast<quintptr>(index.internalPointer());
    const QString fontKey = font.key();
    MeasureCache& mc = m_measureCache[key];
    if (mc.title != title || mc.text != text || mc.fontKey != fontKey || mc.titleW != l.title.width() ||
        mc.textW != l.text.width() || mc.titleLines != titleLines || mc.textLines != textLines) {
        if (m_measureCache.size() > 4000) {   // 节点被删后残留的条目，超量时整体丢弃
            m_measureCache.clear();
            m_measureCache[key] = MeasureCache();
        }
        MeasureCache& e = m_measureCache[key];
        e.title = title;
        e.text = text;
        e.fontKey = fontKey;
        e.titleW = l.title.width();
        e.textW = l.text.width();
        e.titleLines = titleLines;
        e.textLines = textLines;
        e.titleH = qMin(fmBold.boundingRect(QRect(0, 0, l.title.width(), 10000), kWrapFlags, title).height(),
                        fm.lineSpacing() * titleLines);
        e.textH = qMin(fm.boundingRect(QRect(0, 0, l.text.width(), 10000), kWrapFlags, text).height(),
                       fm.lineSpacing() * textLines);
    }
    const MeasureCache& cached = m_measureCache[key];
    const int titleH = cached.titleH;
    const int textH = cached.textH;

    l.height = qMax(kItemMinHeight, qMax(titleH, textH) + 2 * kVPad);

    // 文字块在行内垂直居中；徽章与首行文字等高居中对齐
    const int top = row.top();
    const int blockH = qMax(qMax(titleH, textH), fm.lineSpacing());
    const int contentTop = top + (l.height - blockH) / 2;
    const int badgeTop = contentTop + (fm.lineSpacing() - kBadge) / 2;
    l.badgeTitle.moveTop(badgeTop);
    l.badgeText.moveTop(badgeTop);
    l.title.setTop(contentTop);
    l.title.setHeight(qMax(titleH, fm.lineSpacing()));
    l.text.setTop(contentTop);
    l.text.setHeight(qMax(textH, fm.lineSpacing()));
    l.keyword.moveTop(top + (l.height - kChipH) / 2);
    l.image.moveTop(top + (l.height - kImageH) / 2);
    return l;
}

QSize SnippetTreeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if (index.data(SnippetTreeModel::IsFolderRole).toBool()) return QSize(0, kFolderHeight);

    // 换行宽度 = 视口宽度 - 该层缩进。绘制时 option.rect 的宽度就是这个值
    int width = option.rect.width();
    if (const auto* view = qobject_cast<const QTreeView*>(option.widget)) {
        width = view->viewport()->width() - depthOf(index) * view->indentation();
    }
    if (width <= 0) width = 600;
    return QSize(0, layoutFor(QRect(0, 0, width, 0), option.font, index).height);
}

SnippetTreeDelegate::Zone SnippetTreeDelegate::zoneAt(const QStyleOptionViewItem& option, const QModelIndex& index,
                                                      const QPoint& pos) const {
    if (index.data(SnippetTreeModel::IsFolderRole).toBool()) {
        // 箭头、图标、徽章、话术数都不是重命名区，只有标题文字所在的区域算
        return folderGeometry(option, index).title.contains(pos) ? Zone::Title : Zone::None;
    }
    const ItemLayout l = layoutFor(option.rect, option.font, index);
    if (l.image.adjusted(-4, -4, 4, 4).contains(pos)) return Zone::Image;
    if (l.keyword.adjusted(-4, -4, 4, 4).contains(pos)) return Zone::Keyword;
    if (pos.x() >= l.badgeText.left() - kGap / 2) return Zone::Text;
    return Zone::Title;
}

void SnippetTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(option.font);
    if (index.data(SnippetTreeModel::IsFolderRole).toBool()) paintFolder(painter, option, index);
    else paintItem(painter, option, index);
    painter->restore();
}

void SnippetTreeDelegate::paintFolder(QPainter* painter, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const {
    const int level = index.data(SnippetTreeModel::LevelRole).toInt();
    const QColor color = SnippetTreeModel::levelColor(level);
    const QRect row = option.rect.adjusted(0, 1, 0, -1);

    painter->setPen(Qt::NoPen);
    painter->setBrush(SnippetTreeModel::levelBg(level));
    painter->drawRoundedRect(row, kRadius, kRadius);
    painter->setBrush(color);
    painter->drawRoundedRect(QRect(row.left(), row.top() + 4, 3, row.height() - 8), 1.5, 1.5);
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(withAlpha(option.palette.color(QPalette::Highlight), 60));
        painter->drawRoundedRect(row, kRadius, kRadius);
    }

    int x = row.left() + kPad;
    const int cy = row.center().y();
    const FolderGeom g = folderGeometry(option, index);

    // 展开箭头：根级没有视图自带的分支列，所以自己画
    painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    if (option.state & QStyle::State_Open) {
        painter->drawPolyline(QPolygon({ QPoint(x, cy - 2), QPoint(x + 4, cy + 2), QPoint(x + 8, cy - 2) }));
    } else {
        painter->drawPolyline(QPolygon({ QPoint(x + 2, cy - 4), QPoint(x + 6, cy), QPoint(x + 2, cy + 4) }));
    }
    x += 16;

    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    const QIcon folderIcon = style->standardIcon((option.state & QStyle::State_Open) ? QStyle::SP_DirOpenIcon
                                                                                     : QStyle::SP_DirIcon);
    folderIcon.paint(painter, QRect(x, cy - 9, 18, 18));

    painter->setFont(g.badgeFont);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRoundedRect(g.badge, 4, 4);
    painter->setPen(Qt::white);
    painter->drawText(g.badge, Qt::AlignCenter, g.badgeText);

    // 右侧话术数
    QFont titleFont = option.font;
    titleFont.setBold(true);
    painter->setFont(option.font);
    painter->setPen(mix(option.palette.color(QPalette::Text), option.palette.color(QPalette::Base), 40));
    painter->drawText(g.count, Qt::AlignVCenter | Qt::AlignRight, g.countText);

    painter->setFont(titleFont);
    painter->setPen(color);
    const QRect titleRect = g.title;
    painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                                        titleRect.width()));
}

void SnippetTreeDelegate::paintItem(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
    const QPalette& pal = option.palette;
    const QColor text = pal.color(QPalette::Text);
    const QColor base = pal.color(QPalette::Base);
    const QColor muted = mix(text, base, 40);
    const ItemLayout l = layoutFor(option.rect, option.font, index);

    // 卡片底：比窗口底色略亮/略深，边框更淡；选中用高亮色，悬停略加深
    const QRect card = option.rect.adjusted(0, 2, 0, -2);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    painter->setBrush(mix(base, text, hovered ? 9 : 5));
    painter->setPen(selected ? QPen(pal.color(QPalette::Highlight), 2) : QPen(mix(base, text, 14), 1));
    painter->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(withAlpha(pal.color(QPalette::Highlight), 40));
        painter->drawRoundedRect(card, kRadius, kRadius);
    }

    // 标题
    drawBadge(painter, l.badgeTitle, QCoreApplication::translate("SnippetTreeDelegate", "CN"), SnippetTreeModel::levelColor(1));
    QFont bold = option.font;
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(text);
    painter->save();
    painter->setClipRect(l.title);
    painter->drawText(l.title, kWrapFlags, index.data(Qt::DisplayRole).toString());
    painter->restore();

    // 内容
    painter->setFont(option.font);
    drawBadge(painter, l.badgeText, QCoreApplication::translate("SnippetTreeDelegate", "FL"), SnippetTreeModel::levelColor(2));
    painter->setFont(option.font);
    painter->setPen(text);
    painter->save();
    painter->setClipRect(l.text);
    painter->drawText(l.text, kWrapFlags, index.data(SnippetTreeModel::TextRole).toString());
    painter->restore();

    // 快捷码芯片
    const QString keyword = index.data(SnippetTreeModel::KeywordRole).toString();
    if (keyword.isEmpty()) {
        drawGhostChip(painter, l.keyword, QCoreApplication::translate("SnippetTreeDelegate", "+ Keyword"), muted);
    } else {
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPointSizeF(option.font.pointSizeF());
        mono.setBold(true);
        painter->setFont(mono);
        const QColor accent = pal.color(QPalette::Highlight);
        painter->setPen(Qt::NoPen);
        painter->setBrush(withAlpha(accent, 40));
        painter->drawRoundedRect(l.keyword, 5, 5);
        painter->setPen(mix(text, accent, 55));
        painter->drawText(l.keyword.adjusted(6, 0, -6, 0), Qt::AlignCenter,
                          painter->fontMetrics().elidedText("[" + keyword + "]", Qt::ElideRight, l.keyword.width() - 12));
    }

    // 配图：缩略图，或“+配图”占位
    painter->setFont(option.font);
    const QVariant thumbVar = index.data(Qt::DecorationRole);
    if (thumbVar.canConvert<QPixmap>() && index.data(SnippetTreeModel::HasImageRole).toBool()) {
        const QPixmap thumb = qvariant_cast<QPixmap>(thumbVar);
        if (!thumb.isNull()) {
            const QSizeF logical = QSizeF(thumb.size()) / thumb.devicePixelRatio();
            const QRectF target(l.image.center().x() - logical.width() / 2.0, l.image.center().y() - logical.height() / 2.0,
                                logical.width(), logical.height());
            QPainterPath clip;
            clip.addRoundedRect(target, 4, 4);
            painter->save();
            painter->setClipPath(clip);
            painter->drawPixmap(target.toRect(), thumb);
            painter->restore();
            painter->setPen(QPen(mix(base, text, 25), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(target.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        }
    } else if (index.data(SnippetTreeModel::HasImageRole).toBool()) {
        // 配了图但读不出来（文件丢失，或远程图还没下载过）
        drawGhostChip(painter, l.image, QCoreApplication::translate("SnippetTreeDelegate", "Image missing"), muted);
    } else {
        drawGhostChip(painter, l.image, QCoreApplication::translate("SnippetTreeDelegate", "+ Image"), muted);
    }
}
