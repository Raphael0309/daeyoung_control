/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Custom QGCCorePlugin Implementation
 *   @author Gus Grubba <gus@auterion.com>
 */

#include "CustomPlugin.h"
#include "QmlComponentInfo.h"
#include "QGCLoggingCategory.h"
#include "QGCPalette.h"
#include "KevaTheme.h"

#include <QtCore/QHash>
#include <QtCore/QVariantMap>
#include "QGCMAVLink.h"
#include "AppSettings.h"
#include "BrandImageSettings.h"
#include "UnitsSettings.h"
#include "FlightMapSettings.h"
#include "VideoSettings.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "QGCGeo.h"
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <cmath>

namespace {
// Status Data panel Thr% (UI deck slide 11): main outputs of SERVO_OUTPUT_RAW (port 0).
// KEVA_7 standard VTOL layout: multicopter motors on outputs 1-4, pusher (FW) motor on 5.
// Change here if the airframe's actuator mapping differs.
constexpr int    kKevaFwOutput          = 5;
constexpr int    kKevaMcOutputs[4]      = { 1, 2, 3, 4 };
constexpr double kKevaPwmMin            = 1000.0;   // 0 %
constexpr double kKevaPwmMax            = 2000.0;   // 100 %
constexpr qint64 kKevaMotorStaleMs      = 3000;

// Status Data panel target dials (UI deck slide 11, review decisions)
constexpr double kKevaAltMin            = 10.0;     // m, relative to home
constexpr double kKevaAltMax            = 3000.0;
constexpr double kKevaAltStep           = 10.0;
constexpr double kKevaAirspeedStep      = 1.0;      // m/s
}

#ifdef KEVA_EDITION_MIL
#include "AccessPolicy.h"
#include "IdleLockController.h"
#include "LoginGate.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QmlObjectListModel.h"
#include "SecurityConfig.h"
#include "Vehicle.h"
#include <cstdlib>

// AccessPolicy uses plain numbers so it can be unit tested without MAVLink; make sure they still match.
static_assert(MAV_CMD_NAV_LOITER_UNLIM == 17 && MAV_CMD_NAV_RETURN_TO_LAUNCH == 20 && MAV_CMD_NAV_LAND == 21 &&
              MAV_CMD_NAV_TAKEOFF == 22 && MAV_CMD_DO_ORBIT == 34 && MAV_CMD_NAV_VTOL_TAKEOFF == 84 &&
              MAV_CMD_NAV_VTOL_LAND == 85 && MAV_CMD_NAV_GUIDED_ENABLE == 92 && MAV_CMD_CONDITION_YAW == 115 &&
              MAV_CMD_DO_SET_MODE == 176 && MAV_CMD_DO_CHANGE_SPEED == 178 && MAV_CMD_DO_SET_HOME == 179 &&
              MAV_CMD_DO_FLIGHTTERMINATION == 185 && MAV_CMD_DO_CHANGE_ALTITUDE == 186 && MAV_CMD_DO_LAND_START == 189 &&
              MAV_CMD_DO_GO_AROUND == 191 && MAV_CMD_DO_REPOSITION == 192 && MAV_CMD_DO_PAUSE_CONTINUE == 193 &&
              MAV_CMD_DO_SET_ROI_LOCATION == 195 && MAV_CMD_DO_SET_ROI_WPNEXT_OFFSET == 196 && MAV_CMD_DO_SET_ROI_NONE == 197 &&
              MAV_CMD_DO_SET_ROI == 201 && MAV_CMD_DO_SET_MISSION_CURRENT == 224 && MAV_CMD_MISSION_START == 300 &&
              MAV_CMD_COMPONENT_ARM_DISARM == 400 && MAV_CMD_DO_VTOL_TRANSITION == 3000,
              "AccessPolicy flight command ids no longer match the MAVLink headers");
#endif

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtQuick/QQuickWindow>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QtCore/QApplicationStatic>
#endif
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlFile>

QGC_LOGGING_CATEGORY(CustomLog, "gcs.custom.customplugin")

Q_APPLICATION_STATIC(CustomPlugin, _customPluginInstance);

