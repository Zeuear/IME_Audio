import QtQuick 2.15

Item {
    id: root
    width: 40; height: 40

    // 状态属性：0-Loading, 1-Listening, 2-Paused
    property int status: 0
    property bool visibleIcon: false

    // 图标显示
    Text {
        id: iconText
        anchors.centerIn: parent
        font.pixelSize: 32
        color: "white"
        // 使用 Unicode 符号实现图标
        text: root.status === 1 ? "||" : (root.status === 2 ? ">" : "")
        opacity: root.visibleIcon ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
        }
    }

    // 动画逻辑
    function showTemporaryIcon(s) {
        root.status = s
        root.visibleIcon = true

        // 1.5秒后自动消失
        hideTimer.start()
    }

    Timer {
        id: hideTimer
        interval: 1500
        onTriggered: root.visibleIcon = false
    }
}