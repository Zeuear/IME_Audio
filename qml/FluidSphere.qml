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
    property real internalPulse: Math.pow(smoothedLevel, 1.5) 

    scale: 0.75 + internalPulse * 0.4
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
    property color colorMid: Qt.rgba(0.55, 0.78, 0.98, 1.0)
    property color colorDeep: Qt.rgba(0.10, 0.42, 0.85, 1.0)

    Item {
        anchors.fill: parent
        ShaderEffect {
            id: shaderItem
            width: parent.width * 0.5
            height: parent.height * 0.5
            transform: Scale { xScale: 2.0; yScale: 2.0 }
            smooth: true

            property real uTime: root.time
            property real uLevel: root.smoothedLevel
            property real uPulse: root.internalPulse
            property vector2d uResolution: Qt.vector2d(width, height)
            property vector3d uColorLight: Qt.vector3d(1.0, 1.0, 1.0)   // 高光白
            property vector3d uColorMid:   Qt.vector3d(colorMid.r, colorMid.g, colorMid.b) // 中间浅蓝
            property vector3d uColorDeep:  Qt.vector3d(colorDeep.r, colorDeep.g, colorDeep.b) // 深蓝

            property real uFlowSpeed: 0.4       // 流动速度 0.2 ~ 3.0
            property real uScale: 1.2           // 分块大小 0.5 ~ 3.0
            property real uWarpStrength: 0.6     // 弯曲/扭曲程度 0.3 ~ 3.0
            property real uDetail: 4.0           // 细节精细程度 1.0 ~ 6.0
            property real uVorticity: 0.05        // 涡流强度 0.0 ~ 1.5
            property real uNormalStrength: 0.1  // 法线明暗强度 0.0 ~ 0.6
            property real uCloudContrast: 2.2   // 云团对比度：越大边界越清晰，留白越明显
            property real uCloudCoverage: 0.5  // 云团覆盖率：越小云越多，越大留白越多
            property real uSpeedSoftness: 0.5  
            property vector3d uColorFast: Qt.vector3d(0.0, 0.95, 0.85)  // 高速流动区偏青色
            property real uSpeedColorAmount: 0.5   // 流速对颜色的影响强度 0.0~2.0
            property real uShadingStrength: 0.6


            Behavior on uColorMid {
                enabled: root.enableColorAnimation
                Vector3dAnimation { duration: 800; easing.type: Easing.InOutQuad }
            }
            Behavior on uColorDeep {
                enabled: root.enableColorAnimation
                Vector3dAnimation { duration: 800; easing.type: Easing.InOutQuad }
            }

            fragmentShader: "qrc:/shaders/fluidsphere.frag.qsb"
            vertexShader:   "qrc:/shaders/fluidsphere.vert.qsb"
        }
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