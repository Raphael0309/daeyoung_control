/****************************************************************************
 *
 * Daeyoung Ground Control - HUD panel (UI deck slide 10)
 *
 * Simple Mission Planner / Bluebird style HUD:
 *   - sky Deep Sky Blue, ground Brown, white text, red reference marks
 *   - heading tape on top (Mission Planner style)
 *   - airspeed tape (left) and relative altitude tape (right), m/s and m,
 *     climb rate ribbon on the altitude tape
 *   - pitch ladder (5 / 10 deg), bank scale (10, 20, 30, 45, 60 deg)
 *   - footer: ground speed, flight time, roll / pitch, MSL altitude, climb rate
 *   - no armed / disarmed text
 *
 * Flight time (hh:mm:ss) counts from takeoff (vehicle "flying"), stops at
 * landing and resets when the vehicle is armed for the next flight.
 *
 ****************************************************************************/

import QtQuick

import QGroundControl
import QGroundControl.Controls

KevaPanelFrame {
    id: root

    property var vehicle: QGroundControl.multiVehicleManager.activeVehicle

    function _fact(f) {
        return (vehicle && f && !isNaN(f.rawValue)) ? f.rawValue : NaN
    }
    function _signed(v, decimals) {
        if (isNaN(v)) {
            return "--"
        }
        return (v >= 0 ? "+" : "") + v.toFixed(decimals)
    }
    function _hhmmss(ms) {
        var s  = Math.floor(ms / 1000)
        var hh = Math.floor(s / 3600)
        var mm = Math.floor((s % 3600) / 60)
        var ss = s % 60
        return (hh < 10 ? "0" : "") + hh + ":" + (mm < 10 ? "0" : "") + mm + ":" + (ss < 10 ? "0" : "") + ss
    }

    readonly property real _roll:       vehicle ? _fact(vehicle.roll) : NaN
    readonly property real _pitch:      vehicle ? _fact(vehicle.pitch) : NaN
    readonly property real _heading:    vehicle ? _fact(vehicle.heading) : NaN
    readonly property real _airSpeed:   vehicle ? _fact(vehicle.airSpeed) : NaN
    readonly property real _gndSpeed:   vehicle ? _fact(vehicle.groundSpeed) : NaN
    readonly property real _climb:      vehicle ? _fact(vehicle.climbRate) : NaN
    readonly property real _altRel:     vehicle ? _fact(vehicle.altitudeRelative) : NaN
    readonly property real _altMsl:     vehicle ? _fact(vehicle.altitudeAMSL) : NaN
    readonly property real _rollDraw:   isNaN(_roll) ? 0 : _roll
    readonly property real _pitchDraw:  isNaN(_pitch) ? 0 : _pitch

    // Geometry
    readonly property real _pad:        Math.max(2, Math.round(height * 0.015))
    readonly property real _headH:      Math.round(height * 0.11)
    readonly property real _footH:      Math.round(height * 0.17)
    readonly property real _tapeW:      Math.round(width * 0.18)
    readonly property real _tapeTop:    _headH + _pad
    readonly property real _tapeH:      height - _footH - _pad - _tapeTop
    readonly property real _centerY:    _tapeTop + _tapeH / 2
    readonly property real _pxPerDeg:   _tapeH / 50     // +/- 25 deg between top and bottom of the tapes
    readonly property rect _ladderClip: Qt.rect(_tapeW + _pad, _tapeTop, width - 2 * (_tapeW + _pad), _tapeH)
    readonly property real _footPx:     Math.max(9, Math.round(_footH * 0.34))

    // ---- Flight time from takeoff ----
    property real _ftElapsedMs: 0   // time flown before the current airborne segment
    property real _ftStartMs:   0   // start of the current airborne segment, 0 when on the ground
    property real _nowMs:       Date.now()

    function _ftReset() {
        _ftElapsedMs = 0
        _ftStartMs = (vehicle && vehicle.flying) ? Date.now() : 0
    }
    onVehicleChanged: _ftReset()
    Component.onCompleted: _ftReset()

    Connections {
        target: root.vehicle
        ignoreUnknownSignals: true
        function onArmedChanged() {
            if (root.vehicle.armed) {
                root._ftReset()
            }
        }
        function onFlyingChanged() {
            if (root.vehicle.flying) {
                if (root._ftStartMs === 0) {
                    root._ftStartMs = Date.now()
                }
            } else if (root._ftStartMs !== 0) {
                root._ftElapsedMs += Date.now() - root._ftStartMs
                root._ftStartMs = 0
            }
        }
    }

    Timer {
        interval:   500
        running:    root._ftStartMs !== 0
        repeat:     true
        onTriggered: root._nowMs = Date.now()
    }

    readonly property real _flightTimeMs: _ftElapsedMs + (_ftStartMs !== 0 ? Math.max(0, _nowMs - _ftStartMs) : 0)

    // ---- Sky / ground ----
    Item {
        id:     horizon
        width:  Math.hypot(root.width, root.height) * 3
        height: width
        x:      root.width / 2 - width / 2
        y:      root._centerY - height / 2

        transform: [
            Translate { y: root._pitchDraw * root._pxPerDeg },
            Rotation {
                origin.x:   horizon.width / 2
                origin.y:   horizon.height / 2
                angle:      -root._rollDraw
            }
        ]

        Rectangle {
            width:  parent.width
            height: parent.height / 2
            color:  root.kevaColors.deepSkyBlue
        }
        Rectangle {
            y:      parent.height / 2
            width:  parent.width
            height: parent.height / 2
            color:  root.kevaColors.brown
        }
    }

    KevaHudAttitude {
        anchors.fill:   parent
        colors:         root.kevaColors
        roll:           root._rollDraw
        pitch:          root._pitchDraw
        centerY:        root._centerY
        pxPerDeg:       root._pxPerDeg
        clipRect:       root._ladderClip
        bankRadius:     Math.max(10, Math.min(root._tapeH / 2, root._ladderClip.width / 2) - root._pad * 2)
    }

    KevaHudHeadingTape {
        x:          1
        y:          1
        width:      root.width - 2
        height:     root._headH
        colors:     root.kevaColors
        heading:    root._heading
    }

    KevaHudTape {
        x:              1
        y:              root._tapeTop
        width:          root._tapeW
        height:         root._tapeH
        colors:         root.kevaColors
        title:          qsTr("AS m/s")
        value:          root._airSpeed
        range:          20
        minorStep:      1
        majorStep:      5
        noNegative:     true
        ticksOnRight:   true
    }

    KevaHudTape {
        x:              root.width - width - 1
        y:              root._tapeTop
        width:          root._tapeW
        height:         root._tapeH
        colors:         root.kevaColors
        title:          qsTr("ALT m")
        value:          root._altRel
        range:          40
        minorStep:      2
        majorStep:      10
        ticksOnRight:   false
        ribbonValue:    root.vehicle ? (isNaN(root._climb) ? 0 : root._climb) : NaN
        ribbonRange:    5
    }

    // ---- Footer ----
    Rectangle {
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        height:         root._footH
        color:          Qt.rgba(0, 0, 0, 0.5)

        component FooterText: QGCLabel {
            color:          root.kevaColors.white
            font.family:    "Arial"
            font.pixelSize: root._footPx
        }

        Column {
            anchors.left:           parent.left
            anchors.leftMargin:     root._pad * 2
            anchors.verticalCenter: parent.verticalCenter
            FooterText { text: qsTr("GS %1 m/s").arg(isNaN(root._gndSpeed) ? "--" : root._gndSpeed.toFixed(1)) }
            FooterText { text: qsTr("FT %1").arg(root._hhmmss(root._flightTimeMs)) }
        }

        Column {
            anchors.horizontalCenter:   parent.horizontalCenter
            anchors.verticalCenter:     parent.verticalCenter
            FooterText { text: qsTr("R %1°").arg(root._signed(root._roll, 1)) }
            FooterText { text: qsTr("P %1°").arg(root._signed(root._pitch, 1)) }
        }

        Column {
            anchors.right:          parent.right
            anchors.rightMargin:    root._pad * 2
            anchors.verticalCenter: parent.verticalCenter
            FooterText {
                anchors.right:  parent.right
                text:           qsTr("MSL %1 m").arg(isNaN(root._altMsl) ? "--" : root._altMsl.toFixed(1))
            }
            FooterText {
                anchors.right:  parent.right
                text:           qsTr("VS %1 m/s").arg(root._signed(root._climb, 1))
            }
        }
    }
}
