/****************************************************************************
 *
 * Daeyoung Ground Control - idle lock screen (MIL edition)
 *
 * Full screen and application modal: the main window cannot be used until
 * the current user's password is entered. Closing is not allowed; "Exit"
 * closes the application and records the logout.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id:         lockWindow
    visible:    true
    width:      1280
    height:     800
    title:      auth.productName
    color:      "#000000"
    modality:   Qt.ApplicationModal
    flags:      Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    readonly property color brandBlue:  "#00BFFF"
    readonly property color errorRed:   "#FF0000"

    onClosing: (close) => { close.accepted = false }

    function submit() {
        auth.unlockSession(pwField.text)
        pwField.text = ""
        pwField.forceActiveFocus()
    }

    ColumnLayout {
        anchors.centerIn:   parent
        spacing:            10

        Image {
            Layout.alignment:   Qt.AlignHCenter
            source:             "qrc:/KevaSecurity/logo.png"
            sourceSize.height:  60
            fillMode:           Image.PreserveAspectFit
        }
        Text {
            Layout.alignment:   Qt.AlignHCenter
            Layout.topMargin:   8
            text:               "Ground Control"
            color:              lockWindow.brandBlue
            font.family:        "Arial"
            font.pixelSize:     36
            font.bold:          true
        }
        Text {
            Layout.alignment:   Qt.AlignHCenter
            Layout.topMargin:   12
            text:               auth.sessionTerminated ? "Account Locked" : "Session Locked"
            color:              auth.sessionTerminated ? lockWindow.errorRed : "#FFFFFF"
            font.family:        "Arial"
            font.pixelSize:     22
            font.bold:          true
        }
        Text {
            Layout.alignment:       Qt.AlignHCenter
            Layout.maximumWidth:    620
            text:                   auth.lockText
            color:                  "#A6A6A6"
            font.family:            "Arial"
            font.pixelSize:         15
            wrapMode:               Text.WordWrap
            horizontalAlignment:    Text.AlignHCenter
        }

        RowLayout {
            Layout.alignment:   Qt.AlignHCenter
            Layout.topMargin:   12
            spacing:            8
            visible:            !auth.sessionTerminated

            TextField {
                id:                     pwField
                Layout.preferredWidth:  300
                Layout.preferredHeight: 34
                placeholderText:        "PW"
                echoMode:               TextInput.Password
                passwordCharacter:      "●"
                inputMethodHints:       Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                font.family:            "Arial"
                font.pixelSize:         16
                font.bold:              true
                color:                  "#000000"
                placeholderTextColor:   "#BFBFBF"
                leftPadding:            8
                focus:                  true
                background: Rectangle {
                    color:          "#FFFFFF"
                    border.width:   2
                    border.color:   pwField.activeFocus ? lockWindow.brandBlue : "#FFFFFF"
                }
                Component.onCompleted: forceActiveFocus()
                Keys.onPressed: (event) => {
                    if (event.matches(StandardKey.Paste) || event.matches(StandardKey.Copy) || event.matches(StandardKey.Cut)) {
                        event.accepted = true
                    }
                }
                Keys.onReturnPressed: lockWindow.submit()
                Keys.onEnterPressed:  lockWindow.submit()
                onTextChanged:        if (text.length > 0) auth.clearMessage()
            }
            Button {
                id:                     unlockButton
                Layout.preferredWidth:  94
                Layout.preferredHeight: 34
                text:                   "Unlock"
                font.family:            "Arial"
                font.pixelSize:         16
                font.bold:              true
                onClicked:              lockWindow.submit()
                contentItem: Text {
                    text:                   unlockButton.text
                    font:                   unlockButton.font
                    color:                  "#000000"
                    horizontalAlignment:    Text.AlignHCenter
                    verticalAlignment:      Text.AlignVCenter
                }
                background: Rectangle { color: unlockButton.down ? "#0099CC" : lockWindow.brandBlue }
            }
        }

        Text {
            Layout.alignment:       Qt.AlignHCenter
            Layout.maximumWidth:    520
            visible:                auth.message.length > 0
            text:                   auth.message
            color:                  lockWindow.errorRed
            font.family:            "Arial"
            font.pixelSize:         13
            wrapMode:               Text.WordWrap
            horizontalAlignment:    Text.AlignHCenter
        }

        Button {
            id:                 exitButton
            Layout.alignment:   Qt.AlignHCenter
            Layout.topMargin:   18
            text:               "Exit Application"
            font.family:        "Arial"
            font.pixelSize:     14
            onClicked:          auth.requestExit()
            contentItem: Text {
                text:                   exitButton.text
                font:                   exitButton.font
                color:                  "#FFFFFF"
                horizontalAlignment:    Text.AlignHCenter
                verticalAlignment:      Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth:  160
                implicitHeight: 32
                color:          exitButton.down ? "#333333" : "#1A1A1A"
                border.color:   lockWindow.brandBlue
            }
        }
    }
}
