#pragma once
#include <QDateTime>
#include <QWidget>
#include "ui/controls/AnimatedCheckbox.h"
#include <memory>


class QCheckBox;
class QScrollArea;
class QLabel;
class QPlainTextEdit;
class QListView;
class QPushButton;
class QTimer;
class TranscriptLineDelegate;
class TranscriptLineModel;
class TranscriptStore;

// @brief 实时转录页
class LiveTranscriptWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LiveTranscriptWidget(QWidget* parent = nullptr);
    ~LiveTranscriptWidget() override;

    // 设置窗口把本页借到 QScrollArea 里：active 时把外层高度钉在视口上，
    // 让本页的 QListView 自己滚，工具条始终固定在顶部；离开本页时恢复外层滚动。
    static void fitHostScrolling(QScrollArea* host, bool active);

    // 原文（左栏），译文（右栏）
    void appendUtterance(const QString& text, const QString& translation = {},
                         const QDateTime& time = QDateTime::currentDateTime());
    
    // 要翻译的句子：识别出来先放原文（左栏，右栏先空着），译文好了再补。
    void appendPending(quint64 seq, const QString& sourceText);
    void completePending(quint64 seq, const QString& sourceText, const QString& finalText);
    // 宿主告知录音是否进行中，用来切换"开始/停止"按钮
    void setRecording(bool recording);

    bool injectToForeground() const;
    void setInjectToForeground(bool on);
    // 同时输入到前台
    QCheckBox* injectCheckBox() const { return m_injectCheck; }

    // 指定自动保存文件：立刻恢复上次稿子，内容变化约 2 秒后合并写入，退出前再写一次，清空时删除；不调用则不保存
    void setSessionFile(const QString& path);

signals:
    void startRequested();
    void stopRequested();
    // title/cause 为中文，由宿主统一弹出
    void errorOccurred(const QString& title, const QString& cause);
    void injectToggled(bool on);
    // 复制成功的提示，title 为中文，由宿主用全局的信息条（InforBar）弹出，和其它通知一致
    void copied(const QString& title);

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateButtons();
    // 复制：有选中行只复制选中行，没选中复制全部；原文、译文各一份，互不混在一起
    void copyOriginal();
    void copyTranslation();
    void copySide(bool translation, const QString& text);
    QList<int> selectedRowList() const;
    // 双击某一侧：在原位叠一个编辑框修正文字，Enter / 失去焦点提交，Esc 放弃
    void beginEdit(int row, bool translation);
    void endEdit(bool commit);
    void exportText();
    void scheduleSave();
    void flushSave();
    void clearAll();
    bool withTime() const;

    QPushButton* m_toggleBtn = nullptr;
    AnimatedCheckBox* m_injectCheck = nullptr;
    AnimatedCheckBox* m_timestampCheck = nullptr;
    QPushButton* m_copyOriginalBtn = nullptr;
    QPushButton* m_copyTranslationBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QListView* m_list = nullptr;
    QLabel* m_placeholder = nullptr;
    TranscriptLineModel* m_model = nullptr;
    TranscriptLineDelegate* m_delegate = nullptr;
    std::unique_ptr<TranscriptStore> m_store;
    QTimer* m_saveTimer = nullptr;
    QPlainTextEdit* m_editor = nullptr;
    int m_editRow = -1;
    bool m_editTranslation = false;

    bool m_recording = false;
    bool m_dirty = false;     
    bool m_restoring = false; 
};