CustomFlyViewOptions::CustomFlyViewOptions(CustomOptions* options, QObject* parent)
    : QGCFlyViewOptions(options, parent)
{

}

// This custom build does not support conecting multiple vehicles to it. This in turn simplifies various parts of the QGC ui.
bool CustomFlyViewOptions::showMultiVehicleList(void) const
{
    return false;
}

// This custom build has it's own custom instrument panel. Don't show regular one.
bool CustomFlyViewOptions::showInstrumentPanel(void) const
{
    return false;
}

CustomOptions::CustomOptions(CustomPlugin *plugin, QObject *parent)
    : QGCOptions(parent)
    , _plugin(plugin)
    , _flyViewOptions(new CustomFlyViewOptions(this, this))
{
    Q_CHECK_PTR(_plugin);
}

QGCFlyViewOptions* CustomOptions::flyViewOptions(void) const
{
    return _flyViewOptions;
}

// Firmware upgrade page is only shown in Advanced Mode.
bool CustomOptions::showFirmwareUpgrade() const
{
    return _plugin->showAdvancedUI();
}

// Normal QGC needs to work with an ESP8266 WiFi thing which is remarkably crappy. This in turns causes PX4 Pro calibration to fail
// quite often. There is a warning in regular QGC about this. Overriding the and returning true means that your custom vehicle has
// a reliable WiFi connection so don't show that warning.
bool CustomOptions::wifiReliableForCalibration(void) const
{
    return true;
}

/*===========================================================================*/

CustomPlugin::CustomPlugin(QObject *parent)
    : QGCCorePlugin(parent)
    , _options(new CustomOptions(this, this))
{
    _showAdvancedUI = true;
    connect(this, &QGCCorePlugin::showAdvancedUIChanged, this, &CustomPlugin::_advancedChanged);
}

CustomPlugin::~CustomPlugin()
{

}

QGCCorePlugin *CustomPlugin::instance()
{
    return _customPluginInstance();
}

