// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/dbus.h"
#include "base/signal.h"

namespace settings {

// One of the compositor's input services, such as org.bayu.Keyboard1:
// its properties, read when it appears and again after each of its change
// signals, and calls to its setters. The first read blocks, so a page shows
// the service's values from its first frame.
class CompositorService
{
public:
    // The object path follows the name: org.bayu.Mouse1 lives at
    // /org/bayu/Mouse1, and its interface has the same name.
    explicit CompositorService(std::string name);
    ~CompositorService();

    CompositorService(const CompositorService&) = delete;
    CompositorService& operator=(const CompositorService&) = delete;

    // Whether the service is running.
    bool isAvailable() const { return m_available; }

    bool boolValue(const std::string& name, bool fallback) const;
    int intValue(const std::string& name, int fallback) const;
    double doubleValue(const std::string& name, double fallback) const;
    std::string stringValue(const std::string& name, const std::string& fallback = {}) const;
    std::vector<std::string> stringListValue(const std::string& name) const;

    const bayu::base::dbus::Proxy& proxy() const { return m_proxy; }

    // Calls a setter. When the service refuses, every property is read
    // again, so the page goes back to what the service has. done, when
    // given, gets the reply.
    template<typename... Args>
    void call(const std::string& method, const Args&... args)
    {
        callWithReply(method, {}, args...);
    }
    template<typename... Args>
    void callWithReply(const std::string& method, std::function<void(const bayu::base::dbus::Reply&)> done,
                       const Args&... args)
    {
        if (!m_available)
            return;
        const uint64_t id = ++m_lastCall;
        m_calls[id] = m_proxy.callAsync(method, [this, id, method, done](const bayu::base::dbus::Reply& reply) {
            finishCall(id, method, reply);
            if (done)
                done(reply);
        }, args...);
    }

    // After the values were read again, or the service came or went.
    bayu::base::Signal<> changed;

private:
    const bayu::base::dbus::Variant* find(const std::string& name) const;
    void reload();
    void finishCall(uint64_t id, const std::string& method, const bayu::base::dbus::Reply& reply);

    bayu::base::dbus::Proxy m_proxy;
    bayu::base::dbus::NameWatch m_watch;
    bayu::base::dbus::Match m_signals;
    bayu::base::dbus::PendingCall m_reload;
    std::map<uint64_t, bayu::base::dbus::PendingCall> m_calls;
    uint64_t m_lastCall = 0;
    // Tells a posted task whether this is still there.
    std::shared_ptr<int> m_alive = std::make_shared<int>(0);
    bayu::base::dbus::VariantMap m_properties;
    bool m_available = false;
};

} // namespace settings
