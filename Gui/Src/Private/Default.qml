import QtQuick
import QtQuick.Controls
import UsdStudio 1.0

// Central 3D viewer of the main window (loaded into the QQuickWidget).
// The Hydra viewport fills the whole area; any Qt Quick UI (buttons, combos,
// panels) is simply declared after it and layered on top - overlay items get
// their own mouse events before the viewport's navigation MouseArea sees them.
Item {
    id: root

    HydraViewportItem {
        id: viewport
        anchors.fill: parent

        // Camera navigation for the whole viewport area.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            property point lastPt
            onPressed: lastPt = Qt.point(mouse.x, mouse.y)
            onPositionChanged: {
                if (!pressed)
                    return
                const dx = mouse.x - lastPt.x
                const dy = mouse.y - lastPt.y
                if (mouse.button === Qt.LeftButton)
                    viewport.orbit(dx, -dy)
                else
                    viewport.pan(dx, dy)
                lastPt = Qt.point(mouse.x, mouse.y)
            }
            onWheel: (wheel) => {
                viewport.dolly(wheel.angleDelta.y > 0 ? 1 / 1.15 : 1.15)
                wheel.accepted = true
            }
        }
    }

    // ---- Overlay UI (examples; add your own items here) ----

    ToolBar {
        id: overlay
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 36
        // Semi-transparent so the 3D image stays visible behind the controls.
        background: Rectangle { color: "#66202024" }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

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

            Button {
                text: qsTr("Frame")
                onClicked: viewport.frameView()
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: viewport.rendererName +
                      (viewport.converged ? qsTr(" — converged") : qsTr(" — rendering…"))
                color: "#c8c8c8"
                font.pixelSize: 11
            }
            Rectangle {
                width: 10; height: 10; radius: 5
                anchors.verticalCenter: parent.verticalCenter
                color: viewport.converged ? "#4caf50" : "#e0a530"
            }
        }
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("LMB orbit   MMB/RMB pan   wheel dolly   F frame")
        color: "#888888"
        font.pixelSize: 10
    }
}
