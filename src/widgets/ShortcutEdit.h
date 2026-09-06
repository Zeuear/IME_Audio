#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeyEvent>
#include <QGroupBox>
#include "qhotkey.h"
#include "AnimatedCheckbox.h"
#include "ShortcutFormat.h"
#include "../utils/Logger.h"


class StatusIndicator : public QWidget {
    Q_OBJECT
public:
    explicit StatusIndicator(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(16, 16);
    }

    void setListening(bool listening) {
        m_color = listening ? QColor("#22c55e") : QColor("#ef4444");
        m_listening = listening;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        p.drawEllipse(rect().adjusted(1, 1, -1, -1));
    }

private:
    QColor m_color = QColor("#ef4444");
    bool m_listening = false;
};

class SingleCharEdit : public QLineEdit {
    Q_OBJECT
protected:
    void keyPressEvent(QKeyEvent* event) override {
        int key = event->key();

        // 1. 支持 A-Z 字母键
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            setText(QChar(key)); // 自动转为大写字母
            emit editingFinished();
        }
        // 2. 支持 0-9 数字键（包含大键盘和数字小键盘）
        else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            setText(QChar(key));
            emit editingFinished();
        }
        // 3. 支持 F1-F24 功能键
        else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
            // 使用 QKeySequence 自动将 Qt::Key_F1 转换为文本 "F1"
            setText(QKeySequence(key).toString());
            emit editingFinished();
        }
        // 4. 支持空格键（默认热键使用）
        else if (key == Qt::Key_Space) {
            setText("Space");
            emit editingFinished();
        }
        // 5. 支持退格键和删除键清空内容
        else if (key == Qt::Key_Backspace || key == Qt::Key_Delete) {
            clear();
            emit editingFinished();
        }
    }
};


class ShortcutEdit : public QWidget {
    Q_OBJECT
public:
    explicit ShortcutEdit(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("shortcut_edit");
        QVBoxLayout* vlayout = new QVBoxLayout(this);
        vlayout->setSpacing(20);
        vlayout->setContentsMargins(10, 10, 10, 10);

        // 激活状态指示灯（绿=监听中，红=空闲/未激活）
        QHBoxLayout* statusLayout = new QHBoxLayout();
        statusLayout->setSpacing(5);

        QLabel* indicatorIconLbl = new QLabel("🎶", this);
        indicatorIconLbl->setFixedHeight(16);
        indicatorLbl = new QLabel(tr("Record Status: "), this);
        indicatorLbl->setFixedHeight(16);
        indicatorLbl->setObjectName("indicatorLbl");
        m_indicator = new StatusIndicator(this);

        statusLayout->addWidget(indicatorIconLbl);
        statusLayout->addWidget(indicatorLbl);
        statusLayout->addWidget(m_indicator);
        statusLayout->addStretch();
        updateIndicator(false);
        vlayout->addLayout(statusLayout);

        QHBoxLayout *layout = new QHBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(3);

        // 修饰键名称：Qt 在 macOS 上把 Ctrl 映射为 ⌘、Meta 映射为 ⌃，
        // 因此标签必须按平台显示，内部存储格式保持 Qt 语义不变。
#ifdef Q_OS_MACOS
        const QString ctrlName = QStringLiteral("⌘ Cmd");
        const QString shiftName = QStringLiteral("⇧ Shift");
        const QString altName = QStringLiteral("⌥ Option");
        const QString metaName = QStringLiteral("⌃ Control");
#else
        const QString ctrlName = QStringLiteral("Ctrl");
        const QString shiftName = QStringLiteral("Shift");
        const QString altName = QStringLiteral("Alt");
        const QString metaName = QStringLiteral("Win");
#endif

        // Ctrl 组合
        QLabel *lblCtrl = new QLabel(ctrlName, this);
        lblCtrl->setFixedHeight(30);
        chkCtrl = new AnimatedCheckBox(this);
        layout->addWidget(lblCtrl);
        layout->addWidget(chkCtrl);
        layout->addSpacing(25);

        // Shift 组合
        QLabel *lblShift = new QLabel(shiftName, this);
        lblShift->setFixedHeight(30);
        chkShift = new AnimatedCheckBox(this);
        layout->addWidget(lblShift);
        layout->addWidget(chkShift);
        layout->addSpacing(25);

        // Alt 组合
        QLabel *lblAlt = new QLabel(altName, this);
        lblAlt->setFixedHeight(30);
        chkAlt = new AnimatedCheckBox(this);
        layout->addWidget(lblAlt);
        layout->addWidget(chkAlt);
        layout->addSpacing(25);

        // Meta 组合（Windows 上是 Win 键，macOS 上是 ⌃ Control）
        QLabel* lblMeta = new QLabel(metaName, this);
        lblMeta->setFixedHeight(30);
        chkMeta = new AnimatedCheckBox(this);
        layout->addWidget(lblMeta);
        layout->addWidget(chkMeta);
        layout->addSpacing(25);

        // 字母输入框
        QLabel* lblKey = new QLabel("快捷键 ( A-Z/0-9/F1-F24/Space )", this);
        lblKey->setFixedHeight(30);
        txtKey = new SingleCharEdit();
        txtKey->setFixedWidth(70);
        txtKey->setFixedHeight(30);

        layout->addWidget(lblKey);
        layout->addWidget(txtKey);

        layout->addStretch();
        layout->addSpacing(75);
        vlayout->addLayout(layout);

    }

