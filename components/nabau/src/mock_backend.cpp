// SPDX-License-Identifier: GPL-3.0-or-later
// Sample data shaped like the design, for developing the pages and taking
// screenshots without a backend: nabau --mock.

#include <algorithm>
#include <chrono>

#include "backend.h"
#include "base/event_loop.h"
#include "base/timer.h"

namespace installer {

namespace {

constexpr uint64_t GB = 1000ull * 1000 * 1000;

class MockBackend : public Backend
{
public:
    MockBackend()
    {
        Disk nvme{.id = "nvme0n1",
                  .path = "/dev/nvme0n1",
                  .model = "Samsung 990 PRO",
                  .bus = "NVMe",
                  .size = 1000 * GB,
                  .freeSpace = 1000 * GB};
        nvme.segments.push_back({.start = 0, .size = 1000 * GB, .free = true});
        Disk sata{.id = "sda",
                  .path = "/dev/sda",
                  .model = "Crucial MX500",
                  .bus = "SATA SSD",
                  .size = 500 * GB,
                  .freeSpace = 0,
                  .reclaimableSpace = 410 * GB,
                  .systems = {"Windows 11"}};
        sata.segments.push_back({.device = "sda1", .start = 0, .size = GB / 10, .format = "efi",
                                 .label = "EFI", .description = "EFI System Partition"});
        sata.segments.push_back({.device = "sda2", .start = GB / 10, .size = GB / 50, .format = "",
                                 .description = "Microsoft reserved"});
        sata.segments.push_back({.device = "sda3", .start = GB / 8, .size = 499 * GB, .format = "ntfs",
                                 .label = "Windows", .description = "Windows 11"});
        Disk usb{.id = "sdb",
                 .path = "/dev/sdb",
                 .model = "SanDisk Ultra",
                 .bus = "USB 3.2",
                 .size = 32 * GB,
                 .removable = true,
                 .installationMedia = true};
        m_disks = {nvme, sata, usb};
    }

    void start(std::function<void(const std::string&)> status, std::function<void(const std::string&)> ready) override
    {
        status("Starting the installation backend");
        ready({});
    }

    SystemInfo systemInfo() const override
    {
        return {.productName = "IGOS Nusantara 26",
                .productShortName = "IGN 26",
                .productVersion = "26",
                .backendVersion = "40.22",
                .architecture = "x86_64",
                .uefi = true,
                .secureBoot = true};
    }

    std::vector<Language> languages() const override
    {
        return {{"en", "English", "English", "United States"},
                {"id", "Bahasa Indonesia", "Indonesian", "Indonesia"},
                {"de", "Deutsch", "German", "Deutschland"},
                {"es", "Español", "Spanish", "España"},
                {"fr", "Français", "French", "France"},
                {"ja", "日本語", "Japanese", "日本"},
                {"pt", "Português", "Portuguese", "Brasil"},
                {"jv", "Basa Jawa", "Javanese", "Indonesia"},
                {"su", "Basa Sunda", "Sundanese", "Indonesia"},
                {"zh", "中文", "Chinese", "中国"}};
    }

    std::vector<Locale> locales(const std::string& language) const override
    {
        if (language == "en")
            return {{"en_US.UTF-8", "United States", "US"},
                    {"en_GB.UTF-8", "United Kingdom", "GB"},
                    {"en_AU.UTF-8", "Australia", "AU"},
                    {"en_CA.UTF-8", "Canada", "CA"},
                    {"en_IN.UTF-8", "India", "IN"}};
        if (language == "id")
            return {{"id_ID.UTF-8", "Indonesia", "ID"}};
        if (language == "de")
            return {{"de_DE.UTF-8", "Deutschland", "DE"}, {"de_AT.UTF-8", "Österreich", "AT"},
                    {"de_CH.UTF-8", "Schweiz", "CH"}};
        return {{language + "_" + "XX.UTF-8", "Default", "XX"}};
    }

    std::string locale() const override { return m_locale; }
    void setLocale(const std::string& locale) override { m_locale = locale; }

    std::vector<KeyboardLayout> keyboardLayouts(const std::string&) const override
    {
        return {{"us", "English (US)", "qwerty · default", "US"},
                {"us (intl)", "English (US, intl.)", "with dead keys", "US"},
                {"id", "Indonesian", "qwerty", "ID"},
                {"de", "German", "qwertz", "DE"},
                {"fr", "French", "azerty", "FR"},
                {"jp", "Japanese", "106/109 key", "JP"}};
    }
    std::string keyboardLayout() const override { return m_layout; }
    void setKeyboardLayout(const std::string& layout) override { m_layout = layout; }

