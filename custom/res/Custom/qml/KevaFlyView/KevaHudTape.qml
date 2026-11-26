/****************************************************************************
 *
 * Daeyoung Ground Control - HUD vertical tape (speed / altitude)
 *
 * Bluebird / Mission Planner style: moving scale, fixed value box with a red
 * reference frame. The altitude tape can draw the climb rate ribbon on its
 * outer edge.
 *
 ****************************************************************************/

import QtQuick

Canvas {
    id: tape

    property real   value:          NaN
    property real   range:          20      // span shown, in units
    property real   minorStep:      1
    property real   majorStep:      5
    property bool   ticksOnRight:   true    // left tape: ticks on its right edge (towards the centre)
    property bool   noNegative:     false   // speed tapes: no labels below 0
    property int    decimals:       1
    property string title:          ""
    property real   ribbonValue:    NaN     // climb rate (m/s); NaN = no ribbon
    property real   ribbonRange:    5       // full ribbon length = +/- this value
    property var    colors

    onValueChanged:         requestPaint()
    onRibbonValueChanged:   requestPaint()
    onWidthChanged:         requestPaint()
    onHeightChanged:        requestPaint()

    function _fmt(v) {
        if (isNaN(v)) {
            return "--"
        }
        return Math.abs(v) >= 1000 ? v.toFixed(0) : v.toFixed(decimals)
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var w = width
        var h = height
        if (w <= 0 || h <= 0) {
            return
        }
        var cy  = h / 2
        var v   = isNaN(value) ? 0 : value
        var ppu = h / range
        var fpx = Math.max(9, Math.round(h * 0.07))

        ctx.fillStyle = "rgba(0,0,0,0.35)"
        ctx.fillRect(0, 0, w, h)

        // Scale
        ctx.strokeStyle  = colors.white
        ctx.fillStyle    = colors.white
        ctx.lineWidth    = 1
        ctx.font         = fpx + "px Arial, sans-serif"
        ctx.textBaseline = "middle"
        ctx.textAlign    = ticksOnRight ? "right" : "left"
        var first = Math.floor((v - range / 2) / minorStep) * minorStep
        for (var t = first; t <= v + range / 2 + 0.0001; t += minorStep) {
            if (noNegative && t < -0.0001) {
                continue
            }
            var y      = cy - (t - v) * ppu
            var major  = Math.abs(t / majorStep - Math.round(t / majorStep)) < 0.0001
            var len    = major ? w * 0.22 : w * 0.12
            ctx.beginPath()
            if (ticksOnRight) {
                ctx.moveTo(w, y)
                ctx.lineTo(w - len, y)
            } else {
                ctx.moveTo(0, y)
                ctx.lineTo(len, y)
            }
            ctx.stroke()
            // Labels are left out next to the value box and under the title
            if (major && Math.abs(y - cy) > fpx * 1.3 && (title === "" || y > fpx * 1.8)) {
                ctx.fillText(Math.round(t).toString(), ticksOnRight ? w - len - 3 : len + 3, y)
            }
        }

        // Climb rate ribbon on the outer edge
        if (!isNaN(ribbonValue)) {
            var rw   = Math.max(3, w * 0.1)
            var rx   = ticksOnRight ? 0 : w - rw
            var frac = Math.max(-1, Math.min(1, ribbonValue / ribbonRange))
            var rlen = frac * (h / 2 - fpx)
            ctx.fillStyle = frac >= 0 ? colors.neonGreen : colors.orange
            if (rlen >= 0) {
                ctx.fillRect(rx, cy - rlen, rw, rlen)
            } else {
                ctx.fillRect(rx, cy, rw, -rlen)
            }
        }

        // Title
        if (title !== "") {
            ctx.fillStyle    = colors.deepSkyBlue
            ctx.font         = "bold " + Math.max(8, Math.round(fpx * 0.85)) + "px Arial, sans-serif"
            ctx.textAlign    = "center"
            ctx.textBaseline = "top"
            ctx.fillText(title, w / 2, 2)
        }

        // Value box with pointer towards the scale, red reference frame
        var bh = Math.round(fpx * 2)
        var bw = w * 0.8
        var bx = ticksOnRight ? 1 : w - bw - 1
        var tip = w * 0.14
        ctx.beginPath()
        if (ticksOnRight) {
            ctx.moveTo(bx, cy - bh / 2)
            ctx.lineTo(bx + bw, cy - bh / 2)
            ctx.lineTo(bx + bw + tip, cy)
            ctx.lineTo(bx + bw, cy + bh / 2)
            ctx.lineTo(bx, cy + bh / 2)
        } else {
            ctx.moveTo(bx + bw, cy - bh / 2)
            ctx.lineTo(bx, cy - bh / 2)
            ctx.lineTo(bx - tip, cy)
            ctx.lineTo(bx, cy + bh / 2)
            ctx.lineTo(bx + bw, cy + bh / 2)
        }
        ctx.closePath()
        ctx.fillStyle   = colors.black
        ctx.fill()
        ctx.strokeStyle = colors.red
        ctx.lineWidth   = 2
        ctx.stroke()

        ctx.fillStyle    = colors.white
        ctx.font         = "bold " + Math.round(fpx * 1.3) + "px Arial, sans-serif"
        ctx.textAlign    = "center"
        ctx.textBaseline = "middle"
        ctx.fillText(_fmt(value), bx + bw / 2, cy)
    }
}
