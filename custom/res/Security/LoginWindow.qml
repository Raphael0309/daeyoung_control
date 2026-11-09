/****************************************************************************
 *
 * Daeyoung Ground Control - login window (MIL edition)
 *
 * Shown before the main application starts. All logic lives in the
 * AuthManager C++ object exposed as "auth"; this file only renders the
 * current stage.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id:         root
    visible:    true
    width:      1280
    height:     800
    minimumWidth:   1024
    minimumHeight:  700
    title:      auth.productName
    color:      "#000000"
    font.family: "Arial"

    readonly property color brandBlue:  "#00BFFF"
    readonly property color errorRed:   "#FF0000"
    readonly property color textWhite:  "#FFFFFF"
    readonly property color dimText:    "#A6A6A6"
    readonly property int   fieldWidth: 300
    readonly property int   fieldHeight: 34
    property bool           _finished:  false

    Component.onCompleted: root.showMaximized()

    Connections {
        target: auth
        function onFinished(ok) { root._finished = true }
    }

    onClosing: (close) => {
        if (!root._finished) {
            auth.cancel()
        }
    }

    // ------------------------------------------------------------------
    // Reusable controls

    component KField: TextField {
        Layout.preferredWidth:  root.fieldWidth
        Layout.preferredHeight: root.fieldHeight
        font.family:            "Arial"
        font.pixelSize:         16
        font.bold:              true
        color:                  "#000000"
        placeholderTextColor:   "#BFBFBF"
        selectByMouse:          true
        leftPadding:            8
        background: Rectangle {
            color:          "#FFFFFF"
            border.width:   2
            border.color:   parent.activeFocus ? root.brandBlue : "#FFFFFF"
        }
    }

    // Password input: masked, no paste (SR-11)
    component PwField: KField {
        echoMode:           TextInput.Password
        passwordCharacter:  "●"
        inputMethodHints:   Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
        Keys.onPressed: (event) => {
            if (event.matches(StandardKey.Paste) || event.matches(StandardKey.Copy) || event.matches(StandardKey.Cut)) {
                event.accepted = true
            }
        }
    }

    component KButton: Button {
        id:                 kbutton
        property bool primary: true
        font.family:        "Arial"
        font.pixelSize:     16
        font.bold:          true
        implicitHeight:     root.fieldHeight
        implicitWidth:      Math.max(120, contentItem.implicitWidth + 32)
        contentItem: Text {
            text:                   kbutton.text
            font:                   kbutton.font
            color:                  kbutton.primary ? "#000000" : root.textWhite
            horizontalAlignment:    Text.AlignHCenter
            verticalAlignment:      Text.AlignVCenter
            wrapMode:               Text.WordWrap
            opacity:                kbutton.enabled ? 1 : 0.5
        }
        background: Rectangle {
            color:          kbutton.primary ? (kbutton.down ? "#0099CC" : root.brandBlue) : (kbutton.down ? "#333333" : "#1A1A1A")
            border.width:   kbutton.primary ? 0 : 1
            border.color:   root.brandBlue
            opacity:        kbutton.enabled ? 1 : 0.4
        }
    }

    component LinkButton: Text {
        id:             link
        signal clicked()
        color:          root.brandBlue
        font.family:    "Arial"
        font.pixelSize: 14
        font.underline: linkArea.containsMouse
        MouseArea {
            id:             linkArea
            anchors.fill:   parent
            hoverEnabled:   true
            cursorShape:    Qt.PointingHandCursor
            onClicked:      link.clicked()
        }
    }

    component BodyText: Text {
        color:          root.textWhite
        font.family:    "Arial"
        font.pixelSize: 16
        wrapMode:       Text.WordWrap
    }

    // Red messages under the input fields
    component ErrorList: ColumnLayout {
        spacing: 2
        Layout.preferredWidth: root.fieldWidth + 140
        Repeater {
            model: auth.messages.length > 0 ? auth.messages : (auth.message.length > 0 ? [auth.message] : [])
            Text {
                Layout.fillWidth:   true
                text:               modelData
                color:              root.errorRed
                font.family:        "Arial"
                font.pixelSize:     13
                wrapMode:           Text.WordWrap
            }
        }
    }

    component RulesPanel: Rectangle {
        Layout.preferredWidth:  360
        Layout.preferredHeight: rulesText.implicitHeight + 24
        color:                  "transparent"
        border.color:           "#404040"
        Text {
            id:                 rulesText
            anchors.fill:       parent
            anchors.margins:    12
            text:               auth.passwordRules
            color:              root.dimText
            font.family:        "Arial"
            font.pixelSize:     13
            wrapMode:           Text.WordWrap
            lineHeight:         1.15
        }
    }

    // ------------------------------------------------------------------
    // Background: blue grid lines in the lower left corner

    Canvas {
        id:             gridLines
        anchors.left:   parent.left
        anchors.bottom: parent.bottom
        width:          parent.width * 0.42
        height:         parent.height * 0.62
        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.brandBlue
            ctx.lineWidth = 2
            const w = width
            const h = height
            // Lines rising to the right
            for (let i = 0; i < 11; ++i) {
                const x0 = -0.05 * w + i * 0.085 * w
                ctx.beginPath()
                ctx.moveTo(x0, h)
                ctx.lineTo(x0 + 0.42 * w, h - 0.75 * h + i * 0.03 * h)
                ctx.stroke()
            }
            // Lines falling to the right
            for (let j = 0; j < 7; ++j) {
                const y0 = 0.18 * h + j * 0.12 * h
                ctx.beginPath()
                ctx.moveTo(0, y0)
                ctx.lineTo(w * (0.9 - j * 0.05), h + 0.02 * h)
                ctx.stroke()
            }
        }
    }

    Text {
        anchors.right:          parent.right
        anchors.bottom:         parent.bottom
        anchors.rightMargin:    40
        anchors.bottomMargin:   30
        text:                   auth.companyName
        color:                  root.textWhite
        font.family:            "Arial"
        font.pixelSize:         16
    }

    // ------------------------------------------------------------------
    // Header and stage content

    Flickable {
        anchors.fill:       parent
        contentWidth:       width
        contentHeight:      Math.max(height, column.implicitHeight + 80)
        clip:               true
        boundsBehavior:     Flickable.StopAtBounds

        // Keeps the background lines from running through text
        Rectangle {
            x:      column.x - 32
            y:      column.y - 24
            width:  column.width + 64
            height: column.height + 48
            color:  "#000000"
        }

        ColumnLayout {
            id:                 column
            anchors.horizontalCenter: parent.horizontalCenter
            y:                  Math.max(40, (parent.height - implicitHeight) / 2 - 40)
            spacing:            6

            Image {
                Layout.alignment:       Qt.AlignHCenter
                source:                 "qrc:/KevaSecurity/logo.png"
                sourceSize.height:      70
                fillMode:               Image.PreserveAspectFit
                smooth:                 true
            }
            Text {
                Layout.alignment:   Qt.AlignHCenter
                Layout.topMargin:   6
                text:               "Daeyoung"
                color:              root.textWhite
                font.family:        "Arial"
                font.pixelSize:     28
                font.bold:          true
            }
            Text {
                Layout.alignment:   Qt.AlignHCenter
                text:               "Ground Control"
                color:              root.brandBlue
                font.family:        "Arial"
                font.pixelSize:     44
                font.bold:          true
            }
            Text {
                Layout.alignment:   Qt.AlignHCenter
                text:               "Ver. " + auth.productVersion
                color:              root.textWhite
                font.family:        "Arial"
                font.pixelSize:     18
            }

            Text {
                Layout.alignment:   Qt.AlignHCenter
                Layout.topMargin:   18
                visible:            auth.title.length > 0
                text:               auth.title
                color:              auth.stage === "fatal" ? root.errorRed : root.brandBlue
                font.family:        "Arial"
                font.pixelSize:     20
                font.bold:          true
            }
            Text {
                Layout.alignment:       Qt.AlignHCenter
                Layout.maximumWidth:    720
                Layout.topMargin:       auth.title.length > 0 ? 0 : 18
                visible:                auth.infoText.length > 0 && auth.stage !== "notice" && auth.stage !== "fatal"
                text:                   auth.infoText
                color:                  root.dimText
                font.family:            "Arial"
                font.pixelSize:         14
                wrapMode:               Text.WordWrap
                horizontalAlignment:    Text.AlignHCenter
            }

            Loader {
                id:                 stageLoader
                Layout.alignment:   Qt.AlignHCenter
                Layout.topMargin:   16
                sourceComponent: {
                    switch (auth.stage) {
                    case "setupAdmin":
                    case "setupUser":       return setupPage
                    case "recoveryCode":    return recoveryCodePage
                    case "login":           return loginPage
                    case "recovery":        return recoveryPage
                    case "changePassword":  return changePasswordPage
                    case "notice":          return noticePage
                    case "adminTools":      return adminToolsPage
                    case "fatal":           return fatalPage
                    default:                return null
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Stages

    Component {
        id: loginPage
        ColumnLayout {
            id: loginForm
            spacing: 8
            function submit() {
                auth.login(idField.text, pwField.text)
                pwField.text = ""
                if (auth.stage === "login") {
                    pwField.forceActiveFocus()
                }
            }
            RowLayout {
                spacing: 8
                ColumnLayout {
                    spacing: 8
                    KField {
                        id:                 idField
                        placeholderText:    "ID"
                        focus:              true
                        Component.onCompleted: forceActiveFocus()
                        Keys.onReturnPressed: pwField.forceActiveFocus()
                        Keys.onEnterPressed:  pwField.forceActiveFocus()
                        onTextChanged:      auth.clearMessage()
                    }
                    PwField {
                        id:                 pwField
                        placeholderText:    "PW"
                        Keys.onReturnPressed: loginForm.submit()
                        Keys.onEnterPressed:  loginForm.submit()
                    }
                }
                KButton {
                    text:                   "Login"
                    Layout.preferredWidth:  94
                    Layout.preferredHeight: root.fieldHeight * 2 + 8
                    onClicked:              loginForm.submit()
                }
            }
            ErrorList {}
            LinkButton {
                Layout.topMargin:   12
                text:               "Administrator recovery"
                onClicked:          auth.showRecovery()
            }
        }
    }

    Component {
        id: setupPage
        RowLayout {
            spacing: 32
            ColumnLayout {
                id: setupForm
                Layout.alignment: Qt.AlignTop
                spacing: 8
                function submit() {
                    if (auth.createAccount(idField.text, pwField.text, confirmField.text)) {
                        return
                    }
                    pwField.text = ""
                    confirmField.text = ""
                    pwField.forceActiveFocus()
                }
                RowLayout {
                    spacing: 8
                    ColumnLayout {
                        spacing: 8
                        KField {
                            id:                 idField
                            placeholderText:    "ID"
                            Component.onCompleted: forceActiveFocus()
                            Keys.onReturnPressed: pwField.forceActiveFocus()
                            Keys.onEnterPressed:  pwField.forceActiveFocus()
                        }
                        PwField {
                            id:                 pwField
                            placeholderText:    "PW"
                            Keys.onReturnPressed: confirmField.forceActiveFocus()
                            Keys.onEnterPressed:  confirmField.forceActiveFocus()
                        }
                        PwField {
                            id:                 confirmField
                            placeholderText:    "PW confirm"
                            Keys.onReturnPressed: setupForm.submit()
                            Keys.onEnterPressed:  setupForm.submit()
                        }
                    }
                    KButton {
                        text:                   auth.stage === "setupAdmin" ? "Create\nAdmin. Account" : "Create\nUser Account"
                        Layout.preferredWidth:  120
                        Layout.preferredHeight: root.fieldHeight * 2 + 8
                        Layout.alignment:       Qt.AlignTop
                        onClicked:              setupForm.submit()
                    }
                }
                ErrorList {}
            }
            RulesPanel { Layout.alignment: Qt.AlignTop }
        }
    }

    Component {
        id: recoveryCodePage
        ColumnLayout {
            spacing: 12
            BodyText {
                Layout.preferredWidth:  640
                horizontalAlignment:    Text.AlignHCenter
                text: "Write down this recovery code and keep it sealed with the security officer.\n" +
                      "It is shown only once. It lets an administrator set a new password when every administrator account is locked."
            }
            Rectangle {
                Layout.alignment:       Qt.AlignHCenter
                Layout.preferredWidth:  codeText.implicitWidth + 48
                Layout.preferredHeight: codeText.implicitHeight + 24
                color:                  "#0D0D0D"
                border.color:           root.brandBlue
                border.width:           2
                Text {
                    id:                 codeText
                    anchors.centerIn:   parent
                    text:               auth.recoveryCode
                    color:              root.brandBlue
                    font.family:        "Consolas"
                    font.pixelSize:     30
                    font.bold:          true
                    font.letterSpacing: 2
                }
            }
            CheckBox {
                id:                 recordedCheck
                Layout.alignment:   Qt.AlignHCenter
                text:               "I have recorded the recovery code."
                font.family:        "Arial"
                font.pixelSize:     15
                contentItem: Text {
                    leftPadding:        recordedCheck.indicator.width + 8
                    text:               recordedCheck.text
                    font:               recordedCheck.font
                    color:              root.textWhite
                    verticalAlignment:  Text.AlignVCenter
                }
            }
            KButton {
                Layout.alignment:   Qt.AlignHCenter
                text:               "Continue"
                enabled:            recordedCheck.checked
                onClicked:          auth.acknowledgeRecoveryCode()
            }
        }
    }

    Component {
        id: recoveryPage
        RowLayout {
            spacing: 32
            ColumnLayout {
                id: recoveryForm
                Layout.alignment: Qt.AlignTop
                spacing: 8
                function submit() {
                    auth.recoverAdmin(codeField.text, idField.text, pwField.text, confirmField.text)
                    pwField.text = ""
                    confirmField.text = ""
                }
                KField {
                    id:                 codeField
                    placeholderText:    "Recovery code"
                    Layout.preferredWidth: root.fieldWidth + 128
                    font.capitalization: Font.AllUppercase
                    Component.onCompleted: forceActiveFocus()
                }
                KField { id: idField; placeholderText: "Administrator ID"; Layout.preferredWidth: root.fieldWidth + 128 }
                PwField { id: pwField; placeholderText: "New PW"; Layout.preferredWidth: root.fieldWidth + 128 }
                PwField {
                    id:                 confirmField
                    placeholderText:    "New PW confirm"
                    Layout.preferredWidth: root.fieldWidth + 128
                    Keys.onReturnPressed: recoveryForm.submit()
                    Keys.onEnterPressed:  recoveryForm.submit()
                }
                ErrorList {}
                RowLayout {
                    spacing: 12
                    KButton { text: "Recover"; onClicked: recoveryForm.submit() }
                    KButton { text: "Back"; primary: false; onClicked: auth.backToLogin() }
                }
            }
            RulesPanel { Layout.alignment: Qt.AlignTop }
        }
    }

    Component {
        id: changePasswordPage
        ColumnLayout {
            spacing: 12
            BodyText {
                Layout.alignment:       Qt.AlignHCenter
                Layout.preferredWidth:  720
                horizontalAlignment:    Text.AlignHCenter
                text:                   auth.infoText
            }
            RowLayout {
                spacing: 32
                ColumnLayout {
                    id: changeForm
                    Layout.alignment: Qt.AlignTop
                    spacing: 8
                    function submit() {
                        auth.changePassword(currentField.text, pwField.text, confirmField.text)
                        currentField.text = ""
                        pwField.text = ""
                        confirmField.text = ""
                    }
                    PwField { id: currentField; placeholderText: "Current PW"; Component.onCompleted: forceActiveFocus() }
                    PwField { id: pwField; placeholderText: "New PW" }
                    PwField {
                        id:                 confirmField
                        placeholderText:    "New PW confirm"
                        Keys.onReturnPressed: changeForm.submit()
                        Keys.onEnterPressed:  changeForm.submit()
                    }
                    ErrorList {}
                    RowLayout {
                        spacing: 12
                        KButton { text: "Change PW"; onClicked: changeForm.submit() }
                        KButton { text: "Exit"; primary: false; onClicked: auth.cancel() }
                    }
                }
                RulesPanel { Layout.alignment: Qt.AlignTop }
            }
        }
    }

    Component {
        id: noticePage
        ColumnLayout {
            spacing: 18
            BodyText {
                Layout.alignment:       Qt.AlignHCenter
                Layout.preferredWidth:  720
                horizontalAlignment:    Text.AlignHCenter
                lineHeight:             1.3
                text:                   auth.infoText
            }
            RowLayout {
                Layout.alignment:   Qt.AlignHCenter
                spacing:            12
                KButton {
                    text:           "Continue"
                    focus:          true
                    Component.onCompleted: forceActiveFocus()
                    Keys.onReturnPressed: auth.continueToApplication()
                    Keys.onEnterPressed:  auth.continueToApplication()
                    onClicked:      auth.continueToApplication()
                }
                KButton {
                    visible:        auth.isAdmin
                    primary:        false
                    text:           "Account Management"
                    onClicked:      auth.showAdminTools()
                }
                KButton {
                    primary:        false
                    text:           "Change My PW"
                    onClicked:      auth.requestPasswordChange()
                }
            }
        }
    }

    Component {
        id: adminToolsPage
        ColumnLayout {
            id:         adminTools
            spacing:    14
            property string resetTarget: ""
            property var auditModel: auth.auditEntries(300)

            function refreshAudit() { auditModel = auth.auditEntries(300) }

            // Accounts
            Text {
                text: "Accounts"
                color: root.textWhite; font.family: "Arial"; font.pixelSize: 17; font.bold: true
            }
            RowLayout {
                spacing: 24
                Repeater {
                    model: [["ID", 140], ["Role", 110], ["Status", 220], ["Last login", 170]]
                    Text {
                        Layout.preferredWidth: modelData[1]
                        text: modelData[0]
                        color: root.dimText; font.family: "Arial"; font.pixelSize: 13; font.bold: true
                    }
                }
            }
            Repeater {
                model: auth.accounts
                RowLayout {
                    spacing: 24
                    Text { Layout.preferredWidth: 140; text: modelData.id + (modelData.isSelf ? "  (you)" : ""); color: root.textWhite; font.family: "Arial"; font.pixelSize: 15 }
                    Text { Layout.preferredWidth: 110; text: modelData.role; color: root.textWhite; font.family: "Arial"; font.pixelSize: 15 }
                    Text {
                        Layout.preferredWidth: 220
                        text: modelData.locked ? "Locked (" + modelData.lockReason + ")" : (modelData.mustChange ? "PW change pending" : "Active")
                        color: modelData.locked ? root.errorRed : root.textWhite
                        font.family: "Arial"; font.pixelSize: 15
                    }
                    Text { Layout.preferredWidth: 170; text: modelData.lastLogin; color: root.textWhite; font.family: "Arial"; font.pixelSize: 15 }
                    KButton {
                        enabled:    modelData.locked
                        opacity:    modelData.locked ? 1 : 0
                        text:       "Unlock"
                        implicitHeight: 28
                        onClicked:  { auth.unlockAccount(modelData.id); adminTools.refreshAudit() }
                    }
                    KButton {
                        visible:    !modelData.isSelf
                        primary:    false
                        text:       "Reset PW"
                        implicitHeight: 28
                        onClicked:  adminTools.resetTarget = modelData.id
                    }
                }
            }

            // Temporary password entry for the selected account
            ColumnLayout {
                visible:    adminTools.resetTarget.length > 0
                spacing:    8
                BodyText { text: "Set a temporary PW for '" + adminTools.resetTarget + "'. The user must change it at the next login." }
                RowLayout {
                    spacing: 8
                    PwField { id: resetPw; placeholderText: "Temporary PW" }
                    PwField { id: resetConfirm; placeholderText: "Temporary PW confirm" }
                    KButton {
                        text: "Set"
                        onClicked: {
                            if (auth.resetPassword(adminTools.resetTarget, resetPw.text, resetConfirm.text)) {
                                adminTools.resetTarget = ""
                                adminTools.refreshAudit()
                            }
                            resetPw.text = ""
                            resetConfirm.text = ""
                        }
                    }
                    KButton { text: "Cancel"; primary: false; onClicked: { adminTools.resetTarget = ""; auth.clearMessage() } }
                }
            }
            ErrorList {}

            // Audit log (SR-05)
            RowLayout {
                Layout.topMargin: 10
                spacing: 16
                Text { text: "Audit log"; color: root.textWhite; font.family: "Arial"; font.pixelSize: 17; font.bold: true }
                Text {
                    text: auth.auditIntegrityText()
                    color: text.indexOf("FAILED") >= 0 ? root.errorRed : "#39FF14"
                    font.family: "Arial"; font.pixelSize: 13
                }
            }
            Rectangle {
                Layout.preferredWidth:  980
                Layout.preferredHeight: 260
                color:                  "#0D0D0D"
                border.color:           "#404040"
                ListView {
                    id:                 auditList
                    anchors.fill:       parent
                    anchors.margins:    8
                    clip:               true
                    model:              adminTools.auditModel
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: Text {
                        width:          auditList.width - 12
                        text:           modelData.time + "   " + modelData.type + "   " + (modelData.account || "-") +
                                        "   " + modelData.result + (modelData.detail ? "   " + modelData.detail : "")
                        color:          modelData.type.indexOf("FAIL") >= 0 || modelData.type.indexOf("LOCK") >= 0 ||
                                        modelData.type.indexOf("INTEGRITY") >= 0 ? "#FFA500" : root.textWhite
                        font.family:    "Consolas"
                        font.pixelSize: 13
                        elide:          Text.ElideRight
                    }
                }
            }
            RowLayout {
                spacing: 12
                KButton { text: "Export CSV"; primary: false; onClicked: exportDialog.open() }
                KButton { text: "Back"; onClicked: auth.closeAdminTools() }
            }

            FileDialog {
                id:             exportDialog
                title:          "Export audit log"
                fileMode:       FileDialog.SaveFile
                nameFilters:    ["CSV files (*.csv)"]
                defaultSuffix:  "csv"
                onAccepted:     { auth.exportAuditLog(selectedFile); adminTools.refreshAudit() }
            }
        }
    }

    Component {
        id: fatalPage
        ColumnLayout {
            spacing: 18
            BodyText {
                Layout.alignment:       Qt.AlignHCenter
                Layout.preferredWidth:  640
                horizontalAlignment:    Text.AlignHCenter
                text:                   auth.infoText
            }
            KButton {
                Layout.alignment:   Qt.AlignHCenter
                text:               "Exit"
                onClicked:          auth.cancel()
            }
        }
    }
}
