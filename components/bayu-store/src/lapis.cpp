// SPDX-License-Identifier: BSD-3-Clause

#include "lapis.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

#include "base/json.h"

using bayu::base::json::Value;

namespace store::lapis {

namespace {

constexpr const char* kSocket = "/run/lapis/control.sock";

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

} // namespace

State state()
{
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
    return simple("install", "path", apk);
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

} // namespace store::lapis
