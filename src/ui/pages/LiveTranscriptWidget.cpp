#include "LiveTranscriptWidget.h"
#include "ui/delegates/TranscriptLineDelegate.h"
#include "ui/models/TranscriptLineModel.h"
#include "ui/models/TranscriptStore.h"
#include <QAction>
#include <QCheckBox>
#include "ui/controls/AnimatedCheckbox.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QListView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kSaveDelayMs = 2000;   // 说话时每句都在追加，合并成一次写入
}

LiveTranscriptWidget::LiveTranscriptWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

LiveTranscriptWidget::~LiveTranscriptWidget()
{
    flushSave();
}

void LiveTranscriptWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(10);
    m_toggleBtn = new QPushButton(this);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_injectCheck = new AnimatedCheckBox(this);
    m_injectCheck->setChecked(true);
    m_timestampCheck = new AnimatedCheckBox(this);
    m_copyOriginalBtn = new QPushButton(this);
    m_copyTranslationBtn = new QPushButton(this);
    m_exportBtn = new QPushButton(this);
    m_clearBtn = new QPushButton(this);
    bar->addWidget(m_toggleBtn);
    bar->addSpacing(8);
    m_injectCheck->hide();
    bar->addWidget(m_timestampCheck);
    bar->addStretch();
    bar->addWidget(m_copyOriginalBtn);
    bar->addWidget(m_copyTranslationBtn);
    bar->addWidget(m_exportBtn);
    bar->addWidget(m_clearBtn);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kSaveDelayMs);
    connect(m_saveTimer, &QTimer::timeout, this, &LiveTranscriptWidget::flushSave);

    m_model = new TranscriptLineModel(this);
    m_list = new QListView(this);
    m_list->setObjectName(QStringLiteral("live_transcript_list"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setSelectionRectVisible(true);   // 从空白处拖出框选，和文件管理器一样
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // 行高随宽度换行变化，窗口缩放时要重新排版
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMouseTracking(true);
    m_delegate = new TranscriptLineDelegate(m_list);
    m_list->setItemDelegate(m_delegate);
    m_list->setModel(m_model);
    m_list->viewport()->installEventFilter(this);
    // 右键：默认选中被点的行（已在选区内则保留选区），菜单对选区操作
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (!m_list->indexAt(pos).isValid()) return;
        QMenu menu(this);
        menu.addAction(tr("Copy Original"), this, &LiveTranscriptWidget::copyOriginal);
        QAction* copyTr = menu.addAction(tr("Copy Translation"), this, &LiveTranscriptWidget::copyTranslation);
        copyTr->setEnabled(m_copyTranslationBtn->isEnabled());
        menu.exec(m_list->viewport()->mapToGlobal(pos));
    });

    auto* copyAction = new QAction(m_list);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetShortcut);
    m_list->addAction(copyAction);
    // Ctrl+C 只复制选中行的原文
    connect(copyAction, &QAction::triggered, this, [this]() {
        copySide(false, m_model->linesText(selectedRowList(), withTime()));
    });

    m_placeholder = new QLabel(m_list->viewport());
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    m_placeholder->setForegroundRole(QPalette::PlaceholderText);
    auto* placeholderLayout = new QVBoxLayout(m_list->viewport());
    placeholderLayout->addWidget(m_placeholder);

    layout->addLayout(bar);
    layout->addWidget(m_list, 1);

    connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
        if (m_recording) emit stopRequested();
        else emit startRequested();
    });
    connect(m_injectCheck, &QCheckBox::toggled, this, &LiveTranscriptWidget::injectToggled);
    connect(m_timestampCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_delegate->setShowTime(on);
        m_list->doItemsLayout();
        m_list->viewport()->update();
    });
    connect(m_copyOriginalBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::copyOriginal);
    connect(m_copyTranslationBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::copyTranslation);
    connect(m_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &LiveTranscriptWidget::updateButtons);
    connect(m_exportBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::exportText);
    connect(m_clearBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::clearAll);
    for (auto signal : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved }) {
        connect(m_model, signal, this, &LiveTranscriptWidget::updateButtons);
        connect(m_model, signal, this, &LiveTranscriptWidget::scheduleSave);
    }
    // 手动改了文字：按钮状态（有没有译文）、自动保存、行高都要跟着更新
    connect(m_model, &QAbstractItemModel::dataChanged, this, &LiveTranscriptWidget::updateButtons);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &LiveTranscriptWidget::scheduleSave);
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this]() { m_list->doItemsLayout(); });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { endEdit(false); });
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { endEdit(true); });
    connect(m_model, &QAbstractItemModel::modelReset, this, &LiveTranscriptWidget::updateButtons);
    connect(m_model, &QAbstractItemModel::modelReset, this, &LiveTranscriptWidget::scheduleSave);

    retranslateUi();
    updateButtons();
}

