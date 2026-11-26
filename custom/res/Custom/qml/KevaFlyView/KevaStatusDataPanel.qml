/****************************************************************************
 *
 * Daeyoung Ground Control - Status Data panel (UI deck slides 11 and 12)
 *
 * Block layout per slide 12 (5 columns, 2 rows):
 *
 *   | Alt (Rel) | Air Spd.  |      | Lat. / Lon. / MGRS             |
 *   |-----------|-----------| Thr% |--------------------------------|
 *   | Comm.     | Gnd Spd.  |      | Wind dir/spd | Dist. home/GCS  |
 *
 * - Before a vehicle is connected every value shows "--".
 * - Comm.: share of good MAVLink packets over the last 5 s (bar):
 *   70-100 % Neon Green, 50-69 % Yellow, 30-49 % Orange, 0-29 % Red.
 * - Thr%: FW (pusher) and M1-M4 outputs: 0-60 % Neon Green, 61-80 % Yellow,
 *   81-100 % Red (outputs from SERVO_OUTPUT_RAW, see CustomPlugin.cc).
 * - Wind speed: 0-7.9 m/s Neon Green, 8-11.9 m/s Yellow, 12 m/s and above Red.
 * - Lat./Lon. with 7 decimals, MGRS with 1 m precision.
 * - Dist. to GCS needs an external GPS on the ground station (NMEA GPS set in
 *   Settings > Comm Links / AutoConnect); without it "--" (the PC's own
 *   network-based location is not accurate enough).
 * - Climb rate, roll, pitch and heading are shown in the HUD only.
 * - Alt. and Air Spd. become target dials in U10.
 *
 ****************************************************************************/

import QtQuick
import QtPositioning

import QGroundControl
import QGroundControl.Controls

