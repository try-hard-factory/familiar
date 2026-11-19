import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils

Item {
    id: root
    property url modelSource
    property real yaw: 25
    property real pitch: -15
    property real distance: 450
    property bool loaded: loader.status === RuntimeLoader.Success
    property string error: loader.errorString
    property bool showHint: true

    Rectangle { anchors.fill: parent; color: "#28313d" }

    View3D {
        anchors.fill: parent
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#28313d"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }
        PerspectiveCamera {
            id: camera
            z: root.distance
            clipFar: 10000
        }
        DirectionalLight { eulerRotation.x: -35; eulerRotation.y: -30; brightness: 1.4 }
        DirectionalLight { eulerRotation.x: 40; eulerRotation.y: 120; brightness: 0.7 }
        Node {
            eulerRotation: Qt.vector3d(root.pitch, root.yaw, 0)
            RuntimeLoader {
                id: loader
                source: root.modelSource
                position: Qt.vector3d(-(bounds.maximum.x + bounds.minimum.x) * fit / 2,
                                      -(bounds.maximum.y + bounds.minimum.y) * fit / 2,
                                      -(bounds.maximum.z + bounds.minimum.z) * fit / 2)
                scale: Qt.vector3d(fit, fit, fit)
                readonly property real extent: Math.max(bounds.maximum.x - bounds.minimum.x,
                                                        bounds.maximum.y - bounds.minimum.y,
                                                        bounds.maximum.z - bounds.minimum.z)
                readonly property real fit: extent > 0 ? 250 / extent : 1
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.loaded
        acceptedButtons: Qt.LeftButton
        property real lastX
        property real lastY
        onPressed: function(mouse) { lastX = mouse.x; lastY = mouse.y }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            root.yaw += (mouse.x - lastX) * 0.6
            root.pitch = Math.max(-89, Math.min(89, root.pitch + (mouse.y - lastY) * 0.6))
            lastX = mouse.x
            lastY = mouse.y
        }
        onWheel: function(wheel) {
            root.distance = Math.max(100, Math.min(2000,
                                root.distance * (wheel.angleDelta.y > 0 ? 0.9 : 1.1)))
            wheel.accepted = true
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.modelSource.toString().length > 0
                 && !root.loaded && loader.status !== RuntimeLoader.Error
        text: "Loading 3D model…"
        color: "#dce7f3"
        font.pixelSize: 15
    }

    Rectangle {
        visible: root.showHint
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: hint.implicitWidth + 18
        height: hint.implicitHeight + 10
        radius: 5
        color: "#bb101821"
        Text {
            id: hint
            anchors.centerIn: parent
            color: "white"
            text: loader.status === RuntimeLoader.Error ? loader.errorString
                : root.loaded ? "Drag to rotate · Wheel to zoom · Lock or Esc to finish"
                : "Lock or Esc to finish"
            font.pixelSize: 12
        }
    }
}
