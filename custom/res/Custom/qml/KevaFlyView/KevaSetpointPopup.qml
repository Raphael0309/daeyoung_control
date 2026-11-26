/****************************************************************************
 *
 * Daeyoung Ground Control - target altitude / airspeed popup
 * (UI deck slide 11 and the review decisions)
 *
 * Click on Alt. (Rel) or Air Spd. opens it: turn the dial (or the mouse
 * wheel), or tap the number to type it, then slide to confirm. The popup
 * closes on confirm, Cancel, Esc or a click outside.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools

Popup {
    id: popup

    property string title
    property string unit
    property int    decimals:       0
    property real   currentValue:   NaN     // live value, shown for reference
    property real   minValue:       0
    property real   maxValue:       100
    property real   step:           1
    property string noteText                // what happens when it is sent
    property real   value:          minValue

    signal confirmed(real value)

    readonly property var   _c:     QGroundControl.corePlugin.kevaColors
    readonly property real  _fh:    ScreenTools.defaultFontPixelHeight
    readonly property real  _fw:    ScreenTools.defaultFontPixelWidth

    function _snap(v) {
        if (isNaN(v)) {
            return minValue
        }
        // Same rounding as CustomPlugin::kevaSetTarget*(): whole steps, then the range
        v = Math.round(v / step) * step
        return Math.max(minValue, Math.min(maxValue, v))
    }
    function _text(v) {
        return isNaN(v) ? "--" : v.toFixed(decimals)
    }

    /// Opens with the current value (rounded to the step) as the starting target
    function openWith(current) {
        currentValue = current
        value = _snap(current)
        numberEdit.visible = false
        confirmSlider.resetSpaceBarSliding()
        open()
        dial.forceActiveFocus()
    }

    modal:          false
    focus:          true
    closePolicy:    Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding:        _fw * 1.5
    width:          _fh * 17

    background: Rectangle {
        color:          popup._c.panel
        border.color:   popup._c.deepSkyBlue
        border.width:   1
        radius:         4
    }

    contentItem: Column {
        spacing: popup._fh * 0.5

        Item {
            width:  parent.width
            height: titleLabel.height

            QGCLabel {
                id:             titleLabel
                text:           popup.title
                color:          popup._c.deepSkyBlue
                font.bold:      true
                font.pointSize: ScreenTools.mediumFontPointSize
            }
            QGCLabel {
                anchors.right:          parent.right
                anchors.verticalCenter: parent.verticalCenter
                text:                   "✕"
                color:                  popup._c.white
                font.pointSize:         ScreenTools.mediumFontPointSize
                MouseArea {
                    anchors.fill:       parent
                    anchors.margins:    -popup._fw
                    onClicked:          popup.close()
                }
            }
        }

        Item {
            width:  parent.width
            height: width * 0.78

            KevaSetpointDial {
                id:                         dial
                anchors.horizontalCenter:   parent.horizontalCenter
                width:                      parent.height
                height:                     parent.height
                colors:                     popup._c
                value:                      popup.value
                minValue:                   popup.minValue
                maxValue:                   popup.maxValue
                step:                       popup.step
                onValueEdited: (newValue) => popup.value = newValue
            }

            // Number in the centre: tap to type
            Column {
                anchors.centerIn:   dial
                visible:            !numberEdit.visible

                QGCLabel {
                    anchors.horizontalCenter:   parent.horizontalCenter
                    text:                       popup._text(popup.value)
                    color:                      popup._c.white
                    font.pixelSize:             popup._fh * 2.2
                    font.bold:                  true
                }
                QGCLabel {
                    anchors.horizontalCenter:   parent.horizontalCenter
                    text:                       popup.unit
                    color:                      popup._c.deepSkyBlue
                }
            }
            MouseArea {
                anchors.centerIn:   dial
                width:              dial.centerRadius * 2
                height:             width
                visible:            !numberEdit.visible
                cursorShape:        Qt.IBeamCursor
                onClicked: {
                    numberEdit.text = popup._text(popup.value)
                    numberEdit.visible = true
                    numberEdit.forceActiveFocus()
                    numberEdit.selectAll()
                }
            }
            QGCTextField {
                id:                 numberEdit
                anchors.centerIn:   dial
                width:              dial.centerRadius * 1.9
                visible:            false
                horizontalAlignment: Text.AlignHCenter
                inputMethodHints:   Qt.ImhFormattedNumbersOnly
                validator:          DoubleValidator { bottom: popup.minValue; top: popup.maxValue; decimals: popup.decimals }
                onAccepted: {
                    popup.value = popup._snap(parseFloat(text))
                    visible = false
                    dial.forceActiveFocus()
                }
                Keys.onEscapePressed: (event) => {
                    visible = false
                    dial.forceActiveFocus()
                    event.accepted = true
                }
                onActiveFocusChanged: {
                    if (!activeFocus && visible) {
                        var v = parseFloat(text)
                        if (!isNaN(v)) {
                            popup.value = popup._snap(v)
                        }
                        visible = false
                    }
                }
            }
        }

        QGCLabel {
            width:                  parent.width
            horizontalAlignment:    Text.AlignHCenter
            text:                   qsTr("Current %1 %2   ·   Range %3 – %4 %2")
                                        .arg(popup._text(popup.currentValue)).arg(popup.unit)
                                        .arg(popup._text(popup.minValue)).arg(popup._text(popup.maxValue))
            color:                  popup._c.white
            font.pointSize:         ScreenTools.smallFontPointSize
        }

        QGCLabel {
            width:                  parent.width
            visible:                popup.noteText !== ""
            wrapMode:               Text.WordWrap
            horizontalAlignment:    Text.AlignHCenter
            text:                   popup.noteText
            color:                  popup._c.yellow
            font.pointSize:         ScreenTools.smallFontPointSize
        }

        SliderSwitch {
            id:             confirmSlider
            width:          parent.width
            height:         popup._fh * 2.6
            confirmText:    qsTr("Slide to set %1 %2").arg(popup._text(popup.value)).arg(popup.unit)
            onAccept: {
                popup.confirmed(popup.value)
                popup.close()
            }
        }
    }
}
