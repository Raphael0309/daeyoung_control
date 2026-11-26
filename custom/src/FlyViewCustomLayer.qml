/****************************************************************************
 *
 * Daeyoung Ground Control - custom Fly view layer (UI deck slide 6)
 *
 * Bottom strip, left to right: video window | HUD panel | Status Data panel.
 *
 * - The strip height is 30% of the Fly view (about 310 px at 1920x1080).
 * - The video window is QGC's picture-in-picture view, always shown (black
 *   "Video Transmission Disconnected" without video). Click swaps it with the
 *   map, double-click or the top-left button opens it in a separate window,
 *   the bottom-left button collapses it, the corner handle resizes it. Its
 *   default size is set so that it lines up with the strip.
 * - The HUD follows the right edge of the video window, the Status Data panel
 *   takes the remaining width.
 * - The custom example's compass / attitude widgets were removed.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools

import "qrc:/Custom/qml/KevaFlyView"

Item {
    id: _root

    property var parentToolInsets                       // These insets tell you what screen real estate is available for positioning the controls in your overlay
    property var totalToolInsets:   _totalToolInsets    // The insets updated for the custom overlay additions
    property var mapControl

    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property real   _toolsMargin:       ScreenTools.defaultFontPixelWidth * 0.75
    readonly property var _kevaColors:  QGroundControl.corePlugin.kevaColors

    // Bottom strip geometry
    readonly property real _panelH:         Math.round(Math.max(ScreenTools.defaultFontPixelHeight * 10, height * 0.30))
    readonly property real _videoDefaultW:  Math.round(_panelH * 16 / 9)
    readonly property real _hudW:           Math.round(_panelH * 1.28)
    readonly property real _stripInset:     _panelH + _toolsMargin * 2

    // QGC's picture-in-picture video view lives in FlyView.qml (id _pipView)
    readonly property bool _hasPip:         typeof _pipView !== "undefined"
    readonly property bool _pipShown:       _hasPip && _pipView.visible
    readonly property real _videoRight: {
        if (_pipShown) {
            return _pipView.x + (_pipView._isExpanded ? _pipView.width : ScreenTools.defaultFontPixelHeight * 2)
        }
        return 0    // video in a separate window: the HUD moves to the left edge
    }

    function _applyVideoSize() {
        if (_hasPip) {
            _pipView._pipSize = _videoDefaultW
        }
    }

    Component.onCompleted:      _applyVideoSize()
    on_VideoDefaultWChanged:    _applyVideoSize()

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
        bottomEdgeLeftInset:    Math.max(parentToolInsets.bottomEdgeLeftInset, _stripInset)
        bottomEdgeCenterInset:  Math.max(parentToolInsets.bottomEdgeCenterInset, _stripInset)
        bottomEdgeRightInset:   Math.max(parentToolInsets.bottomEdgeRightInset, _stripInset)
    }

    KevaHudPanel {
        id:                     hudPanel
        x:                      _videoRight + _toolsMargin
        anchors.bottom:         parent.bottom
        anchors.bottomMargin:   _toolsMargin
        width:                  _hudW
        height:                 _panelH
    }

    KevaStatusDataPanel {
        id:                     statusPanel
        anchors.left:           hudPanel.right
        anchors.leftMargin:     _toolsMargin
        anchors.right:          parent.right
        anchors.rightMargin:    _toolsMargin
        anchors.bottom:         parent.bottom
        anchors.bottomMargin:   _toolsMargin
        height:                 _panelH
    }

    // Administrator sessions monitor only (SR-15): make that obvious on screen
    Rectangle {
        id:                         adminBanner
        visible:                    QGroundControl.corePlugin.kevaIsAdmin
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.bottom:             hudPanel.top
        anchors.bottomMargin:       _toolsMargin * 2
        width:                      adminBannerText.implicitWidth + ScreenTools.defaultFontPixelWidth * 3
        height:                     adminBannerText.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.6
        radius:                     ScreenTools.defaultFontPixelHeight / 4
        color:                      Qt.rgba(0, 0, 0, 0.7)
        border.color:               _kevaColors.orange
        border.width:               1
        z:                          1000

        QGCLabel {
            id:                 adminBannerText
            anchors.centerIn:   parent
            text:               qsTr("ADMINISTRATOR (%1) - MONITORING ONLY").arg(QGroundControl.corePlugin.kevaUserId)
            color:              _kevaColors.orange
            font.bold:          true
        }
    }
}