void CustomPlugin::init()
{
    // Motor outputs for the Status Data panel go blank when the data stops or the vehicle changes
    QTimer *motorTimer = new QTimer(this);
    motorTimer->setInterval(1000);
    connect(motorTimer, &QTimer::timeout, this, &CustomPlugin::_checkMotorOutputs);
    motorTimer->start();

#ifdef KEVA_DISPLAY_NAME
    // Window titles and dialogs show the product name (QGC_APP_NAME has no spaces).
    QGuiApplication::setApplicationDisplayName(QStringLiteral(KEVA_DISPLAY_NAME));
#endif

#ifdef KEVA_EDITION_MIL
    // Secured edition: authenticate before QGC creates its main window and before
    // any MAVLink link, video stream or map is started (SR-03 ~ SR-15).
    _auth = new KevaSecurity::AuthManager(KevaSecurity::LoginGate::defaultOptions(), this);
    if (!KevaSecurity::LoginGate::run(_auth)) {
        qCInfo(CustomLog) << "Login not completed - exiting";
        // QGC has no clean abort path at this point of start-up (GStreamer is already
        // initialised). Audit records are flushed on every write, so leave immediately.
        std::_Exit(0);
    }
    qCInfo(CustomLog) << "Logged in as" << _auth->currentUser() << (_auth->isAdmin() ? "(administrator)" : "(user)");

    // Administrators monitor only: no flight commands, no joystick (SR-15)
    _vehicleControlRestricted = _auth->isAdmin();
    _lastDeniedNotice.invalidate();

    // Idle lock: 10 minutes without input, never while a vehicle is armed
    KevaSecurity::IdleLockController::Hooks hooks;
    hooks.lockSuppressed = []() {
        const QmlObjectListModel *vehicles = MultiVehicleManager::instance()->vehicles();
        for (int i = 0; i < vehicles->count(); ++i) {
            const Vehicle *vehicle = vehicles->value<Vehicle *>(i);
            if (vehicle && vehicle->armed()) {
                return true;
            }
        }
        return false;
    };
    hooks.referenceWindow = []() -> QWindow * { return qgcApp()->mainRootWindow(); };
    _idleLock = new KevaSecurity::IdleLockController(_auth, static_cast<qint64>(KevaSecurity::kIdleLockMinutes) * 60 * 1000, hooks, this);
    connect(_auth, &KevaSecurity::AuthManager::exitRequested, qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    _idleLock->start();
#endif
}

// ---------------------------------------------------------------------------
// Role based access (SR-15)

bool CustomPlugin::vehicleControlAllowed(int command)
{
#ifdef KEVA_EDITION_MIL
    if (!_vehicleControlRestricted.load()) {
        return true;
    }
    if (command == kVehicleControlManual) {
        return false;   // joystick thread: no UI work here
    }
    if (!KevaSecurity::AccessPolicy::isFlightCommand(command)) {
        return true;
    }
    if (!_lastDeniedNotice.isValid() || _lastDeniedNotice.elapsed() > 3000) {
        _lastDeniedNotice.start();
        qgcApp()->showAppMessage(tr("Administrator accounts cannot send flight commands."));
        if (_auth) {
            _auth->recordEvent(QStringLiteral("COMMAND_BLOCKED"), QStringLiteral("denied"), QStringLiteral("MAV_CMD %1").arg(command));
        }
    }
    return false;
#else
    Q_UNUSED(command)
    return true;
#endif
}

bool CustomPlugin::kevaSecured() const
{
#ifdef KEVA_EDITION_MIL
    return true;
#else
    return false;
#endif
}

bool CustomPlugin::kevaIsAdmin() const
{
#ifdef KEVA_EDITION_MIL
    return _auth && _auth->isAdmin();
#else
    return false;
#endif
}

QString CustomPlugin::kevaUserId() const
{
#ifdef KEVA_EDITION_MIL
    return _auth ? _auth->currentUser() : QString();
#else
    return QString();
#endif
}

bool CustomPlugin::kevaCanControlVehicle() const
{
#ifdef KEVA_EDITION_MIL
    return !kevaIsAdmin();
#else
    return true;
#endif
}

bool CustomPlugin::kevaCanPlan() const
{
#ifdef KEVA_EDITION_MIL
    return !kevaIsAdmin();
#else
    return true;
#endif
}

bool CustomPlugin::kevaCanConfigureVehicle() const
{
#ifdef KEVA_EDITION_MIL
    return kevaIsAdmin();
#else
    return true;
#endif
}

QVariantMap CustomPlugin::kevaColors() const
{
    using namespace KevaTheme;
    return {
        { QStringLiteral("deepSkyBlue"),    DeepSkyBlue },
        { QStringLiteral("neonGreen"),      NeonGreen },
        { QStringLiteral("yellow"),         Yellow },
        { QStringLiteral("orange"),         Orange },
        { QStringLiteral("red"),            Red },
        { QStringLiteral("white"),          White },
        { QStringLiteral("brown"),          Brown },
        { QStringLiteral("black"),          Black },
        { QStringLiteral("panel"),          Panel },
        { QStringLiteral("border"),         Border },
    };
}

bool CustomPlugin::kevaSettingsPageAllowed(const QString &pageUrl) const
{
#ifdef KEVA_EDITION_MIL
    using KevaSecurity::AccessPolicy;
    return AccessPolicy::settingsPageAllowed(kevaIsAdmin() ? AccessPolicy::Role::Administrator : AccessPolicy::Role::User, pageUrl);
#else
    Q_UNUSED(pageUrl)
    return true;
#endif
}

// No first run prompts: units are fixed to metric and the vehicle type to PX4 VTOL.
QList<int> CustomPlugin::firstRunPromptStdIds()
{
    return QList<int>();
}

void CustomPlugin::cleanup()
{
#ifdef KEVA_EDITION_MIL
    if (_auth) {
        _auth->recordLogout();
    }
#endif

    if (_qmlEngine) {
        _qmlEngine->removeUrlInterceptor(_selector);
    }

    delete _selector;
}

void CustomPlugin::_advancedChanged(bool changed)
{
    // Firmware Upgrade page is only show in Advanced mode
    emit _options->showFirmwareUpgradeChanged(changed);
}

void CustomPlugin::_addSettingsEntry(const QString& title, const char* qmlFile, const char* iconFile)
{
    Q_CHECK_PTR(qmlFile);
    // 'this' instance will take ownership on the QmlComponentInfo instance
    _customSettingsList.append(QVariant::fromValue(
        new QmlComponentInfo(title,
                QUrl::fromUserInput(qmlFile),
                iconFile == nullptr ? QUrl() : QUrl::fromUserInput(iconFile),
                this)));
}

QGCOptions* CustomPlugin::options()
{
    return _options;
}

QString CustomPlugin::brandImageIndoor(void) const
{
    return QStringLiteral("/custom/img/dronecode-white.svg");
}

QString CustomPlugin::brandImageOutdoor(void) const
{
    return QStringLiteral("/custom/img/dronecode-black.svg");
}

bool CustomPlugin::overrideSettingsGroupVisibility(const QString &name)
{
    // Units are fixed to metric (decided 2026-11-09); hide them from the settings pages.
    if (name == QLatin1String(UnitsSettings::name)) {
        return false;
    }
    return true;
}

// This allows you to override/hide QGC Application settings
bool CustomPlugin::adjustSettingMetaData(const QString& settingsGroup, FactMetaData& metaData)
{
    bool parentResult = QGCCorePlugin::adjustSettingMetaData(settingsGroup, metaData);

    if (settingsGroup == AppSettings::settingsGroup) {
        // This tells QGC than when you are creating Plans while not connected to a vehicle
        // the specific firmware/vehicle the plan is for.
        if (metaData.name() == AppSettings::offlineEditingFirmwareClassName) {
            metaData.setRawDefaultValue(QGCMAVLink::FirmwareClassPX4);
            return false;
        } else if (metaData.name() == AppSettings::qLocaleLanguageName) {
            // UI language is fixed to English (KEVA_FORCE_ENGLISH)
            return false;
        } else if (metaData.name() == AppSettings::indoorPaletteName) {
            // Black KEVA UI only (the palette is the same in both themes, this just hides the choice)
            metaData.setRawDefaultValue(1);
            return false;
        } else if (metaData.name() == AppSettings::offlineEditingVehicleClassName) {
            // KEVA airframes are PX4 VTOL
            metaData.setRawDefaultValue(QGCMAVLink::VehicleClassVTOL);
            return false;
        }
    }

    // Default map: Google Hybrid (UI deck slide 5). The operator can still pick another map in the settings.
    if (settingsGroup == QLatin1String(FlightMapSettings::settingsGroup)) {
        if (metaData.name() == QLatin1String(FlightMapSettings::mapProviderName)) {
            metaData.setRawDefaultValue(QStringLiteral("Google"));
        } else if (metaData.name() == QLatin1String(FlightMapSettings::mapTypeName)) {
            metaData.setRawDefaultValue(QStringLiteral("Hybrid"));
        }
        return parentResult;
    }

    // Video: no frames for about 5 s -> "Video Transmission Disconnected" (UI deck slide 9).
    // QGC stops the stream after the timeout (UDP: fixed 3 s); RTSP uses this setting.
    if (settingsGroup == QLatin1String(VideoSettings::settingsGroup)) {
        if (metaData.name() == QLatin1String(VideoSettings::rtspTimeoutName)) {
            metaData.setRawDefaultValue(4);
        } else if (metaData.name() == QLatin1String(VideoSettings::videoFitName)) {
            // Whole picture, never cropped (black bars where the aspect ratio differs)
            metaData.setRawDefaultValue(3);     // "No Crop"
        }
        return parentResult;
    }

    // Metric units only. Returning false hides the setting and forces the default value.
    if (settingsGroup == QLatin1String(UnitsSettings::settingsGroup)) {
        const QString name = metaData.name();
        if (name == QLatin1String(UnitsSettings::horizontalDistanceUnitsName)) {
            metaData.setRawDefaultValue(static_cast<uint32_t>(UnitsSettings::HorizontalDistanceUnitsMeters));
        } else if (name == QLatin1String(UnitsSettings::verticalDistanceUnitsName)) {
            metaData.setRawDefaultValue(static_cast<uint32_t>(UnitsSettings::VerticalDistanceUnitsMeters));
        } else if (name == QLatin1String(UnitsSettings::areaUnitsName)) {
            metaData.setRawDefaultValue(static_cast<uint32_t>(UnitsSettings::AreaUnitsSquareMeters));
        } else if (name == QLatin1String(UnitsSettings::speedUnitsName)) {
            metaData.setRawDefaultValue(static_cast<uint32_t>(UnitsSettings::SpeedUnitsMetersPerSecond));
        } else if (name == QLatin1String(UnitsSettings::temperatureUnitsName)) {
            metaData.setRawDefaultValue(static_cast<uint32_t>(UnitsSettings::TemperatureUnitsCelsius));
        } else if (name == QLatin1String(UnitsSettings::weightUnitsName)) {
            metaData.setRawDefaultValue(static_cast<uint32_t>(UnitsSettings::WeightUnitsKg));
        }
        return false;
    }

    return parentResult;
}

// This modifies QGC colors palette to match possible custom corporate branding
void CustomPlugin::paletteOverride(const QString &colorName, QGCPalette::PaletteColorInfo_t& colorInfo)
{
    // KEVA palette (UI deck slide 1). The same colours go into both QGC themes so that
    // the UI looks the same whichever theme is selected; the theme setting is hidden anyway.
    using namespace KevaTheme;
    struct Entry { QColor enabled; QColor disabled; };
    static const QHash<QString, Entry> palette = {
        { QStringLiteral("window"),                 { Black,        Black } },
        { QStringLiteral("windowShadeLight"),       { PanelLight,   PanelLight } },
        { QStringLiteral("windowShade"),            { Panel,        Panel } },
        { QStringLiteral("windowShadeDark"),        { PanelDark,    PanelDark } },
        { QStringLiteral("text"),                   { DeepSkyBlue,  Disabled } },
        { QStringLiteral("warningText"),            { Red,          Red } },
        { QStringLiteral("button"),                 { Control,      PanelDark } },
        { QStringLiteral("buttonBorder"),           { DeepSkyBlue,  Border } },
        { QStringLiteral("buttonText"),             { DeepSkyBlue,  Disabled } },
        { QStringLiteral("buttonHighlight"),        { DeepSkyBlue,  Control } },
        { QStringLiteral("buttonHighlightText"),    { Black,        Disabled } },
        { QStringLiteral("primaryButton"),          { DeepSkyBlue,  Control } },
        { QStringLiteral("primaryButtonText"),      { Black,        Disabled } },
        { QStringLiteral("textField"),              { White,        PanelLight } },
        { QStringLiteral("textFieldText"),          { Black,        Grey } },
        { QStringLiteral("mapButton"),              { Black,        Control } },
        { QStringLiteral("mapButtonHighlight"),     { DeepSkyBlue,  Control } },
        { QStringLiteral("mapIndicator"),           { DeepSkyBlue,  Disabled } },
        { QStringLiteral("mapIndicatorChild"),      { NeonGreen,    Disabled } },
        { QStringLiteral("mapWidgetBorderLight"),   { White,        White } },
        { QStringLiteral("mapWidgetBorderDark"),    { Black,        Black } },
        { QStringLiteral("mapMissionTrajectory"),   { DeepSkyBlue,  DeepSkyBlue } },
        { QStringLiteral("brandingPurple"),         { DeepSkyBlue,  DeepSkyBlue } },
        { QStringLiteral("brandingBlue"),           { DeepSkyBlue,  DeepSkyBlue } },
        { QStringLiteral("colorGreen"),             { NeonGreen,    NeonGreen } },
        { QStringLiteral("colorYellow"),            { Yellow,       Yellow } },
        { QStringLiteral("colorYellowGreen"),       { QColor(0xAD, 0xFF, 0x2F), QColor(0xAD, 0xFF, 0x2F) } },
        { QStringLiteral("colorOrange"),            { Orange,       Orange } },
        { QStringLiteral("colorRed"),               { Red,          Red } },
        { QStringLiteral("colorGrey"),              { Grey,         Grey } },
        { QStringLiteral("colorBlue"),              { DeepSkyBlue,  DeepSkyBlue } },
        { QStringLiteral("alertBackground"),        { QColor(0x2B, 0x00, 0x00), QColor(0x2B, 0x00, 0x00) } },
        { QStringLiteral("alertBorder"),            { Red,          Red } },
        { QStringLiteral("alertText"),              { White,        White } },
        { QStringLiteral("missionItemEditor"),      { PanelDark,    PanelDark } },
        { QStringLiteral("statusFailedText"),       { Red,          Red } },
        { QStringLiteral("statusPassedText"),       { NeonGreen,    NeonGreen } },
        { QStringLiteral("statusPendingText"),      { Yellow,       Yellow } },
        { QStringLiteral("toolbarBackground"),      { Black,        Black } },
        { QStringLiteral("toolStripFGColor"),       { DeepSkyBlue,  Disabled } },
        { QStringLiteral("toolStripHoverColor"),    { QColor(0x00, 0x3F, 0x55), QColor(0x00, 0x3F, 0x55) } },
        { QStringLiteral("groupBorder"),            { Border,       Border } },
    };

    const auto it = palette.constFind(colorName);
    if (it == palette.constEnd()) {
        return;     // keep QGC's colour (survey/UTM specific colours)
    }
    for (QGCPalette::Theme theme : { QGCPalette::Light, QGCPalette::Dark }) {
        colorInfo[theme][QGCPalette::ColorGroupEnabled]  = it->enabled;
        colorInfo[theme][QGCPalette::ColorGroupDisabled] = it->disabled;
    }
}

// We override this so we can get access to QQmlApplicationEngine and use it to register our qml module
QQmlApplicationEngine* CustomPlugin::createQmlApplicationEngine(QObject* parent)
{
    _qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);
    _qmlEngine->addImportPath("qrc:/Custom/Widgets");
    // TODO: Investigate _qmlEngine->setExtraSelectors({"custom"})

    _selector = new CustomOverrideInterceptor();
    _qmlEngine->addUrlInterceptor(_selector);

    return _qmlEngine;
}

