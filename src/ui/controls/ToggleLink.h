#pragma once

class QAbstractButton;
class QCheckBox;

// @brief 把横条开关按钮和设置页已有复选框连成一个状态：点按钮即操作复选框，复选框变化按钮跟着变，不拥有任何一方。
namespace ToggleLink {

void connect(QAbstractButton* button, QCheckBox* box);

} // namespace ToggleLink
