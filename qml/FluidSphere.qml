import QtQuick 2.15

Item {
    id: root
    property real level: 0.0       
    property var levels: []
    property real smoothedLevel: 0.0
    Behavior on smoothedLevel {
        NumberAnimation { duration: 220; easing.type: Easing.OutQuad }
    }
    onLevelChanged: smoothedLevel = level
    property real internalPulse: Math.pow(smoothedLevel, 1.3) 

    scale: 0.6 + internalPulse * 0.3
    Behavior on scale {
        NumberAnimation { duration: 70; easing.type: Easing.OutQuad }
    }
    property real time: 0.0
    NumberAnimation on time {
        from: 0; to: 1000
        duration: 1000000
        loops: Animation.Infinite
    }
    property bool enableColorAnimation: false
    
    // --- 白毛玻璃圆球 ---
    Rectangle {
        id: glassSphere
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height)
        height: width
        color: Qt.rgba(1.0, 1.0, 1.0, 0.0)
        radius: width / 2
        border.color: Qt.rgba(1.0, 1.0, 1.0, 0.0)
        border.width: 1
        clip: true
    }

    BarSpectrum {
        id: spectrumView
        anchors.fill: parent
        level: root.level
        levels: root.levels	
        scale: 0.8
        anchors.centerIn: parent

        opacity: 0.8 + (root.level * 0.2)
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: sphereController.sphereClicked()
    }
}