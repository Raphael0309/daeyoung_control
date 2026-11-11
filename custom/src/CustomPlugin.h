/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief Custom QGCCorePlugin Declaration
 *   @author Gus Grubba <gus@auterion.com>
 */

#pragma once

#include <QtCore/QTranslator>
#include <QtQml/QQmlAbstractUrlInterceptor>

#include "QGCCorePlugin.h"
#include "QGCOptions.h"

class CustomOptions;
class CustomPlugin;
class CustomSettings;
class QQmlApplicationEngine;

#ifdef KEVA_EDITION_MIL
namespace KevaSecurity { class AuthManager; class IdleLockController; }
#endif

#include <QtCore/QElapsedTimer>
#include <atomic>

Q_DECLARE_LOGGING_CATEGORY(CustomLog)

class CustomFlyViewOptions : public QGCFlyViewOptions
{
public:
    CustomFlyViewOptions(CustomOptions* options, QObject* parent = nullptr);

    // Overrides from CustomFlyViewOptions
    bool                    showInstrumentPanel         (void) const final;
    bool                    showMultiVehicleList        (void) const final;
};

/*===========================================================================*/

class CustomOptions : public QGCOptions
{
public:
    CustomOptions(CustomPlugin *plugin, QObject* parent = nullptr);

    // Overrides from QGCOptions
    bool                    wifiReliableForCalibration  (void) const final;
    bool                    showFirmwareUpgrade         (void) const final;
    QGCFlyViewOptions*      flyViewOptions(void) const final;

private:
    QGCCorePlugin *_plugin = nullptr;
    CustomFlyViewOptions *_flyViewOptions = nullptr;
};

/*===========================================================================*/

class CustomPlugin : public QGCCorePlugin
{
    Q_OBJECT

    // Session and role information for QML (fixed for the life of the application;
    // switching user means restarting). In the COM edition everything is allowed.
    Q_PROPERTY(bool     kevaSecured             READ kevaSecured                CONSTANT)
    Q_PROPERTY(bool     kevaIsAdmin             READ kevaIsAdmin                CONSTANT)
    Q_PROPERTY(QString  kevaUserId              READ kevaUserId                 CONSTANT)
    Q_PROPERTY(bool     kevaCanControlVehicle   READ kevaCanControlVehicle      CONSTANT)
    Q_PROPERTY(bool     kevaCanPlan             READ kevaCanPlan                CONSTANT)
    Q_PROPERTY(bool     kevaCanConfigureVehicle READ kevaCanConfigureVehicle    CONSTANT)

public:
    explicit CustomPlugin(QObject *parent = nullptr);
    ~CustomPlugin();

    static QGCCorePlugin *instance();

    // Overrides from QGCCorePlugin
    void init() final;
    void cleanup() final;
    QGCOptions*             options                         (void) final;
    QString                 brandImageIndoor                (void) const final;
    QString                 brandImageOutdoor               (void) const final;
    bool                    overrideSettingsGroupVisibility (const QString &name) final;
    bool                    adjustSettingMetaData           (const QString& settingsGroup, FactMetaData& metaData) final;
    void                    paletteOverride                 (const QString &colorName, QGCPalette::PaletteColorInfo_t& colorInfo) final;
    QQmlApplicationEngine*  createQmlApplicationEngine      (QObject* parent) final;
    QList<int>              firstRunPromptStdIds            () final;
    bool                    vehicleControlAllowed           (int command) final;

    bool    kevaSecured             () const;
    bool    kevaIsAdmin             () const;
    QString kevaUserId              () const;
    bool    kevaCanControlVehicle   () const;
    bool    kevaCanPlan             () const;
    bool    kevaCanConfigureVehicle () const;
    Q_INVOKABLE bool kevaSettingsPageAllowed(const QString &pageUrl) const;

#ifdef KEVA_EDITION_MIL
    /// Login session of the secured edition (role, user ID). Valid after init().
    KevaSecurity::AuthManager *authManager() const { return _auth; }
#endif

private slots:
    void _advancedChanged(bool advanced);

private:
    void _addSettingsEntry(const QString& title, const char* qmlFile, const char* iconFile = nullptr);

private:
    CustomOptions*  _options = nullptr;
    QQmlApplicationEngine *_qmlEngine = nullptr;
    class CustomOverrideInterceptor *_selector = nullptr;
    QVariantList    _customSettingsList; // Not to be mixed up with QGCCorePlugin implementation
#ifdef KEVA_EDITION_MIL
    KevaSecurity::AuthManager *_auth = nullptr;
    KevaSecurity::IdleLockController *_idleLock = nullptr;
    std::atomic<bool> _vehicleControlRestricted { false };
    QElapsedTimer _lastDeniedNotice;
#endif
};

/*===========================================================================*/

class CustomOverrideInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    CustomOverrideInterceptor();

    QUrl intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type) final;
};
