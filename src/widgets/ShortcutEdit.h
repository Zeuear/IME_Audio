#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeyEvent>
#include "qhotkey.h"
#include "animatedcheckbox.h"
#include "../utils/Logger.h"

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
        // 4. 支持退格键和删除键清空内容
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
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        // Ctrl 组合
        QLabel *lblCtrl = new QLabel("Ctrl", this);
        chkCtrl = new AnimatedCheckBox(this);
        layout->addWidget(lblCtrl);
        layout->addWidget(chkCtrl);
        layout->addSpacing(25);

        // Shift 组合
        QLabel *lblShift = new QLabel("Shift", this);
        chkShift = new AnimatedCheckBox(this);
        layout->addWidget(lblShift);
        layout->addWidget(chkShift);
        layout->addSpacing(25);

        // Alt 组合
        QLabel *lblAlt = new QLabel("Alt", this);
        chkAlt = new AnimatedCheckBox(this);
        layout->addWidget(lblAlt);
        layout->addWidget(chkAlt);
        layout->addSpacing(25);

        // Win 组合
        QLabel* lblWin = new QLabel("Win", this);
        chkWin = new AnimatedCheckBox(this);
        layout->addWidget(lblWin);
        layout->addWidget(chkWin);
        layout->addSpacing(25);

        // 字母输入框
        QLabel* lblKey = new QLabel("快捷键(A-Z/0-9/F1-F24)", this);
        txtKey = new SingleCharEdit();
        txtKey->setMaxLength(1);
        txtKey->setFixedWidth(70);
        layout->addWidget(lblKey);
        layout->addWidget(txtKey);

        layout->addStretch();
    }

    ~ShortcutEdit() {
        unregisterHotkey();
    }

    QString getShortCut() const {
        QStringList list;
        if (chkCtrl->isChecked()) list << "Ctrl";
        if (chkShift->isChecked()) list << "Shift";
        if (chkAlt->isChecked()) list << "Alt";
		if (chkWin->isChecked()) list << "Win";
        
        QString keyText = txtKey->text().trimmed().toUpper();
        if (!keyText.isEmpty()) {
            list << keyText;
        }
        return list.join("+");
    }

    void setShortCut(const QString &shortcut) {
        unregisterHotkey(); 
        chkCtrl->setChecked(false);
        chkShift->setChecked(false);
        chkAlt->setChecked(false);
		chkWin->setChecked(false);
        txtKey->clear();

        QStringList parts = shortcut.split("+");
        for (const QString &part : parts) {
            QString p = part.trimmed();
            if (p.compare("Ctrl", Qt::CaseInsensitive) == 0) chkCtrl->setChecked(true);
            else if (p.compare("Shift", Qt::CaseInsensitive) == 0) chkShift->setChecked(true);
            else if (p.compare("Alt", Qt::CaseInsensitive) == 0) chkAlt->setChecked(true);
			else if (p.compare("Win", Qt::CaseInsensitive) == 0) chkWin->setChecked(true);
            else if (p.length() == 1) txtKey->setText(p.toUpper());
        }
        registerGlobalHotkey(shortcut);
    }

signals:
    void hotkeyActivated(); 

private slots:
    void updateGlobalHotkeyFromUi() {
        registerGlobalHotkey(getShortCut());
    }

private:
    void unregisterHotkey() {
        if (globalHotkey) {
            if (globalHotkey->isRegistered()) {
                LOG_INFO("Unregister global hotkey");
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
            LOG_INFO("Register global hotkey");
            connect(globalHotkey, &QHotkey::activated, this, &ShortcutEdit::hotkeyActivated);
        }
    }

private:
    AnimatedCheckBox* chkCtrl;
    AnimatedCheckBox*chkShift;
    AnimatedCheckBox* chkAlt;
    AnimatedCheckBox*chkWin;
    QLineEdit *txtKey;
    QHotkey *globalHotkey = nullptr;
};
