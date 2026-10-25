// SPDX-License-Identifier: GPL-3.0-or-later
// Android apps through lapis' control service (/run/lapis/control.sock):
// one JSON request per connection, as `lapis app ...` sends them. Blocking:
// call from a worker thread.

#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace store::lapis {

enum class State {
    // lapis is not installed or its control socket is off.
    Missing,
    // Installed, Android not started for this session.
    Stopped,
    Running,
    Frozen,
};

State state();
// Installed apps and their version codes.
std::map<std::string, int64_t> packages(std::string* error);
// An APK the user owns, outside /tmp (the service has a private one).
std::string install(const std::string& apk);
std::string uninstall(const std::string& package);
std::string launch(const std::string& package);
std::string unfreeze();

} // namespace store::lapis
