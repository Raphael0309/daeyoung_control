/****************************************************************************
 *
 * Daeyoung Ground Control - Status Data panel (UI deck slides 11 and 12)
 *
 * Block layout per slide 12 (5 columns, 2 rows):
 *
 *   | Alt (Rel) | Air spd.  |      | Lat. / Lon. / MGRS        |
 *   |-----------|-----------| Thr% |---------------------------|
 *   | Comm.     | Grnd spd. |      | Wind dir/spd | Dist. home/GCS |
 *
 * U6: layout with "--" values (shown before connection). Live values,
 * colour thresholds and the Comm. bar come in U9; the altitude / airspeed
 * dials in U10.
 *
 ****************************************************************************/

import QtQuick

import QGroundControl
import QGroundControl.Controls

KevaPanelFrame {
    id: root

    readonly property real  _pad:       Math.max(2, Math.round(height * 0.025))
    readonly property real  _titlePx:   Math.max(10, height * 0.065)
    readonly property real  _valuePx:   Math.max(12, height * 0.11)
    readonly property real  _smallPx:   Math.max(11, height * 0.07)
    readonly property real  _rowH:      (height - _pad * 3) / 2
    readonly property real  _innerW:    width - _pad * 6
    // Column widths taken from the slide 12 block diagram
    readonly property var   _colFrac:   [ 0.205, 0.175, 0.285, 0.177, 0.158 ]

    function _colW(i) { return _innerW * _colFrac[i] }
    function _colX(i) {
        var x = _pad
        for (var k = 0; k < i; k++) {
            x += _colW(k) + _pad
        }
        return x
    }
    function _rowY(r) { return _pad + r * (_rowH + _pad) }

    component Cell: Rectangle {
        property string title
        property var    lines:      [ "--" ]
        property real   valuePx:    root._valuePx

        color:          "transparent"
        border.color:   root.kevaColors.border
        border.width:   1
        radius:         3
        clip:           true

        Column {
            anchors.fill:       parent
            anchors.margins:    root._pad * 2
            spacing:            root._pad

            QGCLabel {
                text:           title
                color:          root.kevaColors.deepSkyBlue
                font.pixelSize: root._titlePx
                font.bold:      true
            }

            Repeater {
                model: lines

                QGCLabel {
                    width:          parent.width
                    text:           modelData
                    color:          root.kevaColors.white
                    font.pixelSize: valuePx
                    elide:          Text.ElideRight
                }
            }
        }
    }

    Cell {
        x: _colX(0); y: _rowY(0); width: _colW(0); height: _rowH
        title: qsTr("Alt. (Rel)")
    }
    Cell {
        x: _colX(0); y: _rowY(1); width: _colW(0); height: _rowH
        title: qsTr("Comm.")
    }
    Cell {
        x: _colX(1); y: _rowY(0); width: _colW(1); height: _rowH
        title: qsTr("Air Spd.")
    }
    Cell {
        x: _colX(1); y: _rowY(1); width: _colW(1); height: _rowH
        title: qsTr("Gnd Spd.")
    }
    Cell {
        x: _colX(2); y: _rowY(0); width: _colW(2); height: _rowH * 2 + _pad
        title:      qsTr("Thr%")
        valuePx:    _smallPx
        lines:      [ "FW  --", "M1  --", "M2  --", "M3  --", "M4  --" ]
    }
    Cell {
        x: _colX(3); y: _rowY(0); width: _colW(3) + _pad + _colW(4); height: _rowH
        title:      qsTr("Position")
        valuePx:    _smallPx
        lines:      [ "Lat.  --", "Lon.  --", "MGRS  --" ]
    }
    Cell {
        x: _colX(3); y: _rowY(1); width: _colW(3); height: _rowH
        title:      qsTr("Wind")
        valuePx:    _smallPx
        lines:      [ "Dir.  --", "Spd.  --" ]
    }
    Cell {
        x: _colX(4); y: _rowY(1); width: _colW(4); height: _rowH
        title:      qsTr("Dist.")
        valuePx:    _smallPx
        lines:      [ "Home  --", "GCS  --" ]
    }
}
