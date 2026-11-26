/****************************************************************************
 *
 * Daeyoung Ground Control - custom Fly view layer
 *
 * The custom example's compass tape and attitude/compass dials were removed
 * (UI deck: HUD and Status Data panels replace them). This layer now holds
 * the administrator "monitoring only" banner; the bottom panels are added in
 * later UI steps.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools

Item {
    property var parentToolInsets                       // These insets tell you what screen real estate is available for positioning the controls in your overlay
    property var totalToolInsets:   _totalToolInsets    // The insets updated for the custom overlay additions
    property var mapControl

    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property real   _toolsMargin:       ScreenTools.defaultFontPixelWidth * 0.75

    QGCPalette { id: qgcPal }

    QGCToolInsets {
        id:                     _totalToolInsets
        leftEdgeTopInset:       parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset:    parentToolInsets.leftEdgeCenterInset
        leftEdgeBottomInset:    parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset:      parentToolInsets.rightEdgeTopInset
        rightEdgeCenterInset:   parentToolInsets.rightEdgeCenterInset
        rightEdgeBottomInset:   parentToolInsets.rightEdgeBottomInset
        topEdgeLeftInset:       parentToolInsets.topEdgeLeftInset
        topEdgeCenterInset:     parentToolInsets.topEdgeCenterInset
        topEdgeRightInset:      parentToolInsets.topEdgeRightInset
        bottomEdgeLeftInset:    parentToolInsets.bottomEdgeLeftInset
        bottomEdgeCenterInset:  parentToolInsets.bottomEdgeCenterInset
        bottomEdgeRightInset:   parentToolInsets.bottomEdgeRightInset
    }

    // Administrator sessions monitor only (SR-15): make that obvious on screen
    Rectangle {
        id:                         adminBanner
        visible:                    QGroundControl.corePlugin.kevaIsAdmin
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.top:                parent.top
        anchors.topMargin:          parentToolInsets.topEdgeCenterInset + _toolsMargin
        width:                      adminBannerText.implicitWidth + ScreenTools.defaultFontPixelWidth * 3
        height:                     adminBannerText.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.6
        radius:                     ScreenTools.defaultFontPixelHeight / 4
        color:                      Qt.rgba(0, 0, 0, 0.7)
        border.color:               QGroundControl.corePlugin.kevaColors.orange
        border.width:               1
        z:                          1000

        QGCLabel {
            id:                 adminBannerText
            anchors.centerIn:   parent
            text:               qsTr("ADMINISTRATOR (%1) - MONITORING ONLY").arg(QGroundControl.corePlugin.kevaUserId)
            color:              QGroundControl.corePlugin.kevaColors.orange
            font.bold:          true
        }
    }
}
