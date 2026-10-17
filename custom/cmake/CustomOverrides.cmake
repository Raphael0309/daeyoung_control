#App
set(QGC_APP_NAME "DAEYOUNG_Control" CACHE STRING "App Name" FORCE)
set(QGC_ORG_NAME "DAEYOUNG_MnS" CACHE STRING "Org Name" FORCE)
set(QGC_ORG_DOMAIN "dyds.com" CACHE STRING "Org Domain" FORCE)
set(QGC_APP_DESCRIPTION "DAEYOUNG_Ground_Control_Station" CACHE STRING "Description" FORCE)
set(QGC_APP_COPYRIGHT "Copyright (c) 2026 DAEYOUNG_MnS" CACHE STRING "Copyright" FORCE)

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