    std::vector<std::pair<std::string, std::vector<std::string>>> timezones() const override
    {
        return {{"Africa", {"Cairo", "Johannesburg", "Lagos", "Nairobi"}},
                {"America", {"Chicago", "Los_Angeles", "New_York", "Sao_Paulo", "Toronto"}},
                {"Asia", {"Jakarta", "Jayapura", "Makassar", "Pontianak", "Singapore", "Tokyo", "Kolkata"}},
                {"Australia", {"Perth", "Sydney"}},
                {"Europe", {"Amsterdam", "Berlin", "London", "Paris"}}};
    }
    TimeSettings timeSettings() const override { return m_time; }
    void setTimeSettings(const TimeSettings& settings) override { m_time = settings; }

    std::vector<NetworkConnection> connections() const override
    {
        return {{"wired", NetworkConnection::Kind::Wired, "Wired · enp3s0", "Connected · 1 Gbps", 100, false, true},
                {"kantor", NetworkConnection::Kind::Wireless, "Kantor-5G", "WPA2 · secured", 82, true, false},
                {"guest", NetworkConnection::Kind::Wireless, "Biznet-Guest", "Open network", 70, false, false},
                {"iot", NetworkConnection::Kind::Wireless, "Lantai-3-IoT", "WPA3 · secured", 40, true, false},
                {"phone", NetworkConnection::Kind::Hotspot, "iPhone Hotspot", "Personal hotspot", 20, true, false}};
    }
    NetworkDetails details(const std::string&) const override
    {
        return {"10.42.18.106/24", "10.42.18.1", "1.1.1.1, 8.8.8.8", "1000 Mb/s", "e4:5f:01:9a:22:7c"};
    }
    void connect(const std::string&, const std::string&, std::function<void(const std::string&)> done) override
    {
        done({});
    }
    std::string hostname() const override { return m_hostname; }
    void setHostname(const std::string& hostname) override { m_hostname = hostname; }
    bool connectAutomatically() const override { return m_autoconnect; }
    void setConnectAutomatically(bool automatic) override { m_autoconnect = automatic; }
    bool isOnline() const override { return true; }

    std::vector<Disk> disks() const override { return m_disks; }
    void rescan(std::function<void()> done) override { done(); }

    std::vector<LayoutPart> previewLayout(const StoragePlan& plan) const
    {
        uint64_t size = 0;
        for (const Disk& disk : m_disks) {
            if (std::find(plan.disks.begin(), plan.disks.end(), disk.id) != plan.disks.end())
                size += plan.existing == StoragePlan::ExistingData::Alongside ? disk.size - plan.shrinkTo : disk.size;
        }
        if (!size)
            return {};
        const uint64_t root = std::min<uint64_t>(80 * GB, size * 7 / 10);
        const uint64_t swap = std::min<uint64_t>(16 * GB, size / 10);
        const uint64_t boot = GB;
        const uint64_t efi = 6 * GB / 10;
        const uint64_t home = size > root + swap + boot + efi ? size - root - swap - boot - efi : 0;
        std::vector<LayoutPart> parts{{"/boot/efi", efi, "efi"}, {"/boot", boot, "ext4"}, {"/", root, "btrfs"}};
        if (home)
            parts.push_back({"/home", home, "btrfs"});
        parts.push_back({"swap", swap, "swap"});
        return parts;
    }
    void planStorage(const StoragePlan& plan,
                     std::function<void(const std::vector<LayoutPart>&, const ValidationReport&)> done) override
    {
        done(previewLayout(plan), {});
    }
    uint64_t minimumSize(const std::string&) const override { return 120 * GB; }
    void applyStorage(const StoragePlan& plan, const std::string&, std::function<void(const ValidationReport&)> done) override
    {
        m_plan = plan;
        done({});
    }
    std::vector<LayoutPart> appliedLayout() const override { return previewLayout(m_plan); }
    uint64_t requiredSpace() const override { return 9 * GB; }

