// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <optional>
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
    // IPP's printer-state: 3 idle, 4 printing, 5 stopped.
    int state = 3;
    int jobs = 0;
    bool isDefault = false;
};

struct Printers
{
    // The printers set up.
    std::vector<Printer> queues;
    // Those on the network, and on USB through ipp-usb, not set up yet.
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

// Runs a CUPS command (lpadmin, lp, lpoptions, cancel): nothing when it
// succeeded, else what it said. Blocks, so off the UI thread.
std::optional<std::string> run(const std::vector<std::string>& command);

} // namespace settings::printers