/*===========================================================================*/

CustomOverrideInterceptor::CustomOverrideInterceptor()
    : QQmlAbstractUrlInterceptor()
{

}

QUrl CustomOverrideInterceptor::intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type)
{
    switch (type) {
    using DataType = QQmlAbstractUrlInterceptor::DataType;
    case DataType::QmlFile:
    case DataType::UrlString:
        if (url.scheme() == QStringLiteral("qrc")) {
            const QString origPath = url.path();
            const QString overrideRes = QStringLiteral(":/Custom%1").arg(origPath);
            if (QFile::exists(overrideRes)) {
                const QString relPath = overrideRes.mid(2);
                QUrl result;
                result.setScheme(QStringLiteral("qrc"));
                result.setPath('/' + relPath);
                return result;
            }
        }
        break;
    default:
        break;
    }

    return url;
}

/*---------------------------------------------------------------------------*/
// Status Data panel helpers

bool CustomPlugin::mavlinkMessage(Vehicle *vehicle, LinkInterface *link, const mavlink_message_t &message)
{
    if (message.msgid == MAVLINK_MSG_ID_SERVO_OUTPUT_RAW && vehicle && vehicle == MultiVehicleManager::instance()->activeVehicle()) {
        mavlink_servo_output_raw_t raw;
        mavlink_msg_servo_output_raw_decode(&message, &raw);
        if (raw.port == 0) {
            const uint16_t pwm[8] = { raw.servo1_raw, raw.servo2_raw, raw.servo3_raw, raw.servo4_raw,
                                      raw.servo5_raw, raw.servo6_raw, raw.servo7_raw, raw.servo8_raw };
            auto percent = [&pwm](int output) -> QVariant {
                const uint16_t v = pwm[output - 1];
                if (v == 0 || v == UINT16_MAX) {
                    return QVariant(std::nan(""));
                }
                return QVariant(qBound(0.0, (v - kKevaPwmMin) * 100.0 / (kKevaPwmMax - kKevaPwmMin), 100.0));
            };
            QVariantList outputs { percent(kKevaFwOutput) };
            for (const int output : kKevaMcOutputs) {
                outputs.append(percent(output));
            }
            _motorOutputsVehicle = vehicle;
            _motorOutputsAge.start();
            _motorOutputs = outputs;
            emit kevaMotorOutputsChanged();
        }
    }
    return QGCCorePlugin::mavlinkMessage(vehicle, link, message);
}

