import QtQuick 2.15

Item {
    id: root
    property real level: 0.0   
    property var levels: []          
    property int barCount: 16
    property real barSpacing: 3   

    readonly property real radius: Math.min(width, height) / 2

    Row {
        anchors.centerIn: parent
        spacing: root.barSpacing  

        Repeater {
            model: root.barCount
            delegate: Rectangle {
                id: bar
                readonly property real centerX: (index * (width + root.barSpacing)) + (width / 2)
                readonly property real relativeX: centerX - (root.width / 2)

                readonly property real circleFactor: {
                    var x = Math.abs(relativeX);
                    if (x >= root.radius) return 0;
                    return Math.sqrt(Math.pow(root.radius, 2) - Math.pow(x, 2)) / root.radius;
                }

                readonly property real energy: (index < root.levels.length) ? root.levels[index] : 0.0

                width: Math.max(1.5, (root.width - (root.barCount - 1) * root.barSpacing) / root.barCount)
                height: Math.max(2, (energy * root.height * 1.1) * circleFactor)

                radius: width / 2 + 5
                anchors.verticalCenter: parent.verticalCenter

                Behavior on height {
                    NumberAnimation { duration: 70; easing.type: Easing.OutQuad }
                }

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#1A73E8" }
                    GradientStop { position: 0.7; color: "#00CFEE" }
                    GradientStop { position: 1.0; color: "#00F5D4" }
                }
            }
        }
    }
}