void LiveTranscriptWidget::fitHostScrolling(QScrollArea* host, bool active)
{
    if (!host) return;
    host->setVerticalScrollBarPolicy(active ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    QWidget* contents = host->widget();
    if (!contents) return;
    contents->setMaximumHeight(active ? host->viewport()->height() : QWIDGETSIZE_MAX);
    contents->adjustSize();
}

void LiveTranscriptWidget::appendUtterance(const QString& text, const QString& translation, const QDateTime& time)
{
    // 滚动条已在底部才跟随新内容；用户往上翻看前文时不抢走位置
    QScrollBar* bar = m_list->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_model->add(text, time.time(), translation);
    if (atBottom) m_list->scrollToBottom();
}

void LiveTranscriptWidget::appendPending(quint64 seq, const QString& sourceText)
{
    QScrollBar* bar = m_list->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_model->addPending(seq, sourceText, QTime::currentTime());
    if (atBottom) m_list->scrollToBottom();
}

void LiveTranscriptWidget::completePending(quint64 seq, const QString& sourceText, const QString& finalText)
{
    const QString translation = (!finalText.isEmpty() && finalText != sourceText) ? finalText : QString();
    if (m_model->completePending(seq, sourceText, translation)) return;
    if (!sourceText.isEmpty()) appendUtterance(sourceText, translation);
}

void LiveTranscriptWidget::setRecording(bool recording)
{
    if (m_recording == recording) return;
    m_recording = recording;
    updateButtons();
}

bool LiveTranscriptWidget::injectToForeground() const
{
    return m_injectCheck->isChecked();
}

void LiveTranscriptWidget::setInjectToForeground(bool on)
{
    m_injectCheck->setChecked(on);
}

bool LiveTranscriptWidget::withTime() const
{
    return m_timestampCheck->isChecked();
}

void LiveTranscriptWidget::updateButtons()
{
    m_toggleBtn->setText(m_recording ? tr("Stop") : tr("Start"));
    const bool has = m_model->rowCount() > 0;
    const int selected = selectedRowList().size();
    m_copyOriginalBtn->setEnabled(has);
    m_copyOriginalBtn->setText(selected > 0 ? tr("Copy Original (%1)").arg(selected) : tr("Copy Original"));
    // 没开翻译、还没有译文时没有可复制的
    m_copyTranslationBtn->setEnabled(has && m_model->hasTranslation());
    m_copyTranslationBtn->setText(selected > 0 ? tr("Copy Translation (%1)").arg(selected) : tr("Copy Translation"));
    m_exportBtn->setEnabled(has);
    m_clearBtn->setEnabled(has);
    m_placeholder->setVisible(!has);
}

QList<int> LiveTranscriptWidget::selectedRowList() const
{
    QList<int> rows;
    for (const QModelIndex& idx : m_list->selectionModel()->selectedIndexes()) rows << idx.row();
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
}

void LiveTranscriptWidget::copyOriginal()
{
    const QList<int> rows = selectedRowList();
    const QString text = rows.isEmpty() ? m_model->allText(withTime()) : m_model->linesText(rows, withTime());
    copySide(false, text);
}

void LiveTranscriptWidget::copyTranslation()
{
    const QList<int> rows = selectedRowList();
    const QString text = rows.isEmpty() ? m_model->allTranslationText(withTime())
                                        : m_model->translationLinesText(rows, withTime());
    copySide(true, text);
}

// 所有复制入口共用：写剪贴板并发 copied，由宿主弹信息条，让用户知道复制成功了
void LiveTranscriptWidget::copySide(bool translation, const QString& text)
{
    if (text.isEmpty()) return;
    QGuiApplication::clipboard()->setText(text);
    emit copied(translation ? QStringLiteral("已复制译文") : QStringLiteral("已复制原文"));
}

// 原文、译文分成两个文件导出：选一个文件名，原文写到它，译文写到同名加 .translation 的文件；没有译文就只写一个
void LiveTranscriptWidget::exportText()
{
    const QString original = m_model->allText(withTime());
    if (original.isEmpty()) return;
    const QString suggested = QStringLiteral("transcript_%1.txt")
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Transcript"), suggested,
                                                      tr("Text files (*.txt)"));
    if (path.isEmpty()) return;
    bool ok = TranscriptStore::exportTxt(path, original);
    const QString translation = m_model->allTranslationText(withTime());
    if (ok && !translation.isEmpty()) {
        const QFileInfo info(path);
        const QString translationPath = info.dir().filePath(info.completeBaseName() + QStringLiteral(".translation.")
                                                            + (info.suffix().isEmpty() ? QStringLiteral("txt") : info.suffix()));
        ok = TranscriptStore::exportTxt(translationPath, translation);
    }
    if (!ok) {
        emit errorOccurred(QStringLiteral("导出失败"), QStringLiteral("无法写入所选位置，请换一个文件夹再试"));
    }
}

void LiveTranscriptWidget::setSessionFile(const QString& path)
{
    m_store = std::make_unique<TranscriptStore>(path);

    const QString last = m_store->load();
    if (!last.isEmpty()) {
        m_restoring = true;
        m_model->restore(last);
        m_restoring = false;
        m_list->scrollToBottom();
    }
    // 窗口销毁不一定发生在退出之前，退出时无论如何补写一次
    connect(qApp, &QCoreApplication::aboutToQuit, this, &LiveTranscriptWidget::flushSave, Qt::UniqueConnection);
}

