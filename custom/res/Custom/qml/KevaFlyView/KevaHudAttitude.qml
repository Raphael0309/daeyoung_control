/****************************************************************************
 *
 * Daeyoung Ground Control - HUD attitude: pitch ladder, bank scale and the
 * fixed red aircraft reference (sky / ground are drawn by KevaHudPanel).
 *
 * Pitch ladder: line every 5 deg, labelled every 10 deg.
 * Bank scale:   ticks at 0, 10, 20, 30, 45 and 60 deg, turning with the
 *               horizon; the red index at the top is fixed (Mission Planner).
 *
 ****************************************************************************/

import QtQuick

Canvas {
    property real   roll:           0       // degrees, right wing down positive
    property real   pitch:          0       // degrees, nose up positive
    property real   centerY:        height / 2
    property real   pxPerDeg:       4
    property rect   clipRect:       Qt.rect(0, 0, width, height)
    property real   bankRadius:     height / 3
    property var    colors

    onRollChanged:      requestPaint()
    onPitchChanged:     requestPaint()
    onWidthChanged:     requestPaint()
    onHeightChanged:    requestPaint()
    onCenterYChanged:   requestPaint()
    onClipRectChanged:  requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        if (width <= 0 || height <= 0) {
            return
        }
        var cx   = width / 2
        var cy   = centerY
        var cw   = clipRect.width
        var rRad = -roll * Math.PI / 180
        var fpx  = Math.max(8, Math.round(height * 0.045))

        ctx.save()
        ctx.beginPath()
        ctx.rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height)
        ctx.clip()

        // Pitch ladder (rotates and moves with the horizon)
        ctx.save()
        ctx.translate(cx, cy)
        ctx.rotate(rRad)
        ctx.translate(0, pitch * pxPerDeg)
        ctx.strokeStyle  = colors.white
        ctx.fillStyle    = colors.white
        ctx.lineWidth    = 1.5
        ctx.font         = fpx + "px Arial, sans-serif"
        ctx.textBaseline = "middle"
        // Horizon line
        ctx.beginPath()
        ctx.moveTo(-width, 0)
        ctx.lineTo(width, 0)
        ctx.stroke()
        var p0 = Math.ceil((pitch - 20) / 5) * 5
        for (var p = p0; p <= pitch + 20; p += 5) {
            if (p === 0 || p < -90 || p > 90) {
                continue
            }
            var y     = -p * pxPerDeg
            var major = p % 10 === 0
            var half  = major ? cw * 0.18 : cw * 0.08
            ctx.beginPath()
            ctx.moveTo(-half, y)
            ctx.lineTo(half, y)
            ctx.stroke()
            if (major) {
                ctx.textAlign = "right"
                ctx.fillText(Math.abs(p).toString(), -half - 4, y)
                ctx.textAlign = "left"
                ctx.fillText(Math.abs(p).toString(), half + 4, y)
            }
        }
        ctx.restore()

        // Bank scale (turns with the horizon)
        ctx.save()
        ctx.translate(cx, cy)
        ctx.rotate(rRad)
        ctx.strokeStyle = colors.white
        ctx.lineWidth   = 1.5
        ctx.beginPath()
        ctx.arc(0, 0, bankRadius, (-90 - 60) * Math.PI / 180, (-90 + 60) * Math.PI / 180, false)
        ctx.stroke()
        var marks = [ -60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60 ]
        for (var i = 0; i < marks.length; i++) {
            var a   = marks[i] * Math.PI / 180
            var big = (marks[i] % 30 === 0)
            var len = big ? bankRadius * 0.12 : bankRadius * 0.07
            ctx.beginPath()
            ctx.moveTo(bankRadius * Math.sin(a), -bankRadius * Math.cos(a))
            ctx.lineTo((bankRadius - len) * Math.sin(a), -(bankRadius - len) * Math.cos(a))
            ctx.stroke()
        }
        ctx.restore()

        ctx.restore()   // clip

        // Fixed red references: bank index, aircraft wings and centre
        ctx.fillStyle   = colors.red
        ctx.strokeStyle = colors.red
        var tri = Math.max(5, bankRadius * 0.08)
        ctx.beginPath()
        ctx.moveTo(cx, cy - bankRadius + 1)
        ctx.lineTo(cx - tri * 0.7, cy - bankRadius + 1 + tri)
        ctx.lineTo(cx + tri * 0.7, cy - bankRadius + 1 + tri)
        ctx.closePath()
        ctx.fill()

        ctx.lineWidth = Math.max(2, height * 0.012)
        var wIn  = cw * 0.08
        var wOut = cw * 0.30
        var drop = height * 0.025
        ctx.beginPath()
        ctx.moveTo(cx - wOut, cy)
        ctx.lineTo(cx - wIn, cy)
        ctx.lineTo(cx - wIn, cy + drop)
        ctx.moveTo(cx + wOut, cy)
        ctx.lineTo(cx + wIn, cy)
        ctx.lineTo(cx + wIn, cy + drop)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(cx, cy, Math.max(2, height * 0.012), 0, 2 * Math.PI, false)
        ctx.fill()
    }
}
