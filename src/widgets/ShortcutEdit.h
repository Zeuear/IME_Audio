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
        QHBoxLayout *layout = new QHBoxLayout(this);
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
        // 每个修饰键一组"名称 + 勾选框"，组与组之间留出间隔
        auto addModifier = [&](const QString& name, AnimatedCheckBox*& box) {
            QLabel* label = new QLabel(name, this);
            label->setFixedHeight(30);
            box = new AnimatedCheckBox(this);
            layout->addWidget(label);
            layout->addWidget(box);
            layout->addSpacing(22);
        };
        addModifier(ctrlName, chkCtrl);
        addModifier(shiftName, chkShift);
        addModifier(altName, chkAlt);
        addModifier(metaName, chkMeta);

        // 按键输入框：允许的按键写在提示里，不再占一行标签
        txtKey = new SingleCharEdit();
        txtKey->setFixedSize(70, 30);
        txtKey->setPlaceholderText(tr("Key"));
        txtKey->setToolTip(tr("A-Z / 0-9 / F1-F24 / Space (Backspace to clear)"));
        layout->addWidget(txtKey);
        layout->addStretch();

        // 用户改了组合键就立即重新注册并通知保存；setShortCut 回填时不通知
        for (AnimatedCheckBox* box : { chkCtrl, chkShift, chkAlt, chkMeta }) {
            connect(box, &QCheckBox::toggled, this, &ShortcutEdit::updateGlobalHotkeyFromUi);
        }
        connect(txtKey, &QLineEdit::editingFinished, this, &ShortcutEdit::updateGlobalHotkeyFromUi);
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
        m_filling = true;
        chkCtrl->setChecked(parts.ctrl);
        chkShift->setChecked(parts.shift);
        chkAlt->setChecked(parts.alt);
        chkMeta->setChecked(parts.meta);
        txtKey->setText(parts.key);
        m_filling = false;

        // 用规范化后的字符串注册，历史配置里的 "Win" 别名才能被 QKeySequence 认出
        registerGlobalHotkey(formatShortcut(parts));
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


signals:
    void hotkeyActivated();
    // 按住说话模式用：快捷键松开
    void hotkeyReleased();
    void shortcutChanged(const QString& shortcut);

private slots:
    void updateGlobalHotkeyFromUi() {
        if (m_filling) return;
        registerGlobalHotkey(getShortCut());
        emit shortcutChanged(getShortCut());
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
            connect(globalHotkey, &QHotkey::released, this, &ShortcutEdit::hotkeyReleased);
        }
    }

private:
    AnimatedCheckBox* chkCtrl;
    AnimatedCheckBox*chkShift;
    AnimatedCheckBox* chkAlt;
    AnimatedCheckBox* chkMeta;
    QLineEdit *txtKey;
    QHotkey *globalHotkey = nullptr;
    bool m_filling = false;   // setShortCut 回填控件期间
};