void LiveTranscriptWidget::scheduleSave()
{
    if (!m_store || m_restoring) return;
    m_dirty = true;
    m_saveTimer->start();
}

void LiveTranscriptWidget::flushSave()
{
    if (!m_store || !m_dirty) return;
    m_dirty = false;
    m_saveTimer->stop();
    const QString text = m_model->serialize();
    if (text.isEmpty()) {
        m_store->remove();
        return;
    }
    if (!m_store->save(text)) {
        emit errorOccurred(QStringLiteral("稿子保存失败"),
                           QStringLiteral("无法写入程序数据目录，请检查磁盘空间和目录权限；内容仍在窗口里，可先复制或导出"));
    }
}

void LiveTranscriptWidget::clearAll()
{
    m_model->clear();
    // 清空是用户明确要抹掉稿子：磁盘上的备份也一并删除，不留隐私内容
    if (m_store) m_store->remove();
    m_dirty = false;
    m_saveTimer->stop();
}

void LiveTranscriptWidget::beginEdit(int row, bool translation)
{
    endEdit(true);
    const QModelIndex idx = m_model->index(row);
    if (!idx.isValid()) return;
    m_editRow = row;
    m_editTranslation = translation;
    m_editor = new QPlainTextEdit(m_list->viewport());
    m_editor->setFrameShape(QFrame::StyledPanel);
    m_editor->setPlainText(translation ? m_model->translationAt(row) : m_model->textAt(row));
    // 盖住这一栏；至少两行高，改长句时看得见
    QRect r = m_delegate->sideRect(m_list->visualRect(idx), translation);
    r.setHeight(qMax(r.height(), m_editor->fontMetrics().lineSpacing() * 2 + 16));
    m_editor->setGeometry(r.adjusted(-2, -4, 2, 4));
    m_editor->installEventFilter(this);
    m_editor->show();
    m_editor->setFocus();
    m_editor->selectAll();
}

void LiveTranscriptWidget::endEdit(bool commit)
{
    if (!m_editor) return;
    QPlainTextEdit* editor = m_editor;
    m_editor = nullptr;   // 先清掉，提交引发的失焦不会再进来一次
    const int row = m_editRow;
    const bool translation = m_editTranslation;
    m_editRow = -1;
    const QString text = editor->toPlainText();
    editor->removeEventFilter(this);
    editor->deleteLater();
    if (!commit) return;
    if (translation) m_model->setTranslation(row, text);
    else m_model->setText(row, text);
}

bool LiveTranscriptWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (m_editor && watched == m_editor) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !(ke->modifiers() & Qt::ShiftModifier)) {
                endEdit(true);   // 一句话一行，Enter 直接提交
                return true;
            }
            if (ke->key() == Qt::Key_Escape) {
                endEdit(false);
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            endEdit(true);
        }
        return false;
    }
    if (watched != m_list->viewport()) return QWidget::eventFilter(watched, event);
    const auto type = event->type();
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease
        && type != QEvent::MouseButtonDblClick && type != QEvent::MouseMove)
        return QWidget::eventFilter(watched, event);

    auto* me = static_cast<QMouseEvent*>(event);
    const QPoint pos = me->position().toPoint();
    const QModelIndex idx = m_list->indexAt(pos);
    const auto hit = idx.isValid() ? m_delegate->hitTest(m_list->visualRect(idx), pos)
                                   : TranscriptLineDelegate::Hit::None;
    const bool onText = hit == TranscriptLineDelegate::Hit::Original || hit == TranscriptLineDelegate::Hit::Translation;

    if (type == QEvent::MouseMove) {
        return false;
    }

    // 单击、Ctrl / Shift 点击、点空白、框选都交给 QListView 默认处理，行为和文件管理器一致；
    // 只有双击文字才接管：在这一侧原位编辑
    if (type != QEvent::MouseButtonDblClick || !onText || me->button() != Qt::LeftButton) return false;
    beginEdit(idx.row(), hit == TranscriptLineDelegate::Hit::Translation);
    return true;
}

void LiveTranscriptWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void LiveTranscriptWidget::retranslateUi()
{
    m_injectCheck->setLabelText(tr("Also type into the foreground window"));
    m_injectCheck->setToolTip(tr("Turn off when you are typing yourself, so the transcript only goes here"));
    m_timestampCheck->setLabelText(tr("Timestamps"));
    m_timestampCheck->setToolTip(tr("Show the time of each line and include it when copying all or exporting"));
    m_copyOriginalBtn->setToolTip(tr("Copy the original text of the selected lines, or of all lines when none are selected"));
    m_copyTranslationBtn->setToolTip(tr("Copy the translation of the selected lines, or of all lines when none are selected"));
    m_exportBtn->setText(tr("Export"));
    m_clearBtn->setText(tr("Clear"));
    m_placeholder->setText(tr("Each sentence appears here on its own line as you speak. Click to select lines (Ctrl/Shift+click for several, drag a box on empty space), double-click a text to edit it, right-click to copy."));
    updateButtons();
}