KevaPanelFrame {
    id: root

    property var vehicle: QGroundControl.multiVehicleManager.activeVehicle

    readonly property real  _pad:       Math.max(2, Math.round(height * 0.025))
    readonly property real  _titlePx:   Math.max(10, height * 0.062)
    readonly property real  _valuePx:   Math.max(12, height * 0.11)
    readonly property real  _unitPx:    Math.max(10, height * 0.06)
    readonly property real  _rowPx:     Math.max(10, height * 0.066)
    readonly property real  _rowH:      Math.floor((height - _pad * 3) / 2)
    readonly property real  _innerW:    width - _pad * 6
    // Column widths from the slide 12 block diagram (Wind / Dist. split evened out for the distance text)
    readonly property var   _colFrac:   [ 0.205, 0.175, 0.285, 0.16, 0.175 ]

    // Whole pixels so the 1 px block borders stay sharp
    function _colW(i) { return Math.floor(_innerW * _colFrac[i]) }
    function _colX(i) {
        var x = _pad
        for (var k = 0; k < i; k++) {
            x += _colW(k) + _pad
        }
        return x
    }
    function _rowY(r) { return Math.round(_pad + r * (_rowH + _pad)) }

    // ---------------------------------------------------------------- values
    function _num(f) {
        return (vehicle && f && !isNaN(f.rawValue)) ? f.rawValue : NaN
    }
    function _fixed(v, d) {
        return isNaN(v) ? "--" : v.toFixed(d)
    }
    function _distText(m) {
        if (isNaN(m)) {
            return "--"
        }
        if (m < 1000) {
            return m.toFixed(0) + " m"
        }
        return (m < 100000 ? (m / 1000).toFixed(2) : (m / 1000).toFixed(0)) + " km"
    }
    function _thrColor(pct) {
        if (isNaN(pct)) {
            return kevaColors.white
        }
        return pct <= 60 ? kevaColors.neonGreen : (pct <= 80 ? kevaColors.yellow : kevaColors.red)
    }
    function _commColor(pct) {
        if (isNaN(pct)) {
            return kevaColors.white
        }
        if (pct >= 70) return kevaColors.neonGreen
        if (pct >= 50) return kevaColors.yellow
        if (pct >= 30) return kevaColors.orange
        return kevaColors.red
    }
    function _windColor(ms) {
        if (isNaN(ms)) {
            return kevaColors.white
        }
        return ms < 8 ? kevaColors.neonGreen : (ms < 12 ? kevaColors.yellow : kevaColors.red)
    }

    readonly property real  _altRel:    vehicle ? _num(vehicle.altitudeRelative) : NaN
    readonly property real  _airSpd:    vehicle ? _num(vehicle.airSpeed) : NaN
    readonly property real  _gndSpd:    vehicle ? _num(vehicle.groundSpeed) : NaN
    readonly property real  _windDir:   vehicle ? _num(vehicle.wind.direction) : NaN
    readonly property real  _windSpd:   vehicle ? _num(vehicle.wind.speed) : NaN
    readonly property real  _distHome:  vehicle ? _num(vehicle.distanceToHome) : NaN
    readonly property bool  _gcsGps:    QGroundControl.settingsManager.autoConnectSettings.autoConnectNmeaPort.rawValue !== "Disabled"
                                        && QGroundControl.qgcPositionManger.gcsPosition.isValid
    readonly property real  _distGcs:   (vehicle && _gcsGps) ? _num(vehicle.distanceToGCS) : NaN
    readonly property var   _coord:     vehicle ? vehicle.coordinate : QtPositioning.coordinate()
    readonly property bool  _coordOk:   vehicle && _coord.isValid

    // Thr%: [FW, M1..M4]. FW falls back to the VFR_HUD throttle in forward flight
    // when the vehicle does not send its outputs.
    readonly property var   _outputs:   vehicle ? QGroundControl.corePlugin.kevaMotorOutputs : []
    function _output(i) {
        if (_outputs.length > i) {
            return _outputs[i]
        }
        if (i === 0 && vehicle && vehicle.vtolInFwdFlight) {
            return _num(vehicle.throttlePct)
        }
        return NaN
    }

    // Comm.: good packets over the last 5 s, sampled once a second
    property var    _commSamples:   []
    property real   _commPct:       NaN
    onVehicleChanged: {
        _commSamples = []
        _commPct = NaN
    }
    Timer {
        interval:   1000
        running:    true
        repeat:     true
        onTriggered: {
            if (!root.vehicle) {
                root._commPct = NaN
                return
            }
            var s = root._commSamples.slice()
            s.push({ rx: root.vehicle.mavlinkReceivedCount, lost: root.vehicle.mavlinkLossCount })
            while (s.length > 6) {
                s.shift()
            }
            root._commSamples = s
            if (s.length >= 2) {
                var rx   = s[s.length - 1].rx - s[0].rx
                var lost = s[s.length - 1].lost - s[0].lost
                root._commPct = (rx + lost) > 0 ? Math.max(0, Math.min(100, rx * 100 / (rx + lost))) : 0
            }
        }
    }

    // ---------------------------------------------------------------- parts
    component Cell: Rectangle {
        property string title
        default property alias content: contentItem.data

        color:          "transparent"
        border.color:   root.kevaColors.border
        border.width:   1
        radius:         3
        clip:           true

        QGCLabel {
            id:             titleLabel
            x:              root._pad * 2
            y:              root._pad * 1.5
            text:           title
            color:          root.kevaColors.deepSkyBlue
            font.pixelSize: root._titlePx
            font.bold:      true
        }

        Item {
            id:                 contentItem
            anchors.left:       parent.left
            anchors.right:      parent.right
            anchors.top:        titleLabel.bottom
            anchors.bottom:     parent.bottom
            anchors.margins:    root._pad * 2
            anchors.topMargin:  root._pad
        }
    }

    // Big number with a small unit
    component BigValue: Row {
        property string value:  "--"
        property string unit
        property color  valueColor: root.kevaColors.white

        anchors.verticalCenter: parent.verticalCenter
        spacing:                root._pad

        QGCLabel {
            id:             bigValueText
            text:           value
            color:          valueColor
            font.pixelSize: root._valuePx
            font.bold:      true
        }
        QGCLabel {
            anchors.baseline:   bigValueText.baseline
            text:               value === "--" ? "" : unit
            color:              root.kevaColors.white
            font.pixelSize:     root._unitPx
        }
    }

    // "label ........ value" line
    component LabelValue: Item {
        property string label
        property string value:  "--"
        property color  valueColor: root.kevaColors.white
        property real   px:     root._rowPx

        width:  parent ? parent.width : 0
        height: px * 1.35

        QGCLabel {
            anchors.left:           parent.left
            anchors.verticalCenter: parent.verticalCenter
            text:                   label
            color:                  root.kevaColors.deepSkyBlue
            font.pixelSize:         px * 0.85
        }
        QGCLabel {
            anchors.right:          parent.right
            anchors.verticalCenter: parent.verticalCenter
            text:                   value
            color:                  valueColor
            font.pixelSize:         px
            font.bold:              true
        }
    }

    // Horizontal percentage bar
    component PctBar: Rectangle {
        property real   pct:    NaN
        property color  barColor: root.kevaColors.white

        color:          "#2A2A2A"
        radius:         2

        Rectangle {
            anchors.left:   parent.left
            anchors.top:    parent.top
            anchors.bottom: parent.bottom
            width:          isNaN(pct) ? 0 : parent.width * Math.max(0, Math.min(100, pct)) / 100
            radius:         2
            color:          barColor
        }
    }

    // ---------------------------------------------------------------- blocks
    Cell {
        x: _colX(0); y: _rowY(0); width: _colW(0); height: _rowH
        title: qsTr("Alt. (Rel)")
        BigValue { value: _fixed(_altRel, 1); unit: "m" }
    }

    Cell {
        x: _colX(0); y: _rowY(1); width: _colW(0); height: _rowH
        title: qsTr("Comm.")

        Column {
            anchors.left:           parent.left
            anchors.right:          parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing:                root._pad * 2

            PctBar {
                width:      parent.width
                height:     Math.max(6, root._rowH * 0.16)
                pct:        root._commPct
                barColor:   root._commColor(root._commPct)
            }
            QGCLabel {
                text:           isNaN(root._commPct) ? "--" : root._commPct.toFixed(0) + " %"
                color:          root._commColor(root._commPct)
                font.pixelSize: root._valuePx * 0.8
                font.bold:      true
            }
        }
    }

    Cell {
        x: _colX(1); y: _rowY(0); width: _colW(1); height: _rowH
        title: qsTr("Air Spd.")
        BigValue { value: _fixed(_airSpd, 1); unit: "m/s" }
    }

    Cell {
        x: _colX(1); y: _rowY(1); width: _colW(1); height: _rowH
        title: qsTr("Gnd Spd.")
        BigValue { value: _fixed(_gndSpd, 1); unit: "m/s" }
    }

    Cell {
        x: _colX(2); y: _rowY(0); width: _colW(2); height: _rowH * 2 + _pad
        title: qsTr("Thr%")

        Column {
            anchors.fill:   parent
            spacing:        root._pad * 1.5

            Repeater {
                model: [ "FW", "M1", "M2", "M3", "M4" ]

                Item {
                    readonly property real pct: root._output(index)

                    width:  parent.width
                    height: (parent.height - root._pad * 1.5 * 4) / 5

                    QGCLabel {
                        id:                     thrLabel
                        anchors.left:           parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width:                  root._rowPx * 1.9
                        text:                   modelData
                        color:                  root.kevaColors.deepSkyBlue
                        font.pixelSize:         root._rowPx * 0.9
                        font.bold:              true
                    }
                    QGCLabel {
                        id:                     thrValue
                        anchors.right:          parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width:                  root._rowPx * 3.2
                        horizontalAlignment:    Text.AlignRight
                        text:                   isNaN(pct) ? "--" : pct.toFixed(0) + "%"
                        color:                  root._thrColor(pct)
                        font.pixelSize:         root._rowPx
                        font.bold:              true
                    }
                    PctBar {
                        anchors.left:           thrLabel.right
                        anchors.right:          thrValue.left
                        anchors.rightMargin:    root._pad * 2
                        anchors.verticalCenter: parent.verticalCenter
                        height:                 Math.max(5, parent.height * 0.4)
                        pct:                    parent.pct
                        barColor:               root._thrColor(parent.pct)
                    }
                }
            }
        }
    }

    Cell {
        x: _colX(3); y: _rowY(0); width: _colW(3) + _pad + _colW(4); height: _rowH
        title: qsTr("Position")

        Column {
            anchors.left:           parent.left
            anchors.right:          parent.right
            anchors.verticalCenter: parent.verticalCenter

            LabelValue { label: qsTr("Lat.");  value: root._coordOk ? root._coord.latitude.toFixed(7) : "--" }
            LabelValue { label: qsTr("Lon.");  value: root._coordOk ? root._coord.longitude.toFixed(7) : "--" }
            LabelValue {
                label: qsTr("MGRS")
                value: {
                    var m = root._coordOk ? QGroundControl.corePlugin.kevaMgrs(root._coord) : ""
                    return m === "" ? "--" : m
                }
            }
        }
    }

    Cell {
        x: _colX(3); y: _rowY(1); width: _colW(3); height: _rowH
        title: qsTr("Wind")

        Column {
            anchors.left:           parent.left
            anchors.right:          parent.right
            anchors.verticalCenter: parent.verticalCenter

            LabelValue { label: qsTr("Dir."); value: isNaN(root._windDir) ? "--" : root._windDir.toFixed(0) + "°" }
            LabelValue {
                label:      qsTr("Spd.")
                value:      isNaN(root._windSpd) ? "--" : root._windSpd.toFixed(1) + " m/s"
                valueColor: root._windColor(root._windSpd)
            }
        }
    }

    Cell {
        x: _colX(4); y: _rowY(1); width: _colW(4); height: _rowH
        title: qsTr("Dist.")

        Column {
            anchors.left:           parent.left
            anchors.right:          parent.right
            anchors.verticalCenter: parent.verticalCenter

            LabelValue { label: qsTr("Home"); value: root._distText(root._distHome); px: root._rowPx * 0.88 }
            LabelValue { label: qsTr("GCS");  value: root._distText(root._distGcs);  px: root._rowPx * 0.88 }
        }
    }
}
