/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 * @file
 *   @brief Custom Firmware Plugin (PX4)
 *   @author Gus Grubba <gus@auterion.com>
 *
 */

#include "CustomFirmwarePlugin.h"
#include "CustomAutoPilotPlugin.h"
#include "Vehicle.h"
#include "px4_custom_mode.h"

//-----------------------------------------------------------------------------
namespace {

// Modes the operator may select (UI deck slide 8, decided 2026-11-09):
// Position, Stabilized, Altitude, Offboard, Mission, Safe Recovery (PX4 Return),
// Takeoff, Land, Hold. Every other mode is only displayed when the vehicle reports it.
bool kevaModeSettable(uint32_t customMode)
{
    switch (static_cast<PX4CustomMode::Mode>(customMode)) {
    case PX4CustomMode::POSCTL_POSCTL:
    case PX4CustomMode::STABILIZED:
    case PX4CustomMode::ALTCTL:
    case PX4CustomMode::OFFBOARD:
    case PX4CustomMode::AUTO_MISSION:
    case PX4CustomMode::AUTO_RTL:
    case PX4CustomMode::AUTO_TAKEOFF:
    case PX4CustomMode::AUTO_LAND:
    case PX4CustomMode::AUTO_LOITER:
        return true;
    default:
        return false;
    }
}

} // namespace

CustomFirmwarePlugin::CustomFirmwarePlugin()
{
    for (auto &mode: _flightModeList) {
        mode.canBeSet = kevaModeSettable(mode.custom_mode);
    }
}

//-----------------------------------------------------------------------------
AutoPilotPlugin* CustomFirmwarePlugin::autopilotPlugin(Vehicle* vehicle) const
{
    return new CustomAutoPilotPlugin(vehicle, vehicle);
}

// KEVA toolbar (UI deck slide 8): flight mode, MR/FW, GPS, batteries.
// RC RSSI, telemetry RSSI, Remote ID and gimbal indicators are not shown.
const QVariantList& CustomFirmwarePlugin::toolIndicators(const Vehicle* vehicle)
{
    Q_UNUSED(vehicle)
    if (_toolIndicatorList.isEmpty()) {
        _toolIndicatorList = QVariantList({
            QVariant::fromValue(QUrl::fromUserInput("qrc:/Custom/qml/KevaToolbar/KevaFlightModeIndicator.qml")),
            QVariant::fromValue(QUrl::fromUserInput("qrc:/Custom/qml/KevaToolbar/KevaTransitionIndicator.qml")),
            QVariant::fromValue(QUrl::fromUserInput("qrc:/Custom/qml/KevaToolbar/KevaGPSIndicator.qml")),
            QVariant::fromValue(QUrl::fromUserInput("qrc:/Custom/qml/KevaToolbar/KevaBatteryIndicator.qml")),
        });
    }
    return _toolIndicatorList;
}

// Vehicle selector when more than one vehicle is connected (as in QGC)
const QVariantList& CustomFirmwarePlugin::modeIndicators(const Vehicle* vehicle)
{
    Q_UNUSED(vehicle)
    if (_modeIndicatorList.isEmpty()) {
        _modeIndicatorList = QVariantList({
            QVariant::fromValue(QUrl::fromUserInput("qrc:/qml/QGroundControl/Toolbar/MultiVehicleSelector.qml")),
        });
    }
    return _modeIndicatorList;
}

// Tells QGC that your vehicle has a gimbal on it. This will in turn cause thing like gimbal commands to point
// the camera straight down for surveys to be automatically added to Plans.
bool CustomFirmwarePlugin::hasGimbal(Vehicle* /*vehicle*/, bool& rollSupported, bool& pitchSupported, bool& yawSupported) const
{
    rollSupported = false;
    pitchSupported = true;
    yawSupported = true;

    return true;
}

void CustomFirmwarePlugin::updateAvailableFlightModes(FlightModeList &modeList)
{

    for(auto &mode: modeList){
        PX4CustomMode::Mode cMode = static_cast<PX4CustomMode::Mode>(mode.custom_mode);
        // Update Multi Rotor
        switch (cMode) {
        case PX4CustomMode::MANUAL            :
        case PX4CustomMode::STABILIZED        :
        case PX4CustomMode::ACRO              :
        case PX4CustomMode::RATTITUDE         :
        case PX4CustomMode::ALTCTL            :
        case PX4CustomMode::OFFBOARD          :
        case PX4CustomMode::SIMPLE            :
        case PX4CustomMode::POSCTL_POSCTL     :
        case PX4CustomMode::AUTO_LOITER       :
        case PX4CustomMode::AUTO_MISSION      :
        case PX4CustomMode::AUTO_RTL          :
        case PX4CustomMode::AUTO_FOLLOW_TARGET:
        case PX4CustomMode::AUTO_LAND         :
        case PX4CustomMode::AUTO_PRECLAND     :
        case PX4CustomMode::AUTO_READY        :
        case PX4CustomMode::AUTO_RTGS         :
        case PX4CustomMode::AUTO_TAKEOFF      :
            mode.multiRotor = true;
            break;
        case PX4CustomMode::POSCTL_ORBIT      :
            mode.multiRotor = false;
            break;
        }

        // Update Fixed Wing
        switch (cMode){
        case PX4CustomMode::OFFBOARD          :
        case PX4CustomMode::SIMPLE            :
        case PX4CustomMode::POSCTL_ORBIT      :
        case PX4CustomMode::AUTO_FOLLOW_TARGET:
        case PX4CustomMode::AUTO_PRECLAND     :
            mode.fixedWing = false;
            break;
        case PX4CustomMode::MANUAL            :
        case PX4CustomMode::STABILIZED        :
        case PX4CustomMode::ACRO              :
        case PX4CustomMode::RATTITUDE         :
        case PX4CustomMode::ALTCTL            :
        case PX4CustomMode::POSCTL_POSCTL     :
        case PX4CustomMode::AUTO_LOITER       :
        case PX4CustomMode::AUTO_MISSION      :
        case PX4CustomMode::AUTO_RTL          :
        case PX4CustomMode::AUTO_LAND         :
        case PX4CustomMode::AUTO_READY        :
        case PX4CustomMode::AUTO_RTGS         :
        case PX4CustomMode::AUTO_TAKEOFF      :
            mode.fixedWing = true;
            break;
        }

        // Update CanBeSet
        mode.canBeSet = kevaModeSettable(mode.custom_mode);
    }
    _updateFlightModeList(modeList);
}
