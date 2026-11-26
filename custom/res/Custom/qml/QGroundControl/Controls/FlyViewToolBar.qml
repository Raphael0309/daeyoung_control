/****************************************************************************
 *
 * Daeyoung Ground Control - Fly view toolbar (UI deck slides 7-8)
 *
 * Replaces QGC's src/QmlControls/FlyViewToolBar.qml through the custom
 * resource override (/Custom/qml/QGroundControl/Controls/FlyViewToolBar.qml).
 *
 *   [company logo] [status box] [flight mode] [MR/FW] [GPS] [battery ...] [vehicle selector] ...... [KEVA]
 *
 * The company logo (custom/res/QGCLogoFull.svg) opens the main menu. "KEVA" is the
 * product brand at the right end, shown while a vehicle is connected - the place
 * where QGC shows its vehicle brand image.
 *
 * Status box colours (QGC's red / yellow / green, green shown as Deep Sky Blue):
 *   Not Ready / Comms Lost : Red,           white text
 *   warnings               : Yellow,        black text
 *   Ready To Fly           : Deep Sky Blue, black text
 *   Armed / Flying         : Deep Sky Blue, white text
 *   Disconnected           : dark grey,     white text
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.MultiVehicleManager
import QGroundControl.ScreenTools
import QGroundControl.Controllers

Rectangle {
    id:     _root
    width:  parent.width
    height: ScreenTools.toolbarHeight
    color:  qgcPal.toolbarBackground

    property var    _activeVehicle:     QGroundControl.multiVehicleManager.activeVehicle
    property bool   _communicationLost: _activeVehicle ? _activeVehicle.vehicleLinkManager.communicationLost : false
    readonly property var _kc:          QGroundControl.corePlugin.kevaColors

    // Written by KevaMainStatusIndicator as "red" / "yellow" / "green" (QGC convention)
    property color  _mainStatusBGColor: qgcPal.brandingPurple

    readonly property color _statusBoxColor: {
        if (!_activeVehicle) {
            return "#2A2A2A"
        }
        if (Qt.colorEqual(_mainStatusBGColor, "red")) {
            return _kc.red
        }
        if (Qt.colorEqual(_mainStatusBGColor, "yellow")) {
            return _kc.yellow
        }
        if (Qt.colorEqual(_mainStatusBGColor, "green")) {
            return _kc.deepSkyBlue
        }
        return "#2A2A2A"
    }
    readonly property bool  _readyToFlyShown: _activeVehicle && !_communicationLost && !_activeVehicle.armed &&
                                              Qt.colorEqual(_statusBoxColor, _kc.deepSkyBlue)
    readonly property color _mainStatusTextColor: (_readyToFlyShown || Qt.colorEqual(_statusBoxColor, _kc.yellow)) ? "black" : "white"

    function dropMainStatusIndicatorTool() {
        mainStatusIndicator.dropMainStatusIndicator();
    }

    QGCPalette { id: qgcPal }

    // Bottom divider
    Rectangle {
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.bottom: parent.bottom
        height:         1
        color:          _kc.border
    }

    RowLayout {
        id:                     viewButtonRow
        anchors.bottomMargin:   1
        anchors.top:            parent.top
        anchors.bottom:         parent.bottom
        spacing:                ScreenTools.defaultFontPixelWidth

        // Company logo: opens the main menu (Plan Flight, Analyze Tools, Vehicle Configuration, Application Settings)
        QGCToolBarButton {
            id:                     currentButton
            Layout.preferredHeight: viewButtonRow.height
            icon.source:            "/res/QGCLogoFull.svg"
            logo:                   true
            onClicked:              mainWindow.showToolSelectDialog()
        }

        // Status box
        Rectangle {
            Layout.preferredHeight: viewButtonRow.height * 0.72
            Layout.preferredWidth:  mainStatusIndicator.implicitWidth + ScreenTools.defaultFontPixelWidth * 2
            Layout.alignment:       Qt.AlignVCenter
            Layout.leftMargin:      ScreenTools.defaultFontPixelWidth
            radius:                 ScreenTools.defaultFontPixelHeight / 6
            color:                  _statusBoxColor

            KevaMainStatusIndicator {
                id:                 mainStatusIndicator
                anchors.centerIn:   parent
                height:             parent.height
            }
        }

        QGCButton {
            id:                 disconnectButton
            text:               qsTr("Disconnect")
            onClicked:          _activeVehicle.closeVehicle()
            visible:            _activeVehicle && _communicationLost
        }
    }

    QGCFlickable {
        id:                     toolsFlickable
        anchors.leftMargin:     ScreenTools.defaultFontPixelWidth * ScreenTools.largeFontPointRatio * 1.5
        anchors.rightMargin:    ScreenTools.defaultFontPixelWidth / 2
        anchors.left:           viewButtonRow.right
        anchors.bottomMargin:   1
        anchors.top:            parent.top
        anchors.bottom:         parent.bottom
        anchors.right:          parent.right
        contentWidth:           toolIndicators.width
        flickableDirection:     Flickable.HorizontalFlick

        // Indicators come from CustomFirmwarePlugin::toolIndicators() / modeIndicators()
        FlyViewToolBarIndicators { id: toolIndicators }
    }

    // Product brand at the right end, while a vehicle is connected (replaces QGC's brand image)
    QGCLabel {
        anchors.right:          parent.right
        anchors.rightMargin:    ScreenTools.defaultFontPixelWidth * 1.5
        anchors.verticalCenter: parent.verticalCenter
        text:                   "KEVA"
        color:                  _kc.deepSkyBlue
        font.bold:              true
        font.pointSize:         ScreenTools.largeFontPointSize * 1.4
        visible:                _activeVehicle && !_communicationLost &&
                                x > (toolsFlickable.x + toolsFlickable.contentWidth + ScreenTools.defaultFontPixelWidth)
    }

    // Small parameter download progress bar
    Rectangle {
        anchors.bottom: parent.bottom
        height:         _root.height * 0.05
        width:          _activeVehicle ? _activeVehicle.loadProgress * parent.width : 0
        color:          _kc.deepSkyBlue
    }
}
