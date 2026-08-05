import QtQuick 2.15

Item {
    id: root
    property real level: 0.0
    property var levels: []
    property int barCount: 16
    property real barSpacing: 1
    property real margin: 12  

    // 柱状条实际可用区域
    readonly property real contentWidth: width - margin * 2
    readonly property real contentHeight: height - margin * 2
    readonly property real radius: Math.min(contentWidth, contentHeight) / 2

    Rectangle {
        id: bg
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: width * 0.5
        radius: 30
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#3b82f6" }
            GradientStop { position: 1.0; color: "#8b5cf6" }
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: root.barSpacing

        Repeater {
            model: root.barCount
            delegate: Rectangle {
                id: bar
                readonly property real centerX: (index * (width + root.barSpacing)) + (width / 2)
                readonly property real relativeX: centerX - (root.contentWidth / 2)
                readonly property real circleFactor: {
                    var x = Math.abs(relativeX);
                    if (x >= root.radius) return 0;
                    return Math.sqrt(Math.pow(root.radius, 2) - Math.pow(x, 2)) / root.radius;
                }

                readonly property real energy: (index < root.levels.length) ? root.levels[index] : 0.0

                readonly property real minBarHeight: 5
                readonly property real maxBarHeight: root.radius
                height: minBarHeight + Math.max(0, energy * circleFactor) * (maxBarHeight - minBarHeight)
                width: Math.max(2, (root.contentWidth - (root.barCount - 1) * root.barSpacing) / root.barCount)

                radius: width / 2 + 5
                anchors.verticalCenter: parent.verticalCenter

                color: Qt.rgba(0.85, 0.85, 0.85, 1.0)
                border.color: Qt.rgba(0.7, 0.7, 0.7, 1.0)
                border.width: 1

                Behavior on height {
                    NumberAnimation { duration: 70; easing.type: Easing.OutQuad }
                }
            }
        }
    }
}