void CustomPlugin::_checkMotorOutputs()
{
    if (_motorOutputs.isEmpty()) {
        return;
    }
    const bool stale = !_motorOutputsAge.isValid() || _motorOutputsAge.elapsed() > kKevaMotorStaleMs;
    if (stale || _motorOutputsVehicle.isNull() || _motorOutputsVehicle != MultiVehicleManager::instance()->activeVehicle()) {
        _motorOutputs.clear();
        emit kevaMotorOutputsChanged();
    }
}

QString CustomPlugin::kevaMgrs(const QGeoCoordinate &coord) const
{
    if (!coord.isValid()) {
        return QString();
    }
    // QGC gives "52SCG 16486 58021"; the UI deck writes the zone and the 100 km square apart
    QString mgrs = QGCGeo::convertGeoToMGRS(coord);
    static const QRegularExpression zoneSquare(QStringLiteral("^(\\d{1,2}[C-X])([A-Z]{2}) "));
    mgrs.replace(zoneSquare, QStringLiteral("\\1 \\2 "));
    return mgrs;
}

QVariantMap CustomPlugin::kevaSetpointLimits() const
{
    return {
        { QStringLiteral("altMin"),         kKevaAltMin },
        { QStringLiteral("altMax"),         kKevaAltMax },
        { QStringLiteral("altStep"),        kKevaAltStep },
        { QStringLiteral("airspeedStep"),   kKevaAirspeedStep },
    };
}

