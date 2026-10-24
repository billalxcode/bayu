// SPDX-License-Identifier: BSD-3-Clause

#include "network_manager.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>

#include "base/dbus.h"
#include "base/i18n.h"
#include "base/timer.h"
#include "dbus_util.h"

using bayu::base::dbus::Connection;
using bayu::base::dbus::ObjectPath;
using bayu::base::dbus::Proxy;
using bayu::base::dbus::Variant;
using bayu::base::dbus::VariantMap;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace installer {

namespace {

constexpr const char* kService = "org.freedesktop.NetworkManager";
constexpr const char* kPath = "/org/freedesktop/NetworkManager";
constexpr const char* kDevice = "org.freedesktop.NetworkManager.Device";
constexpr const char* kWired = "org.freedesktop.NetworkManager.Device.Wired";
constexpr const char* kWireless = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr const char* kAccessPoint = "org.freedesktop.NetworkManager.AccessPoint";

constexpr uint32_t kDeviceEthernet = 1;
constexpr uint32_t kDeviceWifi = 2;
constexpr uint32_t kStateActivated = 100;
// Preparing to activated: the device is on its way up.
constexpr uint32_t kStatePrepare = 40;

// RSN/WPA key management flags.
constexpr uint32_t kKeyMgmtPsk = 0x100;
constexpr uint32_t kKeyMgmtSae = 0x400;
constexpr uint32_t kKeyMgmt8021x = 0x200;

struct AccessPoint
{
    std::string path;
    std::string ssid;
    int strength = 0;
    uint32_t wpa = 0;
    uint32_t rsn = 0;
    uint32_t flags = 0;
};

std::vector<ObjectPath> devices(Connection* bus)
{
    Proxy manager(bus, kService, kPath, kService);
    std::vector<ObjectPath> paths;
    manager.call("GetDevices").read(paths);
    return paths;
}

std::string securityName(const AccessPoint& ap)
{
    if (ap.rsn & kKeyMgmtSae)
        return "WPA3";
    if (ap.rsn & (kKeyMgmtPsk | kKeyMgmt8021x))
        return "WPA2";
    if (ap.wpa)
        return "WPA";
    if (ap.flags & 1)
        return "WEP";
    return {};
}

} // namespace

NetworkManager::NetworkManager()
    : m_bus(Connection::system())
    , m_timer(std::make_unique<bayu::base::Timer>())
{
    m_timer->onTimeout = [this] { poll(); };
    if (m_bus)
        m_timer->start(std::chrono::seconds(4));
}

NetworkManager::~NetworkManager() = default;

void NetworkManager::poll()
{
    std::string signature;
    for (const NetworkConnection& connection : connections())
        signature += connection.id + (connection.connected ? "+" : "-") + std::to_string(connection.strength / 25);
    if (signature != m_signature) {
        m_signature = signature;
        if (onChanged)
            onChanged();
    }
}

std::vector<NetworkConnection> NetworkManager::connections() const
{
    std::vector<NetworkConnection> result;
    if (!m_bus)
        return result;
    for (const ObjectPath& path : devices(m_bus)) {
        Proxy device(m_bus, kService, path.path, kDevice);
        const uint32_t type = device.property<uint32_t>("DeviceType").value_or(0);
        const uint32_t state = device.property<uint32_t>("State").value_or(0);
        const std::string name = device.property<std::string>("Interface").value_or(std::string());
        if (type == kDeviceEthernet) {
            if (!device.property<bool>("Managed").value_or(true))
                continue;
            NetworkConnection connection;
            connection.id = path.path;
            connection.kind = NetworkConnection::Kind::Wired;
            connection.name = format(tr("Wired · {name}"), {{"name", name}});
            connection.connected = state == kStateActivated;
            connection.connecting = state >= kStatePrepare && state < kStateActivated;
            const uint32_t speed = Proxy(m_bus, kService, path.path, kWired).property<uint32_t>("Speed").value_or(0);
            if (connection.connected) {
                std::string rate = speed >= 1000 ? std::to_string(speed / 1000) + " Gbps"
                                                 : std::to_string(speed) + " Mbps";
                connection.detail = speed ? format(tr("Connected · {rate}"), {{"rate", rate}}) : tr("Connected");
            } else {
                const bool carrier = Proxy(m_bus, kService, path.path, kWired).property<bool>("Carrier").value_or(false);
                connection.detail = carrier ? tr("Cable connected · not set up") : tr("Cable unplugged");
            }
            connection.strength = 100;
            result.push_back(connection);
        } else if (type == kDeviceWifi) {
            Proxy wireless(m_bus, kService, path.path, kWireless);
            const std::string active = wireless.property<ObjectPath>("ActiveAccessPoint").value_or(ObjectPath{}).path;
            std::vector<ObjectPath> points;
            wireless.call("GetAllAccessPoints").read(points);
            // One row per network name, its strongest access point.
            std::map<std::string, AccessPoint> bySsid;
            for (const ObjectPath& point : points) {
                Proxy ap(m_bus, kService, point.path, kAccessPoint);
                AccessPoint entry;
                entry.path = point.path;
                const std::vector<uint8_t> ssid = ap.property<std::vector<uint8_t>>("Ssid").value_or(std::vector<uint8_t>{});
                entry.ssid.assign(ssid.begin(), ssid.end());
                if (entry.ssid.empty())
                    continue;
                entry.strength = ap.property<uint8_t>("Strength").value_or(0);
                entry.wpa = ap.property<uint32_t>("WpaFlags").value_or(0);
                entry.rsn = ap.property<uint32_t>("RsnFlags").value_or(0);
                entry.flags = ap.property<uint32_t>("Flags").value_or(0);
                auto it = bySsid.find(entry.ssid);
                if (it == bySsid.end() || entry.path == active || (it->second.path != active && entry.strength > it->second.strength))
                    bySsid[entry.ssid] = entry;
            }
            for (const auto& [ssid, ap] : bySsid) {
                NetworkConnection connection;
                connection.id = path.path + "|" + ap.path;
                connection.kind = NetworkConnection::Kind::Wireless;
                connection.name = ssid;
                connection.strength = ap.strength;
                const std::string security = securityName(ap);
                connection.secured = !security.empty();
                connection.connected = ap.path == active && state == kStateActivated;
                connection.connecting = (ap.path == active || connection.id == m_connecting) && state >= kStatePrepare
                                        && state < kStateActivated;
                if (connection.connected)
                    connection.detail = tr("Connected");
                else
                    connection.detail = connection.secured ? format(tr("{security} · secured"), {{"security", security}})
                                                           : tr("Open network");
                // Phones share their connection under names like these.
                if (ssid.find("iPhone") != std::string::npos || ssid.find("Android") != std::string::npos
                    || ssid.find("Hotspot") != std::string::npos) {
                    connection.kind = NetworkConnection::Kind::Hotspot;
                    if (!connection.connected)
                        connection.detail = tr("Personal hotspot");
                }
                result.push_back(connection);
            }
        }
    }
    // The connected one first, then wired, then by signal.
    std::stable_sort(result.begin(), result.end(), [](const NetworkConnection& a, const NetworkConnection& b) {
        if (a.connected != b.connected)
            return a.connected;
        if ((a.kind == NetworkConnection::Kind::Wired) != (b.kind == NetworkConnection::Kind::Wired))
            return a.kind == NetworkConnection::Kind::Wired;
        return a.strength > b.strength;
    });
    return result;
}

NetworkDetails NetworkManager::details(const std::string& id) const
{
    NetworkDetails details;
    if (!m_bus)
        return details;
    const std::string devicePath = id.substr(0, id.find('|'));
    Proxy device(m_bus, kService, devicePath, kDevice);
    details.hardwareAddress = device.property<std::string>("HwAddress").value_or(std::string());
    for (char& c : details.hardwareAddress)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    const uint32_t type = device.property<uint32_t>("DeviceType").value_or(0);
    if (type == kDeviceEthernet) {
        const uint32_t speed = Proxy(m_bus, kService, devicePath, kWired).property<uint32_t>("Speed").value_or(0);
        if (speed)
            details.linkSpeed = std::to_string(speed) + " Mb/s";
    } else if (type == kDeviceWifi) {
        const uint32_t bitrate = Proxy(m_bus, kService, devicePath, kWireless).property<uint32_t>("Bitrate").value_or(0);
        if (bitrate)
            details.linkSpeed = std::to_string(bitrate / 1000) + " Mb/s";
    }
    const std::string ip4 = device.property<ObjectPath>("Ip4Config").value_or(ObjectPath{}).path;
    if (ip4.empty() || ip4 == "/")
        return details;
    Proxy config(m_bus, kService, ip4, "org.freedesktop.NetworkManager.IP4Config");
    const std::vector<VariantMap> addresses =
        config.property<std::vector<VariantMap>>("AddressData").value_or(std::vector<VariantMap>{});
    if (!addresses.empty())
        details.ipv4 = dbus::get<std::string>(addresses.front(), "address") + "/"
                       + std::to_string(dbus::get<uint32_t>(addresses.front(), "prefix"));
    details.gateway = config.property<std::string>("Gateway").value_or(std::string());
    const std::vector<VariantMap> servers =
        config.property<std::vector<VariantMap>>("NameserverData").value_or(std::vector<VariantMap>{});
    for (const VariantMap& server : servers) {
        if (!details.dns.empty())
            details.dns += ", ";
        details.dns += dbus::get<std::string>(server, "address");
    }
    return details;
}

void NetworkManager::connect(const std::string& id, const std::string& password,
                             std::function<void(const std::string&)> done)
{
    if (!m_bus) {
        done(tr("NetworkManager is not running."));
        return;
    }
    Proxy manager(m_bus, kService, kPath, kService);
    manager.setTimeout(std::chrono::seconds(60));
    const size_t bar = id.find('|');
    m_connecting = id;
    if (bar == std::string::npos) {
        // A wired device: whatever connection suits it.
        m_pending = manager.callAsync("ActivateConnection", [done](const bayu::base::dbus::Reply& reply) { done(dbus::errorText(reply)); },
                          ObjectPath{"/"}, ObjectPath{id}, ObjectPath{"/"});
        return;
    }
    const std::string device = id.substr(0, bar);
    const std::string point = id.substr(bar + 1);
    std::map<std::string, VariantMap> settings;
    if (!password.empty()) {
        Proxy ap(m_bus, kService, point, kAccessPoint);
        const uint32_t rsn = ap.property<uint32_t>("RsnFlags").value_or(0);
        settings["802-11-wireless-security"] = {{"key-mgmt", Variant(std::string(rsn & kKeyMgmtSae ? "sae" : "wpa-psk"))},
                                                {"psk", Variant(password)}};
    }
    m_pending = manager.callAsync("AddAndActivateConnection",
                      [this, done](const bayu::base::dbus::Reply& reply) {
                          m_connecting.clear();
                          done(dbus::errorText(reply));
                      },
                      settings, ObjectPath{device}, ObjectPath{point});
}

bool NetworkManager::isOnline() const
{
    if (!m_bus)
        return false;
    // NM_STATE_CONNECTED_SITE (60) or GLOBAL (70).
    return Proxy(m_bus, kService, kPath, kService).property<uint32_t>("State").value_or(0) >= 60;
}

std::string NetworkManager::activeConnectionUuid() const
{
    if (!m_bus)
        return {};
    const std::string primary =
        Proxy(m_bus, kService, kPath, kService).property<ObjectPath>("PrimaryConnection").value_or(ObjectPath{}).path;
    if (primary.empty() || primary == "/")
        return {};
    return Proxy(m_bus, kService, primary, "org.freedesktop.NetworkManager.Connection.Active")
        .property<std::string>("Uuid")
        .value_or(std::string());
}

} // namespace installer
