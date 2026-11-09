/****************************************************************************
 *
 * Daeyoung Ground Control - login gate (MIL edition)
 *
 * Runs the login window in a nested event loop from CustomPlugin::init(),
 * i.e. before QGC creates its main window, MAVLink links, video or maps.
 *
 ****************************************************************************/

#pragma once

#include "AuthManager.h"

namespace KevaSecurity {

class LoginGate
{
public:
    /// Production options: ProgramData security folder, PC name, product strings, qrc word lists.
    static AuthManager::Options defaultOptions();

    /// Folder holding accounts.dat and audit.log.
    static QString securityDirectory();

    /// Shows the login window and blocks until the user is authenticated (true)
    /// or closes the window / cannot be authenticated (false).
    static bool run(AuthManager *auth);
};

} // namespace KevaSecurity
