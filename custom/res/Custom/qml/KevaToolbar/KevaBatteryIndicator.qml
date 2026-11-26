/****************************************************************************
 *
 * Daeyoung Ground Control - toolbar: battery state (UI deck slide 8)
 *
 * Voltage, current and remaining % for every battery the vehicle reports
 * (one block per battery). Colour by remaining capacity:
 *   31-100 % Neon Green, 16-30 % Yellow, 0-15 % Red, unknown White.
 * Click opens the battery details.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools

Item {
    id:             control
    width:          batteriesRow.width
    anchors.top:    parent.top
    anchors.bottom: parent.bottom

    property bool showIndicator:    true
    property var  activeVehicle:    QGroundControl.multiVehicleManager.activeVehicle
    readonly property var kc:       QGroundControl.corePlugin.kevaColors

    function percentColor(pct) {
        if (isNaN(pct)) {
            return kc.white
        }
        return pct > 30 ? kc.neonGreen : (pct > 15 ? kc.yellow : kc.red)
    }

    function valueText(fact, digits, unit) {
        return (fact && !isNaN(fact.rawValue)) ? fact.rawValue.toFixed(digits) + unit : "-" + unit
    }

    Row {
        id:                     batteriesRow
        anchors.verticalCenter: parent.verticalCenter
        spacing:                ScreenTools.defaultFontPixelWidth * 1.5

        Repeater {
            model: control.activeVehicle ? control.activeVehicle.batteries : []

            Row {
                spacing: ScreenTools.defaultFontPixelWidth / 2

                property var  battery:  object
                property real pct:      battery ? battery.percentRemaining.rawValue : NaN
                property color stateColor: control.percentColor(pct)

                QGCColoredImage {
                    anchors.verticalCenter: parent.verticalCenter
                    width:                  ScreenTools.defaultFontPixelHeight * 1.4
                    height:                 width
                    sourceSize.height:      height
                    fillMode:               Image.PreserveAspectFit
                    source:                 "/qmlimages/Battery.svg"
                    color:                  parent.stateColor
                }

                QGCLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    color:                  parent.stateColor
                    font.pointSize:         ScreenTools.largeFontPointSize
                    text: control.valueText(parent.battery.voltage, 1, "V") + "  " +
                          control.valueText(parent.battery.current, 1, "A") + "  " +
                          (isNaN(parent.pct) ? "-%" : parent.pct.toFixed(0) + "%")
                }
            }
        }
    }

    QGCMouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(batteryPage, control)
    }

    Component {
        id: batteryPage

        ToolIndicatorPage {
            showExpand: false
            contentComponent: Component {
                ColumnLayout {
                    spacing: ScreenTools.defaultFontPixelWidth

                    Repeater {
                        model: control.activeVehicle ? control.activeVehicle.batteries : []

                        SettingsGroupLayout {
                            heading:            qsTr("Battery %1").arg(object.id.rawValue)
                            Layout.fillWidth:   true

                            LabelledLabel { label: qsTr("Voltage");         labelText: control.valueText(object.voltage, 2, " V") }
                            LabelledLabel { label: qsTr("Current");         labelText: control.valueText(object.current, 2, " A") }
                            LabelledLabel { label: qsTr("Remaining");       labelText: control.valueText(object.percentRemaining, 0, " %") }
                            LabelledLabel { label: qsTr("Consumed");        labelText: control.valueText(object.mahConsumed, 0, " mAh") }
                            LabelledLabel { label: qsTr("Time remaining");  labelText: object.timeRemainingStr.value; visible: !isNaN(object.timeRemaining.rawValue) }
                            LabelledLabel { label: qsTr("Temperature");     labelText: control.valueText(object.temperature, 1, " °C"); visible: !isNaN(object.temperature.rawValue) }
                        }
                    }
                }
            }
        }
    }
}
