// SPDX-License-Identifier: GPL-3.0-or-later
// Android apps through lapis' control service (/run/lapis/control.sock):
// one JSON request per connection, as `lapis app ...` sends them. Blocking:
// call from a worker thread.

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace store::lapis {

enum class State {
    // lapis is not installed, its control socket is off, or Android's
    // images are not all there (their download did not finish).
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

// Android for this session, through the lapis-session user unit. Starting
// waits, at most `timeout`, for Android to run; an error text when it does
// not. Stopping keeps Android off at the next logins too, until it starts
// again: lapis-session does not start while the mark turnedOff() reads is
// there.
std::string startSession(std::chrono::seconds timeout);
std::string stopSession();
bool turnedOff();

} // namespace store::lapis
