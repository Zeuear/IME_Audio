import QtQuick 2.15

Item {
    id: root
    width: 120; height: 120

    LoadingDots {
        id: loadingView
        anchors.fill: parent
        opacity: sphereController.state === 0 ? 1.0 : 0.0 // 0 = Loading
        visible: opacity > 0.01

        Behavior on opacity {
            NumberAnimation { duration: 350; easing.type: Easing.InOutQuad }
        }
    }

    // 定义不同状态下的颜色方案
    readonly property color colorLoadingMid: "#8cc7fa" 
    readonly property color colorLoadingDeep: "#1a6bd5"

    readonly property color colorProcessingMid: "#FF8C00"   // 橙色
    readonly property color colorProcessingDeep: "#FF4500"  // 橙红

    readonly property color colorRecordingMid: "#8cc7fa" 
    readonly property color colorRecordingDeep: "#1a6bd5"


    FluidSphere {
        id: fluidView
        anchors.fill: parent

        colorMid: sphereController.state === 3 ? colorProcessingMid : 
                  (sphereController.state === 2 ? colorRecordingMid :
                  (sphereController.state === 1 ? colorRecordingMid : colorLoadingMid))
                  
        colorDeep: sphereController.state === 3 ? colorProcessingDeep : 
                   (sphereController.state === 2 ? colorRecordingDeep :
                   (sphereController.state === 1 ? colorRecordingDeep : colorLoadingDeep))


        level: sphereController.state === 2 ? 0.0 : sphereController.level
        levels: sphereController.spectrumLevels
        opacity: sphereController.state === 0 ? 0.0 : 1.0
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