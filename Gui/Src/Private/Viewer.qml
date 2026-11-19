import QtQuick
import QtQuick.Controls
import QtQuick.Window
import UsdStudio 1.0

// Hydra viewport window (loaded by HydraViewerWindow; shown from C++ after
// the QVulkanInstance has been attached).
Window {
    id: win
    width: 1280
    height: 800
    visible: false   // C++ calls show() once the Vulkan instance is set
    title: qsTr("UsdStudio Hydra Viewer")
    color: "#1f1f24"

    HydraViewportItem {
        id: viewport
        anchors.fill: parent

        MouseArea {
            anchors.fill: parent
            focus: true
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            property point lastPt
            onPressed: lastPt = Qt.point(mouse.x, mouse.y)
            onPositionChanged: {
                if (!pressed)
                    return
                const dx = mouse.x - lastPt.x
                const dy = mouse.y - lastPt.y
                if (mouse.button === Qt.LeftButton)
                    viewport.orbit(dx * 0.25, -dy * 0.25)
                else
                    viewport.pan(dx, dy)
                lastPt = Qt.point(mouse.x, mouse.y)
            }
            onWheel: (wheel) => {
                viewport.dolly(wheel.angleDelta.y > 0 ? 1 / 1.15 : 1.15)
                wheel.accepted = true
            }
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_F) {
                    viewport.frameView()
                    event.accepted = true
                }
            }
        }
    }

    // Overlay HUD: renderer selection + convergence indicator.
    Row {
        id: hud
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10

        ComboBox {
            id: rendererBox
            model: viewport.rendererNames
            onActivated: (index) => viewport.setRendererId(viewport.rendererIds[index])
            Component.onCompleted: {
                for (let i = 0; i < viewport.rendererIds.length; ++i)
                    if (viewport.rendererIds[i] === "HdStormRendererPlugin")
                        currentIndex = i
            }
        }

        Rectangle {
            width: 10; height: 10; radius: 5
            anchors.verticalCenter: parent.verticalCenter
            color: viewport.converged ? "#4caf50" : "#e0a530"
        }
    }

    Text {
        anchors.top: hud.bottom
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: viewport.rendererName +
              (viewport.converged ? qsTr(" — converged") : qsTr(" — rendering…")) +
              qsTr("   (LMB orbit, MMB/RMB pan, wheel dolly, F frame)")
        color: "#c8c8c8"
        font.pixelSize: 11
    }
}
