/****************************************************************************
 *
 * Daeyoung Ground Control - toolbar: GPS state (UI deck slide 8)
 *
 * Satellites | lock | HDOP. Colour follows the worst of the three:
 *   Neon Green : >= 20 satellites, 3D lock or better (RTK counts), HDOP <= 0.9
 *   Yellow     : 15-19 satellites, 2D lock, 0.9 < HDOP < 1.3
 *   Red        : < 15 satellites, no fix / no GPS, HDOP >= 1.3 (or unknown, shown as "-")
 * Click opens QGC's GPS details page.
 *
 ****************************************************************************/

import QtQuick

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
    readonly property var kc:       QGroundControl.corePlugin.kevaColors

    readonly property real satellites:  activeVehicle ? activeVehicle.gps.count.value : NaN
    readonly property int  lock:        activeVehicle ? activeVehicle.gps.lock.value : 0
    // PX4/QGC report a large placeholder HDOP (e.g. 99.99 / 100) without a fix: treat it as unknown
    readonly property real hdop: {
        var h = activeVehicle ? activeVehicle.gps.hdop.value : NaN
        return (isNaN(h) || h >= 50) ? NaN : h
    }

    readonly property var lockNames: [ qsTr("No GPS"), qsTr("No Fix"), qsTr("2D"), qsTr("3D"), qsTr("DGPS"), qsTr("RTK Float"), qsTr("RTK Fixed") ]

    // 0 = good, 1 = caution, 2 = bad
    readonly property int level: {
        var s = (isNaN(satellites) || satellites < 15) ? 2 : (satellites < 20 ? 1 : 0)
        var k = lock >= 3 ? 0 : (lock === 2 ? 1 : 2)
        var d = (isNaN(hdop) || hdop >= 1.3) ? 2 : (hdop > 0.9 ? 1 : 0)
        return Math.max(s, k, d)
    }
    readonly property color stateColor: level === 0 ? kc.neonGreen : (level === 1 ? kc.yellow : kc.red)

    Row {
        id:                     row
        anchors.verticalCenter: parent.verticalCenter
        spacing:                ScreenTools.defaultFontPixelWidth / 2

        QGCColoredImage {
            anchors.verticalCenter: parent.verticalCenter
            width:                  ScreenTools.defaultFontPixelHeight * 1.4
            height:                 width
            sourceSize.height:      height
            fillMode:               Image.PreserveAspectFit
            source:                 "/qmlimages/Gps.svg"
            color:                  control.stateColor
        }

        QGCLabel {
            anchors.verticalCenter: parent.verticalCenter
            color:                  control.stateColor
            font.pointSize:         ScreenTools.largeFontPointSize
            text: {
                var sats = isNaN(control.satellites) || control.satellites < 0 ? "-" : control.satellites.toFixed(0)
                var lockText = control.lock >= 0 && control.lock < control.lockNames.length ? control.lockNames[control.lock] : "-"
                var hdopText = isNaN(control.hdop) ? "-" : control.hdop.toFixed(1)
                return sats + "  " + lockText + "  " + hdopText
            }
        }
    }

    QGCMouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(gpsPage, control)
    }

    Component {
        id: gpsPage
        GPSIndicatorPage { }
    }
}