    void beginCustom(const std::vector<std::string>& disks) override { m_customDisks = disks; }
    std::vector<Disk> customDisks() const override
    {
        std::vector<Disk> out;
        for (const Disk& disk : m_disks) {
            if (std::find(m_customDisks.begin(), m_customDisks.end(), disk.id) != m_customDisks.end())
                out.push_back(disk);
        }
        return out;
    }
    std::string addMount(const MountRequest&) override { return {}; }
    std::string changeMount(const MountRequest&) override { return {}; }
    std::string resize(const std::string&, uint64_t) override { return {}; }
    std::string remove(const std::string&) override { return {}; }
    std::string createRecommended(bool) override { return {}; }
    void resetCustom() override {}
    std::vector<PendingAction> pendingActions() const override { return {}; }
    std::vector<std::string> fileSystems() const override { return {"btrfs", "ext4", "xfs", "vfat", "swap"}; }
    std::vector<std::string> suggestedMountPoints() const override
    {
        return {"/", "/home", "/boot", "/boot/efi", "swap", "/var", "/srv", "/opt"};
    }
    std::vector<std::string> deviceTypes() const override { return {"partition", "lvm", "lvm-thin", "btrfs"}; }
    std::pair<uint64_t, uint64_t> sizeLimits(const std::string&) const override { return {GB, 500 * GB}; }

    std::vector<SoftwareEnvironment> environments() const override
    {
        return {{"bayu", "Bayu Desktop", "The desktop on this media: Files, Terminal, Settings", "monitor", 8 * GB, true},
                {"server-gui", "Server with GUI", "Headless services plus a graphical console", "layout-grid",
                 6 * GB, false},
                {"server", "Server", "Command line only, tuned for production", "server", 2800000000ull, false},
                {"minimal", "Minimal Install", "Just enough to boot and manage remotely", "box", 1200000000ull, false}};
    }
    std::vector<SoftwareAddon> addons(const std::string&) const override
    {
        return {{"development", "Development Tools", "gcc, make, git, headers", true},
                {"container-management", "Container Tools", "podman, buildah, skopeo", true},
                {"headless-management", "Headless Management", "cockpit, realmd", false},
                {"office-suite", "Office Suite", "LibreOffice, fonts", true},
                {"multimedia", "Multimedia", "GStreamer plugins, ffmpeg", true},
                {"rpm-development-tools", "RPM Development", "rpmbuild, mock", false}};
    }
    void setSoftware(const std::string&, const std::vector<std::string>& addons) override { m_addons = addons; }
    SoftwareSummary softwareSummary() const override
    {
        return {1842, 2100000000ull, 8400000000ull, 6};
    }

    void setAccount(const Account&) override {}

    void install(std::function<void(const InstallProgress&)> progress, std::function<void(const InstallLogLine&)> log,
                 std::function<void(const InstallResult&)> done) override
    {
        m_step = 0;
        m_timer = std::make_unique<bayu::base::Timer>();
        m_timer->setInterval(std::chrono::milliseconds(300));
        static const char* lines[] = {"Partition table written to /dev/nvme0n1", "LUKS2 container opened as luks-9f3c",
                                      "btrfs subvolumes @, @home, @log created",
                                      "Installation image copied (8.4 GB)", "kernel-6.12.0-211.68.1 installed",
                                      "Boot loader installed", "User ibnu created"};
        m_timer->onTimeout = [this, progress, log, done] {
            ++m_step;
            const double fraction = std::min(1.0, m_step / 40.0);
            progress({fraction, "Installing 864 of 1,842 · bayu-desktop-0.1.0-1.el10.x86_64",
                      int((1 - fraction) * 190)});
            if (m_step % 6 == 0 && m_step / 6 <= 7)
                log({InstallLogLine::State::Done, lines[m_step / 6 - 1]});
            if (fraction >= 1) {
                m_timer->stop();
                done({true, {}, 372, 1842, true});
            }
        };
        m_timer->start();
    }
    bool canCancelInstall() const override { return m_step < 5; }
    void cancelInstall() override
    {
        if (m_timer)
            m_timer->stop();
    }
    std::string logText() const override { return "anaconda: sample log\n"; }
    void reboot() override {}
    void quit() override {}

private:
    std::vector<Disk> m_disks;
    std::vector<std::string> m_customDisks;
    std::string m_locale = "en_US.UTF-8";
    std::string m_layout = "us";
    TimeSettings m_time{"Asia/Jakarta", true, false, "pool.ntp.org"};
    std::string m_hostname = "ign-ws01";
    bool m_autoconnect = true;
    StoragePlan m_plan;
    std::vector<std::string> m_addons;
    std::unique_ptr<bayu::base::Timer> m_timer;
    int m_step = 0;
};

} // namespace

std::unique_ptr<Backend> createMockBackend()
{
    return std::make_unique<MockBackend>();
}

} // namespace installer
