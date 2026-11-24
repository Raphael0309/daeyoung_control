target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        EventHandler.cc
        EventHandler.h
        HealthAndArmingCheckReport.cc
        HealthAndArmingCheckReport.h
        logging.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

#===========================================================================#

# KEVA custom build: pin libevents. Upstream tracks "main", but libevents 0ef305f
# (2026-09-28, "support 32 bit system IDs") calls mavlink_msg_target_field() /
# mavlink_msg_get_target_system(), which the MAVLink headers pinned by QGC 5.0
# (QGC_MAVLINK_GIT_TAG) do not have. 077677a is the last compatible commit.
CPMAddPackage(
    NAME libevents
    GITHUB_REPOSITORY mavlink/libevents
    GIT_TAG 077677a7d2f48f0af0eb8a4e4a7b6310956728bc
    SOURCE_SUBDIR libs/cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libevents)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${libevents_SOURCE_DIR}/libs/cpp)
