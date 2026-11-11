message(STATUS "QGC: Adding Custom Plugin")

# Enable custom build
set_property(DIRECTORY ${CMAKE_SOURCE_DIR}
    APPEND PROPERTY COMPILE_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
    KEVA_DISPLAY_NAME="${KEVA_DISPLAY_NAME}"
    KEVA_PRODUCT_VERSION="${KEVA_PRODUCT_VERSION}"
    KEVA_EDITION_${KEVA_EDITION}
)

if(ANDROID)
    set(CUSTOM_ANDROID_DIR "${CMAKE_SOURCE_DIR}/custom/android")
    if(EXISTS "${CUSTOM_ANDROID_DIR}")
        file(GLOB CUSTOM_ANDROID_FILES "${CUSTOM_ANDROID_DIR}/*")
        if(CUSTOM_ANDROID_FILES)
            message(STATUS "QGC: Custom Android package template found. Overlaying custom files...")
            set(DEFAULT_ANDROID_DIR "${CMAKE_SOURCE_DIR}/android")
            set(FINAL_ANDROID_DIR "${CMAKE_BINARY_DIR}/custom/android")
            file(COPY "${DEFAULT_ANDROID_DIR}/." DESTINATION "${FINAL_ANDROID_DIR}")
            file(COPY "${CUSTOM_ANDROID_DIR}/." DESTINATION "${FINAL_ANDROID_DIR}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                            "${DEFAULT_ANDROID_DIR}/"
                            "${CUSTOM_ANDROID_DIR}/"
                        )
            set(QGC_ANDROID_PACKAGE_SOURCE_DIR "${FINAL_ANDROID_DIR}" CACHE PATH "Path to a custom Android package template" FORCE)
            message(STATUS "QGC: Android package template path will be set to: ${QGC_ANDROID_PACKAGE_SOURCE_DIR}")
        else()
            message(STATUS "QGC: Custom Android package template empty. Using default.")
        endif()
    else()
        message(STATUS "QGC: No custom Android package template found. Using default.")
    endif()
endif()

# Our own, custom resources
set(KEVA_SECURITY_QRC ${CMAKE_CURRENT_SOURCE_DIR}/res/Security/security.qrc)
list(APPEND CUSTOM_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc
)
if(KEVA_EDITION STREQUAL "MIL")
    list(APPEND CUSTOM_RESOURCES ${KEVA_SECURITY_QRC})
endif()
set(_keva_resources ${QGC_RESOURCES} ${CUSTOM_RESOURCES})
if(NOT KEVA_EDITION STREQUAL "MIL")
    # The security module must never end up in the commercial edition,
    # even when a build folder previously configured as MIL is reused.
    list(REMOVE_ITEM _keva_resources ${KEVA_SECURITY_QRC})
endif()
list(REMOVE_DUPLICATES _keva_resources)
set(QGC_RESOURCES ${_keva_resources} CACHE STRING "Paths to .qrc Resources" FORCE)

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/res" CACHE STRING "Extra qml import paths" FORCE)

qt_add_library(CustomModule STATIC)

set_source_files_properties(res/Custom/Widgets/CustomArtificialHorizon.qml PROPERTIES QT_RESOURCE_ALIAS CustomArtificialHorizon.qml)
set_source_files_properties(res/Custom/Widgets/CustomAttitudeWidget.qml PROPERTIES QT_RESOURCE_ALIAS CustomAttitudeWidget.qml)
set_source_files_properties(res/Custom/Widgets/CustomIconButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomIconButton.qml)
set_source_files_properties(res/Custom/Widgets/CustomOnOffSwitch.qml PROPERTIES QT_RESOURCE_ALIAS CustomOnOffSwitch.qml)
set_source_files_properties(res/Custom/Widgets/CustomQuickButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomQuickButton.qml)
set_source_files_properties(res/Custom/Widgets/CustomSignalStrength.qml PROPERTIES QT_RESOURCE_ALIAS CustomSignalStrength.qml)
set_source_files_properties(res/Custom/Widgets/CustomToolBarButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomToolBarButton.qml)
set_source_files_properties(res/Custom/Widgets/CustomVehicleButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomVehicleButton.qml)

qt_add_qml_module(CustomModule
    URI Custom.Widgets
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        res/Custom/Widgets/CustomArtificialHorizon.qml
        res/Custom/Widgets/CustomAttitudeWidget.qml
        res/Custom/Widgets/CustomIconButton.qml
        res/Custom/Widgets/CustomOnOffSwitch.qml
        res/Custom/Widgets/CustomQuickButton.qml
        res/Custom/Widgets/CustomSignalStrength.qml
        res/Custom/Widgets/CustomToolBarButton.qml
        res/Custom/Widgets/CustomVehicleButton.qml
    NO_PLUGIN
)

set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.h
)

set(_keva_libraries
    CustomModule # Explicitly link QML module, needed by Qt 6.6.3, TODO: Remove when support for this version is dropped
    AutoPilotPluginsPX4Module
)

set(_keva_include_dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin
)

# Security module: MIL edition only (login, account policy, audit log)
if(KEVA_EDITION STREQUAL "MIL")
    message(STATUS "KEVA: security module enabled")
    set(_keva_sec ${CMAKE_CURRENT_SOURCE_DIR}/src/Security)
    list(APPEND CUSTOM_SOURCES
        ${_keva_sec}/SecurityConfig.h
        ${_keva_sec}/AccessPolicy.h
        ${_keva_sec}/AccessPolicy.cc
        ${_keva_sec}/IdleLockController.h
        ${_keva_sec}/IdleLockController.cc
        ${_keva_sec}/SecurityCrypto.h
        ${_keva_sec}/SecurityCrypto.cc
        ${_keva_sec}/PasswordPolicy.h
        ${_keva_sec}/PasswordPolicy.cc
        ${_keva_sec}/AccountStore.h
        ${_keva_sec}/AccountStore.cc
        ${_keva_sec}/AuditLog.h
        ${_keva_sec}/AuditLog.cc
        ${_keva_sec}/AuthManager.h
        ${_keva_sec}/AuthManager.cc
        ${_keva_sec}/LoginGate.h
        ${_keva_sec}/LoginGate.cc
    )
    list(APPEND _keva_include_dirs ${_keva_sec})
    if(WIN32)
        # CNG (PBKDF2, RNG), DPAPI, Windows event log
        list(APPEND _keva_libraries bcrypt crypt32 advapi32)
    endif()
else()
    message(STATUS "KEVA: security module not included (commercial edition)")
endif()

set(CUSTOM_SOURCES ${CUSTOM_SOURCES} CACHE INTERNAL "" FORCE)
set(CUSTOM_LIBRARIES ${_keva_libraries} CACHE INTERNAL "" FORCE)
set(CUSTOM_INCLUDE_DIRECTORIES ${_keva_include_dirs} CACHE INTERNAL "" FORCE)
