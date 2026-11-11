/****************************************************************************
 *
 * Daeyoung Ground Control - role based access policy (SR-15, MIL edition)
 *
 ****************************************************************************/

#include "AccessPolicy.h"

#include <QtCore/QSet>

namespace KevaSecurity {

namespace {

// Numeric MAV_CMD values (MAVLink common.xml). CustomPlugin.cc static_asserts them
// against the MAVLink headers so a renumbering would fail the build.
const QSet<int> kFlightCommands = {
    17,     // MAV_CMD_NAV_LOITER_UNLIM
    20,     // MAV_CMD_NAV_RETURN_TO_LAUNCH
    21,     // MAV_CMD_NAV_LAND
    22,     // MAV_CMD_NAV_TAKEOFF
    34,     // MAV_CMD_DO_ORBIT
    84,     // MAV_CMD_NAV_VTOL_TAKEOFF
    85,     // MAV_CMD_NAV_VTOL_LAND
    92,     // MAV_CMD_NAV_GUIDED_ENABLE
    115,    // MAV_CMD_CONDITION_YAW
    176,    // MAV_CMD_DO_SET_MODE
    178,    // MAV_CMD_DO_CHANGE_SPEED
    179,    // MAV_CMD_DO_SET_HOME
    185,    // MAV_CMD_DO_FLIGHTTERMINATION
    186,    // MAV_CMD_DO_CHANGE_ALTITUDE
    189,    // MAV_CMD_DO_LAND_START
    191,    // MAV_CMD_DO_GO_AROUND
    192,    // MAV_CMD_DO_REPOSITION
    193,    // MAV_CMD_DO_PAUSE_CONTINUE
    195,    // MAV_CMD_DO_SET_ROI_LOCATION
    196,    // MAV_CMD_DO_SET_ROI_WPNEXT_OFFSET
    197,    // MAV_CMD_DO_SET_ROI_NONE
    201,    // MAV_CMD_DO_SET_ROI
    224,    // MAV_CMD_DO_SET_MISSION_CURRENT
    300,    // MAV_CMD_MISSION_START
    400,    // MAV_CMD_COMPONENT_ARM_DISARM
    3000,   // MAV_CMD_DO_VTOL_TRANSITION
};

// Operational pages a user may open. Everything else is administrator only.
const QStringList kUserSettingsPages = {
    QStringLiteral("GeneralSettings.qml"),
    QStringLiteral("FlyViewSettings.qml"),
    QStringLiteral("PlanViewSettings.qml"),
    QStringLiteral("VideoSettings.qml"),
    QStringLiteral("LinkSettings.qml"),
    QStringLiteral("MapSettings.qml"),
    QStringLiteral("RemoteIDSettings.qml"),
    QStringLiteral("HelpSettings.qml"),
};

} // namespace

bool AccessPolicy::isFlightCommand(int mavCmd)
{
    return kFlightCommands.contains(mavCmd);
}

QStringList AccessPolicy::userSettingsPages()
{
    return kUserSettingsPages;
}

bool AccessPolicy::settingsPageAllowed(Role role, const QString &pageUrl)
{
    if (role == Role::Administrator) {
        return true;
    }
    const QString fileName = pageUrl.section(QLatin1Char('/'), -1);
    return kUserSettingsPages.contains(fileName);
}

} // namespace KevaSecurity
