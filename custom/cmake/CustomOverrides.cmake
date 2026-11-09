#######################################################
#   Daeyoung Ground Control (KEVA GCS) - build overrides
#######################################################

# Product edition
#   MIL : secured edition (login, account policy, audit log)
#   COM : commercial edition (no security module compiled in)
set(KEVA_EDITION "MIL" CACHE STRING "Product edition: MIL (secured) or COM (commercial)")
set_property(CACHE KEVA_EDITION PROPERTY STRINGS MIL COM)
string(TOUPPER "${KEVA_EDITION}" KEVA_EDITION)
if(NOT KEVA_EDITION MATCHES "^(MIL|COM)$")
    message(FATAL_ERROR "KEVA_EDITION must be MIL or COM (got '${KEVA_EDITION}')")
endif()

# Product version shown to users (independent of upstream QGC tags)
set(KEVA_PRODUCT_VERSION "1.0.1" CACHE STRING "Product version (major.minor.patch)")
set(QGC_APP_VERSION_OVERRIDE "${KEVA_PRODUCT_VERSION}")

#App
# QGC_APP_NAME is used for the executable/target name and the settings location,
# so it must not contain spaces. KEVA_DISPLAY_NAME is what users see.
# Each edition uses its own name so both can be installed on the same PC.
if(KEVA_EDITION STREQUAL "MIL")
    set(QGC_APP_NAME "DaeyoungGCS" CACHE STRING "App Name" FORCE)
    set(KEVA_DISPLAY_NAME "Daeyoung Ground Control" CACHE STRING "User visible product name" FORCE)
else()
    set(QGC_APP_NAME "DaeyoungGCS_CE" CACHE STRING "App Name" FORCE)
    set(KEVA_DISPLAY_NAME "Daeyoung Ground Control CE" CACHE STRING "User visible product name" FORCE)
endif()
set(QGC_ORG_NAME "DAEYOUNG_MnS" CACHE STRING "Org Name" FORCE)
set(QGC_ORG_DOMAIN "dyds.com" CACHE STRING "Org Domain" FORCE)
set(QGC_APP_DESCRIPTION "${KEVA_DISPLAY_NAME}" CACHE STRING "Description" FORCE)
set(QGC_APP_COPYRIGHT "Copyright (c) 2026 Daeyoung M&S" CACHE STRING "Copyright" FORCE)

message(STATUS "KEVA: edition=${KEVA_EDITION} name='${KEVA_DISPLAY_NAME}' version=${KEVA_PRODUCT_VERSION}")

set(QGC_MACOS_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/res" CACHE PATH "MacOS Icon Path" FORCE)
set(QGC_APPIMAGE_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/res/icons/custom_qgroundcontrol.png" CACHE FILEPATH "AppImage Icon Path" FORCE)

if(EXISTS ${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp)
    set(QGC_WINDOWS_INSTALL_HEADER_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/installheader.bmp" CACHE FILEPATH "Windows Install Header Path" FORCE)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/custom/deploy/windows/WindowsQGC.ico)
    set(QGC_WINDOWS_ICON_PATH "${CMAKE_SOURCE_DIR}/custom/deploy/windows/WindowsQGC.ico" CACHE FILEPATH "Windows Icon Path" FORCE)
endif()

# Build a single flight stack by disabling APM support
set(QGC_DISABLE_APM_MAVLINK ON CACHE BOOL "Disable APM Dialect" FORCE)
set(QGC_DISABLE_APM_PLUGIN ON CACHE BOOL "Disable APM Plugin" FORCE)
set(QGC_DISABLE_APM_PLUGIN_FACTORY ON CACHE BOOL "Disable APM Plugin Factory" FORCE)

# We implement our own PX4 plugin factory
set(QGC_DISABLE_PX4_PLUGIN_FACTORY ON CACHE BOOL "Disable PX4 Plugin Factory" FORCE)

# Always Stable build not daily
set(QGC_STABLE_BUILD ON CACHE BOOL "Stable Build" FORCE)
