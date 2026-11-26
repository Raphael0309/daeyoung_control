/****************************************************************************
 *
 * Daeyoung Ground Control - common frame for the Fly view bottom panels
 *
 ****************************************************************************/

import QtQuick

import QGroundControl

Rectangle {
    readonly property var kevaColors: QGroundControl.corePlugin.kevaColors

    color:          kevaColors.panel
    border.color:   kevaColors.border
    border.width:   1
    radius:         4
    clip:           true
}
