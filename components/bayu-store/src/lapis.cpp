// SPDX-License-Identifier: GPL-3.0-or-later

#include "lapis.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

#include "base/json.h"
#include "base/paths.h"

using bayu::base::json::Value;

namespace store::lapis {

namespace {

constexpr const char* kSocket = "/run/lapis/control.sock";
// Android's images, where android-setup has `lapis init` put them.
constexpr const char* kSystemImage = "/var/lib/lapis/images/system.img";
constexpr const char* kVendorImage = "/var/lib/lapis/images/vendor.img";

// lapis-session.service does not start while it is there (Bayu Store's
// drop-in, %h/.config/bayu-store/android-off): the same place whatever
// XDG_CONFIG_HOME says.
std::string offMark()
{
    return bayu::base::paths::home() + "/.config/bayu-store/android-off";
}

// One request, its answer; an error text in `error` when it failed.
Value call(const Value& request, std::string* error, int timeoutSeconds = 180)
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        *error = "Cannot reach lapis.";
        return Value();
    }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, kSocket, sizeof(address.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(fd);
        *error = "lapis is not running (its control socket is off).";
        return Value();
    }
    timeval timeout{timeoutSeconds, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    const std::string line = request.serialize() + "\n";
    if (::send(fd, line.data(), line.size(), MSG_NOSIGNAL) != ssize_t(line.size())) {
        ::close(fd);
        *error = "Cannot talk to lapis.";
        return Value();
    }
    std::string answer;
    char buffer[4096];
    while (true) {
        const ssize_t read = ::recv(fd, buffer, sizeof(buffer), 0);
        if (read <= 0)
            break;
        answer.append(buffer, size_t(read));
        if (answer.find('\n') != std::string::npos)
            break;
    }
    ::close(fd);
    const std::optional<Value> value = Value::parse(answer);
    if (!value || !value->isObject()) {
        *error = "lapis did not answer.";
        return Value();
    }
    if (!(*value)["ok"].toBool()) {
        *error = (*value)["error"].toString("lapis refused.");
        return Value();
    }
    return *value;
}

Value request(const char* op)
{
    Value v = Value::object();
    v.insert("op", op);
    return v;
}

std::string simple(const char* op, const char* field, const std::string& value)
{
    Value r = request(op);
    if (field)
        r.insert(field, value);
    std::string error;
    call(r, &error);
    return error;
}

// Android's container runs, but Android in it never finished booting: without
// a GPU that renders (OpenGL, or virgl in a virtual machine) its surfaceflinger
// aborts and Android starts over (#46). Its services, "package" first, never come.
constexpr const char* kNotBooted =
        "Android started but did not finish booting. It needs graphics with 3D acceleration "
        "(OpenGL; in a virtual machine, virgl), which this computer may not have.";

} // namespace

State state()
{
    // Not set up until its images are there: lapis answers once its control
    // socket is on, also while (or after) their download stopped half way.
    if (::access(kSystemImage, F_OK) != 0 || ::access(kVendorImage, F_OK) != 0)
        return State::Missing;
    std::string error;
    const Value answer = call(request("status"), &error, 10);
    if (!error.empty())
        return State::Missing;
    const std::string s = answer["state"].toString();
    if (s == "RUNNING")
        return State::Running;
    if (s == "FROZEN")
        return State::Frozen;
    return State::Stopped;
}

std::map<std::string, int64_t> packages(std::string* error)
{
    std::map<std::string, int64_t> out;
    std::string failure;
    const Value answer = call(request("packages"), &failure, 30);
    if (!failure.empty()) {
        if (error)
            *error = failure;
        return out;
    }
    const Value& list = answer["packages"];
    for (size_t i = 0; i < list.size(); ++i)
        out[list[i]["package"].toString()] = list[i]["version_code"].toInt();
    return out;
}

std::string install(const std::string& apk)
{
    const std::string error = simple("install", "path", apk);
    return error.find("Can't find service") != std::string::npos ? kNotBooted : error;
}

std::string uninstall(const std::string& package)
{
    return simple("uninstall", "package", package);
}

std::string launch(const std::string& package)
{
    return simple("launch", "package", package);
}

std::string unfreeze()
{
    return simple("unfreeze", nullptr, {});
}

static bool booted()
{
    Value r = request("getprop");
    r.insert("prop", "sys.boot_completed");
    std::string error;
    return call(r, &error, 10)["value"].toString() == "1";
}

std::string startSession(std::chrono::seconds timeout)
{
    std::error_code ignored;
    std::filesystem::remove(offMark(), ignored);
    // lapis-session.desktop imports these at login only when lapis was set
    // up then; Android's windows need them.
    std::system("systemctl --user import-environment WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE");
    if (std::system("systemctl --user --no-block start lapis-session.service") != 0)
        return "lapis-session could not be started.";
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool ran = false;
    while (true) {
        const State now = state();
        if (now == State::Missing)
            return "lapis is not available.";
        ran = ran || now == State::Running;
        // Running is the container: apps open, and install, once Android
        // has booted in it ("has no launcher activity" before). Frozen it
        // booted already, and answers no queries.
        if (now == State::Frozen || (now == State::Running && booted()))
            return {};
        if (std::chrono::steady_clock::now() >= deadline)
            return ran ? kNotBooted : "Android did not start in time.";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::string stopSession()
{
    const std::string mark = offMark();
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::path(mark).parent_path(), ignored);
    std::ofstream(mark).flush();
    if (std::system("systemctl --user stop lapis-session.service") != 0)
        return "lapis-session could not be stopped.";
    return {};
}

bool turnedOff()
{
    return ::access(offMark().c_str(), F_OK) == 0;
}

} // namespace store::lapis
