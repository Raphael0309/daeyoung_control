/****************************************************************************
 *
 * Daeyoung Ground Control - toolbar: current flight mode (UI deck slide 8)
 *
 * White text. Click opens the list of selectable modes (set in
 * CustomFirmwarePlugin: Position, Stabilized, Altitude, Offboard, Mission,
 * Safe Recovery, Takeoff, Land, Hold). PX4 "Return" is shown as "Safe Recovery".
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools

Item {
    id:             control
    width:          row.width
    anchors.top:    parent.top
    anchors.bottom: parent.bottom

    property bool showIndicator:    true
    property var  activeVehicle:    QGroundControl.multiVehicleManager.activeVehicle

    function displayName(mode) {
        return mode === "Return" ? qsTr("Safe Recovery") : mode
    }

    Row {
        id:                     row
        anchors.verticalCenter: parent.verticalCenter
        spacing:                ScreenTools.defaultFontPixelWidth / 2

        QGCColoredImage {
            anchors.verticalCenter: parent.verticalCenter
            width:                  ScreenTools.defaultFontPixelHeight * 1.2
            height:                 width
            sourceSize.height:      height
            fillMode:               Image.PreserveAspectFit
            mipmap:                 true
            color:                  "white"
            source:                 "/qmlimages/FlightModesComponentIcon.png"
        }

        QGCLabel {
            anchors.verticalCenter: parent.verticalCenter
            text:                   control.activeVehicle ? control.displayName(control.activeVehicle.flightMode) : qsTr("N/A")
            color:                  "white"
            font.bold:              true
            font.pointSize:         ScreenTools.largeFontPointSize
        }
    }

    QGCMouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(drawerComponent, control)
    }

    Component {
        id: drawerComponent

        ToolIndicatorPage {
            showExpand:         false
            contentComponent:   modesComponent
        }
    }

    Component {
        id: modesComponent

        ColumnLayout {
            spacing: ScreenTools.defaultFontPixelWidth / 2

            QGCLabel {
                text:           qsTr("Flight Mode")
                font.bold:      true
            }

            Repeater {
                model: control.activeVehicle ? control.activeVehicle.flightModes : []

                QGCButton {
                    Layout.fillWidth:   true
                    text:               control.displayName(modelData)
                    primary:            control.activeVehicle && control.activeVehicle.flightMode === modelData
                    enabled:            QGroundControl.corePlugin.kevaCanControlVehicle
                    onClicked: {
                        control.activeVehicle.flightMode = modelData
                        mainWindow.closeIndicatorDrawer()
                    }
                }
            }

            QGCLabel {
                visible:                !QGroundControl.corePlugin.kevaCanControlVehicle
                text:                   qsTr("Administrator accounts cannot change the flight mode.")
                color:                  QGroundControl.corePlugin.kevaColors.orange
                font.pointSize:         ScreenTools.smallFontPointSize
            }
        }
    }
}