    ~ShortcutEdit() {
        unregisterHotkey();
    }

    QString getShortCut() const {
        ShortcutParts parts;
        parts.ctrl = chkCtrl->isChecked();
        parts.shift = chkShift->isChecked();
        parts.alt = chkAlt->isChecked();
        parts.meta = chkMeta->isChecked();
        parts.key = normaliseKeyName(txtKey->text().trimmed());
        return formatShortcut(parts);
    }

    void setShortCut(const QString &shortcut) {
        unregisterHotkey();

        const ShortcutParts parts = parseShortcut(shortcut);
        chkCtrl->setChecked(parts.ctrl);
        chkShift->setChecked(parts.shift);
        chkAlt->setChecked(parts.alt);
        chkMeta->setChecked(parts.meta);
        txtKey->setText(parts.key);

        // 用规范化后的字符串注册，历史配置里的 "Win" 别名才能被 QKeySequence 认出
        registerGlobalHotkey(formatShortcut(parts));
    }

    void setListening(bool listening) {
        updateIndicator(listening);
    }


protected:
    void paintEvent(QPaintEvent* event)
    {
        QStyleOption opt;
        opt.initFrom(this);
        QPainter p(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
        QWidget::paintEvent(event);
    }


    void changeEvent(QEvent* event) override {
        if (event->type() == QEvent::LanguageChange) {
            this->retranslateUi();
        }
        QWidget::changeEvent(event);
    }

signals:
    void hotkeyActivated(); 

private slots:
    void updateGlobalHotkeyFromUi() {
        registerGlobalHotkey(getShortCut());
    }

    void updateIndicator(bool listening) {
        m_indicator->setListening(listening);
    }

private:
    void unregisterHotkey() {
        if (globalHotkey) {
            if (globalHotkey->isRegistered()) {
                LOG_DEBUG("Unregister global hotkey");
                globalHotkey->setRegistered(false);
            }
            delete globalHotkey;
            globalHotkey = nullptr;
        }
    }

    void registerGlobalHotkey(const QString &shortcutStr) {
        unregisterHotkey();
        if (shortcutStr.isEmpty()) return;

        globalHotkey = new QHotkey(QKeySequence(shortcutStr), true, this);
        if (globalHotkey->isRegistered()) {
            LOG_DEBUG("Register global hotkey");
            connect(globalHotkey, &QHotkey::activated, this, &ShortcutEdit::hotkeyActivated);
        }
    }

    void retranslateUi()
    {
        indicatorLbl->setText(tr("Record Status: "));
    }

private:
    StatusIndicator* m_indicator = nullptr;
    AnimatedCheckBox* chkCtrl;
    AnimatedCheckBox*chkShift;
    AnimatedCheckBox* chkAlt;
    AnimatedCheckBox* chkMeta;
    QLabel* indicatorLbl;
    QLineEdit *txtKey;
    QHotkey *globalHotkey = nullptr;
};
