import QtQuick 2.15

Item {
    id: root
    width: 120; height: 120

    LoadingDots {
        id: loadingView
        anchors.fill: parent
        opacity: sphereController.state === 0 ? 1.0 : 0.0
        visible: opacity > 0.01
        Behavior on opacity {
            NumberAnimation { duration: 150; easing.type: Easing.InOutQuad }
        }
    }

    LoadingDots {
        id: transcribingDots
        dotColor: "#FFC107"
        anchors.fill: parent
        opacity: sphereController.state === 3 ? 1.0 : 0.0
        visible: opacity > 0.01
        Behavior on opacity {
            NumberAnimation { duration: 100; easing.type: Easing.InOutQuad }
        }
    }

    readonly property color colorLoadingMid: "#8cc7fa"
    readonly property color colorLoadingDeep: "#1a6bd5"
    readonly property color colorProcessingMid: "#FF8C00"
    readonly property color colorProcessingDeep: "#FF4500"
    readonly property color colorRecordingMid: "#8cc7fa"
    readonly property color colorRecordingDeep: "#1a6bd5"

    FluidSphere {
        id: fluidView
        anchors.fill: parent
        level: sphereController.state === 2 ? 0.0 : sphereController.level
        levels: sphereController.spectrumLevels
        opacity: (sphereController.state === 1 || sphereController.state === 2) ? 1.0 : 0.0
        enableColorAnimation: sphereController.state !== 1 ? false : true
        visible: opacity > 0.01
        Behavior on opacity {
            NumberAnimation { duration: 450; easing.type: Easing.InOutQuad }
        }
    }

    StatusIcon {
        id: statusIcon
        anchors.centerIn: parent
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            sphereController.sphereClicked()
            if (sphereController.state === 1) {
                statusIcon.showTemporaryIcon(2)
            } else if (sphereController.state === 2) {
                statusIcon.showTemporaryIcon(1)
            }
        }
    }
}
