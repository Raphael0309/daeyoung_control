/****************************************************************************
 *
 * Daeyoung Ground Control - role based access policy (SR-15, MIL edition)
 *
 * Decided 2026-11-09:
 *   Administrator : monitors flights only (no flight commands, no joystick),
 *                   no Plan Flight, full Vehicle Configuration and settings.
 *   User          : flies; no Vehicle Configuration; operational settings only.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace KevaSecurity {

class AccessPolicy
{
public:
    enum class Role { Administrator, User };

    static bool canSendFlightCommands(Role role)    { return role == Role::User; }
    static bool canUseManualControl(Role role)      { return role == Role::User; }
    static bool canPlanFlights(Role role)           { return role == Role::User; }
    static bool canConfigureVehicle(Role role)      { return role == Role::Administrator; }

    /// MAV_CMD ids that move or arm the vehicle or change how it flies.
    /// Payload (camera, gimbal), calibration, reboot and parameter traffic are not included.
    static bool isFlightCommand(int mavCmd);

    /// Settings pages are identified by their QML file name (e.g. "MapSettings.qml").
    static bool settingsPageAllowed(Role role, const QString &pageUrl);
    static QStringList userSettingsPages();
};

} // namespace KevaSecurity
