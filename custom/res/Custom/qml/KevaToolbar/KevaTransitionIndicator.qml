/****************************************************************************
 *
 * Daeyoung Ground Control - toolbar: VTOL transition state (UI deck slide 8)
 *
 * "MR" in Neon Green (multi-rotor), "FW" in Deep Sky Blue (fixed wing).
 * Click opens the transition command, as in QGC (confirmed by slider).
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools

Item {
    id:             control
    width:          label.width
    anchors.top:    parent.top
    anchors.bottom: parent.bottom

    property var  activeVehicle:    QGroundControl.multiVehicleManager.activeVehicle
    property bool showIndicator:    activeVehicle ? activeVehicle.vtol : false
    property bool inFwdFlight:      activeVehicle ? activeVehicle.vtolInFwdFlight : false
    property bool inAir:            activeVehicle ? (activeVehicle.flying || activeVehicle.landing) : false
    readonly property var kc:       QGroundControl.corePlugin.kevaColors

    QGCLabel {
        id:                     label
        anchors.verticalCenter: parent.verticalCenter
        text:                   control.inFwdFlight ? qsTr("FW") : qsTr("MR")
        color:                  control.inFwdFlight ? control.kc.deepSkyBlue : control.kc.neonGreen
        font.bold:              true
        font.pointSize:         ScreenTools.largeFontPointSize
    }

    QGCMouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(transitionPage, control)
    }

    Component {
        id: transitionPage

        ToolIndicatorPage {
            showExpand: false
            contentComponent: Component {
                ColumnLayout {
                    spacing: ScreenTools.defaultFontPixelWidth / 2

                    QGCLabel {
                        text:       control.inFwdFlight ? qsTr("Fixed wing flight (FW)") : qsTr("Multi-rotor flight (MR)")
                        font.bold:  true
                    }

                    QGCButton {
                        Layout.fillWidth:   true
                        text:               control.inFwdFlight ? qsTr("Transition to Multi-Rotor") : qsTr("Transition to Fixed Wing")
                        enabled:            control.inAir && QGroundControl.corePlugin.kevaCanControlVehicle
                        onClicked: {
                            if (control.inFwdFlight) {
                                mainWindow.vtolTransitionToMRFlightRequest()
                            } else {
                                mainWindow.vtolTransitionToFwdFlightRequest()
                            }
                            mainWindow.closeIndicatorDrawer()
                        }
                    }

                    QGCLabel {
                        visible:        !control.inAir
                        text:           qsTr("Transition is available in flight.")
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }
        }
    }
}
