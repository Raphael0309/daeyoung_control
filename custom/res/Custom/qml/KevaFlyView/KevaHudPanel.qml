/****************************************************************************
 *
 * Daeyoung Ground Control - HUD panel (UI deck slide 10)
 *
 * U6: frame and static colours only (sky Deep Sky Blue, ground Brown, red
 * reference mark). The live attitude, tapes, heading and flight time come in U8.
 *
 ****************************************************************************/

import QtQuick

import QGroundControl
import QGroundControl.Controls

KevaPanelFrame {
    id: root

    Rectangle {
        id:             sky
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.top:    parent.top
        anchors.margins: 1
        height:         parent.height / 2
        color:          kevaColors.deepSkyBlue
    }

    Rectangle {
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.top:    sky.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 1
        anchors.topMargin: 0
        color:          kevaColors.brown
    }

    // Aircraft reference mark
    Rectangle {
        anchors.centerIn:   parent
        width:              parent.width * 0.3
        height:             Math.max(2, parent.height * 0.012)
        color:              kevaColors.red
    }

    QGCLabel {
        anchors.top:                parent.top
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.topMargin:          parent.height * 0.03
        text:                       qsTr("HUD")
        color:                      kevaColors.white
        font.pixelSize:             Math.max(10, parent.height * 0.06)
    }
}
