#pragma once

class QAbstractButton;
class QCheckBox;

// @brief 把横条上的开关按钮和设置页里已有的复选框连成一个状态。
//        横条不自己写配置：点按钮就是操作那个复选框，已有的配置绑定、保存、界面联动照常触发；
//        反过来复选框变化（含被代码改、被禁用）时，按钮跟着变。
//        连接随按钮销毁而解除，不拥有任何一方。
namespace ToggleLink {

void connect(QAbstractButton* button, QCheckBox* box);

} // namespace ToggleLink


