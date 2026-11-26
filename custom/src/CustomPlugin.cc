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
