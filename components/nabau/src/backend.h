// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "model.h"

namespace installer {

// What the installer drives: Anaconda's modules over D-Bus, or sample data
// for development and screenshots. Calls that may take a while report back
// through callbacks on the UI thread; the rest answer from what the backend
// read when it started.
class Backend
{
public:
    virtual ~Backend() = default;

    // Starts or connects to the installation backend. `status` tells what
    // it is waiting for; `ready` follows once, with an error text on failure.
    virtual void start(std::function<void(const std::string& status)> status,
                       std::function<void(const std::string& error)> ready) = 0;

    virtual SystemInfo systemInfo() const = 0;

    // Localization.
    virtual std::vector<Language> languages() const = 0;
    virtual std::vector<Locale> locales(const std::string& language) const = 0;
    // "en_US.UTF-8".
    virtual std::string locale() const = 0;
    virtual void setLocale(const std::string& locale) = 0;
    virtual std::vector<KeyboardLayout> keyboardLayouts(const std::string& language) const = 0;
    virtual std::string keyboardLayout() const = 0;
    // Also applied to the running session, so the layout can be tried.
    virtual void setKeyboardLayout(const std::string& layout) = 0;

    // Time.
    // Region to cities ("Asia" -> {"Jakarta", ...}).
    virtual std::vector<std::pair<std::string, std::vector<std::string>>> timezones() const = 0;
    virtual TimeSettings timeSettings() const = 0;
    virtual void setTimeSettings(const TimeSettings& settings) = 0;

    // Network.
    virtual std::vector<NetworkConnection> connections() const = 0;
    virtual NetworkDetails details(const std::string& connection) const = 0;
    // Wi-Fi: joins with the password, empty for open networks.
    virtual void connect(const std::string& connection, const std::string& password,
                         std::function<void(const std::string& error)> done) = 0;
    virtual std::string hostname() const = 0;
    virtual void setHostname(const std::string& hostname) = 0;
    virtual bool connectAutomatically() const = 0;
    virtual void setConnectAutomatically(bool automatic) = 0;
    virtual bool isOnline() const = 0;
    // The connection list or state changed.
    std::function<void()> onNetworkChanged;

    // Storage.
    virtual std::vector<Disk> disks() const = 0;
    // Rescans the disks, as after one was plugged in.
    virtual void rescan(std::function<void()> done) = 0;
    // Works the plan out without touching the disks: the layout it makes and
    // what the checks say about it. Custom plans take the editor's layout.
    virtual void planStorage(const StoragePlan& plan,
                             std::function<void(const std::vector<LayoutPart>& layout,
                                                const ValidationReport& report)> done) = 0;
    // The space a partition can shrink to, to install beside it.
    virtual uint64_t minimumSize(const std::string& device) const = 0;
    // Applies the plan to the backend: Automatic partitions; Custom takes the
    // editor's scheduled changes. The report says what blocks the install.
    virtual void applyStorage(const StoragePlan& plan, const std::string& passphrase,
                              std::function<void(const ValidationReport& report)> done) = 0;
    // The resulting layout of the plan applied last, mount points first.
    virtual std::vector<LayoutPart> appliedLayout() const = 0;
    // The installation needs this much space, for the checks.
    virtual uint64_t requiredSpace() const = 0;

    // The custom editor, on the disks of the plan. The disks as they will be
    // with the scheduled changes; each call schedules a change at once.
    virtual void beginCustom(const std::vector<std::string>& disks) = 0;
    virtual std::vector<Disk> customDisks() const = 0;
    struct MountRequest
    {
        // The free space to create in, or the device to change.
        std::string disk;
        std::string device;
        std::string mountPoint;
        uint64_t size = 0;
        std::string format;
        bool reformat = true;
        bool encrypt = false;
        std::string label;
        // Advanced: "partition", "lvm", "lvm-thin" or "btrfs".
        std::string type = "partition";
    };
    // An error text, empty on success.
    virtual std::string addMount(const MountRequest& request) = 0;
    virtual std::string changeMount(const MountRequest& request) = 0;
    virtual std::string resize(const std::string& device, uint64_t size) = 0;
    virtual std::string remove(const std::string& device) = 0;
    // The recommended mount points created in the free space left.
    virtual std::string createRecommended(bool encrypt) = 0;
    virtual void resetCustom() = 0;
    virtual std::vector<PendingAction> pendingActions() const = 0;
    virtual std::vector<std::string> fileSystems() const = 0;
    virtual std::vector<std::string> suggestedMountPoints() const = 0;
    // The device types new mount points can have, "partition" first.
    virtual std::vector<std::string> deviceTypes() const { return {"partition"}; }
    // The volumes of an LVM group or a Btrfs volume on a partition.
    virtual std::vector<DiskSegment> volumes(const std::string&) const { return {}; }
    // Limits for a device's size; equal when it cannot change.
    virtual std::pair<uint64_t, uint64_t> sizeLimits(const std::string& device) const = 0;

    // Software.
    virtual std::vector<SoftwareEnvironment> environments() const = 0;
    virtual std::vector<SoftwareAddon> addons(const std::string& environment) const = 0;
    virtual void setSoftware(const std::string& environment, const std::vector<std::string>& addons) = 0;
    virtual SoftwareSummary softwareSummary() const = 0;

    // Account.
    virtual void setAccount(const Account& account) = 0;
    // Encrypting a disk is possible with the file systems offered.
    virtual bool supportsEncryption() const { return true; }

    // Installation.
    virtual void install(std::function<void(const InstallProgress& progress)> progress,
                         std::function<void(const InstallLogLine& line)> log,
                         std::function<void(const InstallResult& result)> done) = 0;
    virtual bool canCancelInstall() const = 0;
    virtual void cancelInstall() = 0;
    virtual std::string logText() const = 0;
    virtual void reboot() = 0;
    // Leaves the backend, as the installer window closes.
    virtual void quit() = 0;
};

std::unique_ptr<Backend> createMockBackend();
std::unique_ptr<Backend> createAnacondaBackend();

} // namespace installer
