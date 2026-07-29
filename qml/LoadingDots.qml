import QtQuick 2.15

Item {
    id: root
    property color dotColor: "#8ec6ff"

    Row {
        anchors.centerIn: parent
        spacing: 10

        Repeater {
            model: 3
            delegate: Rectangle {
                width: 10; height: 10; radius: 5
                color: dotColor
                opacity: 0.3

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    PauseAnimation { duration: index * 160 }
                    NumberAnimation { to: 1.0; duration: 400; easing.type: Easing.OutQuad }
                    NumberAnimation { to: 0.3; duration: 400; easing.type: Easing.InQuad }
                    PauseAnimation { duration: (2 - index) * 160 }
                }
            }
        }
    }
}