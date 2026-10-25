#include "SettingsBinder.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

SettingsBinder::SettingsBinder(AppConfig& config, QObject* parent)
    : QObject(parent), m_config(config)
{
}

void SettingsBinder::add(QWidget* widget, std::function<void()> load, const Options& opt)
{
    m_bindings.append({ widget, std::move(load), opt.lockWhileDictating });
}

void SettingsBinder::commit(const Options& opt, const std::function<bool()>& write)
{
    // write 返回值没变时不发通知：OnCommit 控件离开焦点时即使没改也会触发
    if (m_loading || !write()) return;
    emit changed(opt.effects);
}

void SettingsBinder::bind(QCheckBox* box, Getter<bool> get, Setter<bool> set, Options opt)
{
    add(box, [=]() { box->setChecked(get(m_config)); }, opt);
    connect(box, &QCheckBox::toggled, this, [=](bool on) {
        commit(opt, [&]() {
            if (get(m_config) == on) return false;
            set(m_config, on);
            return true;
        });
    });
}

void SettingsBinder::bind(QSpinBox* spin, Getter<int> get, Setter<int> set, Options opt)
{
    if (opt.apply == Apply::OnCommit) spin->setKeyboardTracking(false);
    add(spin, [=]() { spin->setValue(get(m_config)); }, opt);
    connect(spin, &QSpinBox::valueChanged, this, [=](int v) {
        commit(opt, [&]() {
            if (get(m_config) == v) return false;
            set(m_config, v);
            return true;
        });
    });
}

void SettingsBinder::bind(QLineEdit* edit, Getter<QString> get, Setter<QString> set, Options opt)
{
    add(edit, [=]() { edit->setText(get(m_config)); }, opt);
    auto write = [=]() {
        commit(opt, [&]() {
            const QString v = edit->text().trimmed();
            if (get(m_config) == v) return false;
            set(m_config, v);
            return true;
        });
    };
    if (opt.apply == Apply::OnCommit) connect(edit, &QLineEdit::editingFinished, this, write);
    else connect(edit, &QLineEdit::textChanged, this, write);
}

void SettingsBinder::bindIndex(QComboBox* combo, Getter<int> get, Setter<int> set, Options opt)
{
    add(combo, [=]() {
        const int i = get(m_config);
        if (i >= 0 && i < combo->count()) combo->setCurrentIndex(i);
    }, opt);
    connect(combo, &QComboBox::currentIndexChanged, this, [=](int i) {
        if (i < 0) return;   // clear() 时会发 -1
        commit(opt, [&]() {
            if (get(m_config) == i) return false;
            set(m_config, i);
            return true;
        });
    });
}

void SettingsBinder::bindText(QComboBox* combo, Getter<QString> get, Setter<QString> set, Options opt)
{
    add(combo, [=]() {
        const int i = combo->findText(get(m_config));
        if (i >= 0) combo->setCurrentIndex(i);
    }, opt);
    connect(combo, &QComboBox::currentTextChanged, this, [=](const QString& text) {
        if (text.isEmpty()) return;
        commit(opt, [&]() {
            if (get(m_config) == text) return false;
            set(m_config, text);
            return true;
        });
    });
}

void SettingsBinder::addLockedWidget(QWidget* widget)
{
    m_bindings.append({ widget, {}, true });
}

void SettingsBinder::loadAll()
{
    m_loading = true;
    for (const Binding& b : std::as_const(m_bindings)) {
        if (b.widget && b.load) b.load();
    }
    m_loading = false;
}

void SettingsBinder::setDictating(bool dictating)
{
    for (const Binding& b : std::as_const(m_bindings)) {
        if (b.widget && b.lock) b.widget->setEnabled(!dictating);
    }
}
