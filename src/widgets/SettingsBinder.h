#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>
#include "AppConfig.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QWidget;

// @brief 设置控件与 AppConfig 字段的双向绑定。
class SettingsBinder : public QObject
{
    Q_OBJECT
public:
    enum Effect : unsigned {
        NoEffect    = 0,
        Recorder    = 1 << 0,   // 录音参数
        LiveCaption = 1 << 1,   // 实时字幕
        Hotkey      = 1 << 2,   // 快捷键及触发方式
    };

    enum class Apply {
        Instant,    // 开关、下拉框
        OnCommit,   // 数字、文字
    };

    struct Options {
        Apply apply = Apply::Instant;
        unsigned effects = NoEffect;
        bool lockWhileDictating = false;  
    };

    template <typename T> using Getter = std::function<T(const AppConfig&)>;
    template <typename T> using Setter = std::function<void(AppConfig&, const T&)>;

    explicit SettingsBinder(AppConfig& config, QObject* parent = nullptr);

    void bind(QCheckBox* box, Getter<bool> get, Setter<bool> set, Options opt);
    void bind(QSpinBox* spin, Getter<int> get, Setter<int> set, Options opt);
    
    // 文字两端空白不计
    void bind(QLineEdit* edit, Getter<QString> get, Setter<QString> set, Options opt);
    void bindIndex(QComboBox* combo, Getter<int> get, Setter<int> set, Options opt);
    void bindText(QComboBox* combo, Getter<QString> get, Setter<QString> set, Options opt);
    // 按 itemData 存取：显示文字会随界面语言变，配置里保存的是不随语言变的键
    void bindData(QComboBox* combo, Getter<QString> get, Setter<QString> set, Options opt);

    // 只登记听写锁定，读写由调用方负责
    void addLockedWidget(QWidget* widget);

    // 配置 → 控件；期间不回写配置
    void loadAll();
    void setDictating(bool dictating);

signals:
    void changed(unsigned effects);

private:
    struct Binding {
        QPointer<QWidget> widget;
        std::function<void()> load;
        bool lock = false;
    };
    void add(QWidget* widget, std::function<void()> load, const Options& opt);
    void commit(const Options& opt, const std::function<bool()>& write);

    AppConfig& m_config;
    QList<Binding> m_bindings;
    bool m_loading = false;
};
