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

    scale: 0.7 + internalPulse * 0.3
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
    

    BarSpectrum {
        id: spectrumView
        anchors.fill: parent
        level: root.level
        levels: root.levels	
        scale: 1
        anchors.centerIn: parent

        opacity: 0.8 + (root.level * 0.2)
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: sphereController.sphereClicked()
    }
}