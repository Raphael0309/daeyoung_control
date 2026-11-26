/****************************************************************************
 *
 * Daeyoung Ground Control - multi-turn setpoint dial (UI deck slide 11)
 *
 * - One step per 15 deg of turn (24 steps per revolution), as many turns as
 *   the range needs; mouse wheel / arrow keys: one step.
 * - The outer ticks turn with the value, the inner arc shows where the value
 *   sits in the whole range.
 * - The centre is left free for the number (handled by the popup).
 *
 ****************************************************************************/

import QtQuick

Item {
    id: dial

    property real   value:          0
    property real   minValue:       0
    property real   maxValue:       100
    property real   step:           1
    property real   degPerStep:     15
    property real   centerRadius:   width * 0.27    // inner area that does not turn the dial
    property var    colors
    property bool   dragging:       dragArea.pressed

    signal valueEdited(real newValue)

    activeFocusOnTab: true

    function _setSteps(n) {
        if (n === 0) {
            return
        }
        var v = Math.max(minValue, Math.min(maxValue, Math.round((value + n * step) / step) * step))
        if (v !== value) {
            valueEdited(v)
        }
    }

    Keys.onUpPressed:       _setSteps(1)
    Keys.onRightPressed:    _setSteps(1)
    Keys.onDownPressed:     _setSteps(-1)
    Keys.onLeftPressed:     _setSteps(-1)
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_PageUp) {
            _setSteps(10)
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            _setSteps(-10)
            event.accepted = true
        }
    }

    onValueChanged:     canvas.requestPaint()
    onMinValueChanged:  canvas.requestPaint()
    onMaxValueChanged:  canvas.requestPaint()

    Canvas {
        id:             canvas
        anchors.fill:   parent

        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cx = width / 2
            var cy = height / 2
            var r  = Math.min(width, height) / 2 - 2
            if (r <= 0) {
                return
            }
            var steps    = (dial.value - dial.minValue) / dial.step
            var turnRad  = steps * dial.degPerStep * Math.PI / 180
            var frac     = dial.maxValue > dial.minValue ? (dial.value - dial.minValue) / (dial.maxValue - dial.minValue) : 0

            // Knob body
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, 2 * Math.PI, false)
            ctx.fillStyle = "#1A1A1A"
            ctx.fill()
            ctx.lineWidth   = 2
            ctx.strokeStyle = dial.colors.border
            ctx.stroke()

            // Range arc (where the value is in the whole range)
            ctx.beginPath()
            ctx.arc(cx, cy, r * 0.62, -Math.PI / 2, -Math.PI / 2 + frac * 2 * Math.PI, false)
            ctx.lineWidth   = Math.max(3, r * 0.06)
            ctx.strokeStyle = dial.colors.deepSkyBlue
            ctx.stroke()

            // Turning ticks, one per step
            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(turnRad)
            var n = Math.round(360 / dial.degPerStep)
            for (var i = 0; i < n; i++) {
                var a     = i * 2 * Math.PI / n
                var major = (i % 6) === 0
                var len   = major ? r * 0.16 : r * 0.09
                ctx.beginPath()
                ctx.moveTo((r - 3) * Math.sin(a), -(r - 3) * Math.cos(a))
                ctx.lineTo((r - 3 - len) * Math.sin(a), -(r - 3 - len) * Math.cos(a))
                ctx.lineWidth   = major ? 2.5 : 1.5
                ctx.strokeStyle = dial.colors.white
                ctx.stroke()
            }
            // Grip mark
            ctx.beginPath()
            ctx.arc(0, -(r * 0.78), Math.max(3, r * 0.06), 0, 2 * Math.PI, false)
            ctx.fillStyle = dial.colors.neonGreen
            ctx.fill()
            ctx.restore()

            // Fixed red index at the top
            ctx.beginPath()
            ctx.moveTo(cx, cy - r + r * 0.2)
            ctx.lineTo(cx - r * 0.06, cy - r - 1)
            ctx.lineTo(cx + r * 0.06, cy - r - 1)
            ctx.closePath()
            ctx.fillStyle = dial.colors.red
            ctx.fill()
        }
    }

    MouseArea {
        id:             dragArea
        anchors.fill:   parent
        preventStealing: true

        property real _lastAngle:   0
        property real _accum:       0
        property bool _turning:     false

        function _angle(mx, my) {
            return Math.atan2(mx - width / 2, -(my - height / 2)) * 180 / Math.PI
        }

        onPressed: (mouse) => {
            var dx = mouse.x - width / 2
            var dy = mouse.y - height / 2
            _turning = Math.sqrt(dx * dx + dy * dy) > dial.centerRadius
            if (!_turning) {
                mouse.accepted = false      // centre: let the number underneath take the click
                return
            }
            dial.forceActiveFocus()
            _lastAngle = _angle(mouse.x, mouse.y)
            _accum = 0
        }
        onPositionChanged: (mouse) => {
            if (!_turning) {
                return
            }
            var a = _angle(mouse.x, mouse.y)
            var d = a - _lastAngle
            if (d > 180) {
                d -= 360
            } else if (d < -180) {
                d += 360
            }
            _lastAngle = a
            _accum += d
            var n = (_accum > 0 ? Math.floor(_accum / dial.degPerStep) : Math.ceil(_accum / dial.degPerStep))
            if (n !== 0) {
                _accum -= n * dial.degPerStep
                dial._setSteps(n)
            }
        }
        onWheel: (wheel) => {
            dial.forceActiveFocus()
            dial._setSteps(wheel.angleDelta.y > 0 ? 1 : (wheel.angleDelta.y < 0 ? -1 : 0))
        }
    }
}
