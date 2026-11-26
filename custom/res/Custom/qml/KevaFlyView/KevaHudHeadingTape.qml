/****************************************************************************
 *
 * Daeyoung Ground Control - HUD heading tape (Mission Planner style)
 *
 ****************************************************************************/

import QtQuick

Canvas {
    property real   heading:    NaN     // degrees
    property real   range:      90      // degrees shown across the width
    property var    colors

    onHeadingChanged:   requestPaint()
    onWidthChanged:     requestPaint()
    onHeightChanged:    requestPaint()

    readonly property var _cardinals: ({ 0: "N", 45: "NE", 90: "E", 135: "SE", 180: "S", 225: "SW", 270: "W", 315: "NW" })

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var w = width
        var h = height
        if (w <= 0 || h <= 0) {
            return
        }
        var cx  = w / 2
        var hd  = isNaN(heading) ? 0 : ((heading % 360) + 360) % 360
        var ppd = w / range
        var fpx = Math.max(9, Math.round(h * 0.36))

        ctx.fillStyle = "rgba(0,0,0,0.45)"
        ctx.fillRect(0, 0, w, h)

        ctx.strokeStyle  = colors.white
        ctx.fillStyle    = colors.white
        ctx.lineWidth    = 1
        ctx.textAlign    = "center"
        ctx.textBaseline = "top"
        var first = Math.floor((hd - range / 2) / 5) * 5
        for (var d = first; d <= hd + range / 2; d += 5) {
            var x    = cx + (d - hd) * ppd
            var norm = ((d % 360) + 360) % 360
            var major = norm % 15 === 0
            ctx.beginPath()
            ctx.moveTo(x, h)
            ctx.lineTo(x, h - (major ? h * 0.32 : h * 0.18))
            ctx.stroke()
            if (major && Math.abs(x - cx) > fpx * 1.8) {
                var card = _cardinals[norm]
                ctx.font = (card ? "bold " : "") + fpx + "px Arial, sans-serif"
                ctx.fillText(card ? card : norm.toString(), x, h * 0.08)
            }
        }

        // Current heading box and red index
        var bw = fpx * 2.6
        var bh = h * 0.62
        ctx.fillStyle   = colors.black
        ctx.fillRect(cx - bw / 2, 1, bw, bh)
        ctx.strokeStyle = colors.red
        ctx.lineWidth   = 2
        ctx.strokeRect(cx - bw / 2, 1, bw, bh)
        ctx.fillStyle   = colors.red
        ctx.beginPath()
        ctx.moveTo(cx, h)
        ctx.lineTo(cx - h * 0.16, h - h * 0.28)
        ctx.lineTo(cx + h * 0.16, h - h * 0.28)
        ctx.closePath()
        ctx.fill()

        ctx.fillStyle    = colors.white
        ctx.font         = "bold " + Math.round(fpx * 1.1) + "px Arial, sans-serif"
        ctx.textBaseline = "middle"
        var txt = isNaN(heading) ? "--" : ("00" + Math.round(hd) % 360).slice(-3)
        ctx.fillText(txt, cx, 1 + bh / 2)
    }
}