bool CustomPlugin::kevaSetTargetAltitude(double altitudeRel)
{
    Vehicle *vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle || !vehicle->flying() || !kevaCanControlVehicle() || qIsNaN(altitudeRel)) {
        return false;
    }
    const double current = vehicle->altitudeRelative()->rawValue().toDouble();
    if (qIsNaN(current)) {
        return false;
    }
    const double target = qBound(kKevaAltMin, std::round(altitudeRel / kKevaAltStep) * kKevaAltStep, kKevaAltMax);
    const QString mode = vehicle->flightMode();
    // PX4 cannot change only the altitude of a running mission: pause (Hold) first, then climb / descend
    vehicle->guidedModeChangeAltitude(target - current, true /* pauseVehicle */);
#ifdef KEVA_EDITION_MIL
    if (_auth) {
        _auth->recordEvent(QStringLiteral("ALTITUDE_COMMAND"), QStringLiteral("sent"),
                           QStringLiteral("target %1 m rel (current %2 m), mode %3").arg(target, 0, 'f', 0).arg(current, 0, 'f', 1).arg(mode));
    }
#endif
    return true;
}

bool CustomPlugin::kevaSetTargetAirspeed(double airspeed)
{
    Vehicle *vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle || !vehicle->flying() || !kevaCanControlVehicle() || qIsNaN(airspeed)) {
        return false;
    }
    // Multirotor flight has no airspeed target
    const bool fixedWingFlight = vehicle->vtol() ? vehicle->vtolInFwdFlight() : vehicle->fixedWing();
    const double minSpeed = vehicle->minimumEquivalentAirspeed();
    const double maxSpeed = vehicle->maximumEquivalentAirspeed();
    if (!fixedWingFlight || qIsNaN(minSpeed) || qIsNaN(maxSpeed) || maxSpeed < minSpeed) {
        return false;
    }
    const double target = qBound(minSpeed, std::round(airspeed / kKevaAirspeedStep) * kKevaAirspeedStep, maxSpeed);
    vehicle->guidedModeChangeEquivalentAirspeedMetersSecond(target);
#ifdef KEVA_EDITION_MIL
    if (_auth) {
        _auth->recordEvent(QStringLiteral("AIRSPEED_COMMAND"), QStringLiteral("sent"),
                           QStringLiteral("target %1 m/s, mode %2").arg(target, 0, 'f', 0).arg(vehicle->flightMode()));
    }
#endif
    return true;
}
