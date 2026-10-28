// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace settings::printers {

struct Printer
{
    // The CUPS queue's name, or for a printer found, the one CUPS gives it.
    std::string name;
    // What people call it, as it describes itself.
    std::string info;
    std::string model;
    std::string uri;
    // A USB printer's IEEE 1284 device ID, which picks its driver.
    std::string deviceId;
    // IPP's printer-state: 3 idle, 4 printing, 5 stopped.
    int state = 3;
    int jobs = 0;
    bool isDefault = false;
};

struct Printers
{
    // The printers set up.
    std::vector<Printer> queues;
    // Those on the network, and on USB, not set up yet.
    std::vector<Printer> found;
};

// Waits up to `searchMs` for the printers that answer on the network; off the
// UI thread.
Printers read(int searchMs);

// The URI of a printer typed in as a host name or an address (IPP Everywhere's
// usual path), or the URI itself when one is typed in.
std::string uriForAddress(const std::string& address);
// A CUPS queue name from a printer's name or address: letters, digits, "-"
// and "_".
std::string queueName(const std::string& text);
// `name`, or with "_2", "_3"... after it while a queue has it.
std::string uniqueName(const std::string& name, const std::set<std::string>& taken);

// Sets the printer up as a new queue: driverless when it prints so, on the
// network or through ipp-usb; otherwise, a USB printer, with the driver its
// device ID matches best. Nothing when it worked, else why not. Blocks.
std::optional<std::string> add(const Printer& printer);

// Runs a CUPS command (lpadmin, lp, lpoptions, cancel): nothing when it
// succeeded, else what it said. Blocks, so off the UI thread.
std::optional<std::string> run(const std::vector<std::string>& command);

} // namespace settings::printers
