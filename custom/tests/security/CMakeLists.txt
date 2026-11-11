# Standalone unit tests for the KEVA security module.
# Not part of the QGC build. Build with any Qt 6 kit, e.g.:
#   cmake -S custom/tests/security -B build-security-tests
#   cmake --build build-security-tests
#   ctest --test-dir build-security-tests --output-on-failure
# On Windows this exercises the real CNG and DPAPI code paths.
cmake_minimum_required(VERSION 3.16)
project(KevaSecurityTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

set(SEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src/Security)
set(RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../res/Security)

add_executable(tst_security
    tst_security.cc
    ${SEC_DIR}/SecurityCrypto.cc
    ${SEC_DIR}/PasswordPolicy.cc
    ${SEC_DIR}/AccountStore.cc
    ${SEC_DIR}/AuditLog.cc
    ${SEC_DIR}/AuthManager.cc
    ${SEC_DIR}/AuthManager.h
    ${SEC_DIR}/AccessPolicy.cc
)
target_include_directories(tst_security PRIVATE ${SEC_DIR})
target_compile_definitions(tst_security PRIVATE
    KEVA_TEST_DICTIONARY="${RES_DIR}/dictionary_words.txt"
    KEVA_TEST_COMMON_PASSWORDS="${RES_DIR}/common_passwords.txt"
)
target_link_libraries(tst_security PRIVATE Qt6::Core Qt6::Test)
if(WIN32)
    target_link_libraries(tst_security PRIVATE bcrypt crypt32 advapi32)
endif()

enable_testing()
add_test(NAME tst_security COMMAND tst_security)

# Optional: login window preview (needs Qt Quick). Renders every stage to PNG.
find_package(Qt6 QUIET COMPONENTS Gui Qml Quick QuickControls2)
if(Qt6Quick_FOUND AND Qt6QuickControls2_FOUND)
    qt_add_executable(login_preview
        login_preview.cc
        ${SEC_DIR}/SecurityCrypto.cc
        ${SEC_DIR}/PasswordPolicy.cc
        ${SEC_DIR}/AccountStore.cc
        ${SEC_DIR}/AuditLog.cc
        ${SEC_DIR}/AuthManager.cc
        ${SEC_DIR}/AuthManager.h
        ${SEC_DIR}/LoginGate.cc
        ${SEC_DIR}/IdleLockController.cc
        ${SEC_DIR}/IdleLockController.h
        ${RES_DIR}/security.qrc
    )
    set_target_properties(login_preview PROPERTIES AUTORCC ON)
    target_include_directories(login_preview PRIVATE ${SEC_DIR})
    target_compile_definitions(login_preview PRIVATE
        KEVA_TEST_DICTIONARY="${RES_DIR}/dictionary_words.txt"
        KEVA_TEST_COMMON_PASSWORDS="${RES_DIR}/common_passwords.txt"
    )
    target_link_libraries(login_preview PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    if(WIN32)
        target_link_libraries(login_preview PRIVATE bcrypt crypt32 advapi32)
    endif()
endif()
