/****************************************************************************
 *
 * Daeyoung Ground Control - Fly view tool strip (UI deck slide 6)
 *
 * Takeoff, Return, Pause and Resume Mission are kept; every other command
 * goes through the flight mode menu or the Status Data panel. The pre-flight
 * checklist button appears only when enabled in the settings (as in QGC).
 * All buttons are hidden for administrator accounts (guided actions disabled).
 *
 ****************************************************************************/

import QtQml.Models

import QGroundControl
import QGroundControl.Controls

ToolStripActionList {
    id: _root

    signal displayPreFlightChecklist

    model: [
        PreFlightCheckListShowAction { onTriggered: displayPreFlightChecklist() },
        GuidedActionTakeoff { },
        GuidedActionRTL { },
        GuidedActionPause { },
        // Resume Mission: continues a paused mission in flight; on the ground it starts the mission
        GuidedToolStripAction {
            property bool _canStart:    _guidedController.showStartMission
            text:                       _canStart ? _guidedController.startMissionTitle : qsTr("Resume Mission")
            iconSource:                 "/res/Play.svg"
            visible:                    true
            enabled:                    _guidedController.showContinueMission || _canStart
            actionID:                   _canStart ? _guidedController.actionStartMission : _guidedController.actionContinueMission
        }
    ]
}
