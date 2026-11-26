/****************************************************************************
 *
 * Daeyoung Ground Control - video slot shown when there is no video
 * (UI deck slide 9). U7 extends this to the "no frames for 5 s" case.
 *
 ****************************************************************************/

import QtQuick

import QGroundControl
import QGroundControl.Controls

KevaPanelFrame {
    color: kevaColors.black

    QGCLabel {
        anchors.fill:           parent
        anchors.margins:        parent.height * 0.05
        text:                   qsTr("Video Transmission Disconnected")
        color:                  kevaColors.red
        font.family:            "Arial"
        font.pixelSize:         Math.max(12, parent.height * 0.075)
        font.bold:              true
        wrapMode:               Text.WordWrap
        horizontalAlignment:    Text.AlignHCenter
        verticalAlignment:      Text.AlignVCenter
    }
}
