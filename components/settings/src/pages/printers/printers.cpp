// SPDX-License-Identifier: BSD-3-Clause

#include "pages/printers/printers.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <set>

#include <cups/cups.h>

namespace settings::printers {

namespace {

Printer fromDest(const cups_dest_t& dest)
{
    const auto option = [&dest](const char* name) {
        const char* value = cupsGetOption(name, dest.num_options, dest.options);
        return value ? std::string(value) : std::string();
    };
    Printer printer;
    printer.name = dest.name;
    printer.info = option("printer-info");
    if (printer.info.empty())
        printer.info = printer.name;
    printer.model = option("printer-make-and-model");
    printer.uri = option("device-uri");
    if (const std::string state = option("printer-state"); !state.empty())
        printer.state = std::atoi(state.c_str());
    printer.isDefault = dest.is_default;
    printer.discovered = std::strtoul(option("printer-type").c_str(), nullptr, 10) & CUPS_PRINTER_DISCOVERED;
    return printer;
}

struct Search
{
    std::set<std::string> known;
    std::vector<Printer>* found;
};

int foundDest(void* data, unsigned flags, cups_dest_t* dest)
{
    auto* search = static_cast<Search*>(data);
    if (flags & CUPS_DEST_FLAGS_REMOVED || dest->instance)
        return 1;
    Printer printer = fromDest(*dest);
    // A printer set up already, under its own name or its address.
    if (printer.uri.empty() || search->known.contains(printer.name) || search->known.contains(printer.uri))
        return 1;
    search->known.insert(printer.name);
    search->found->push_back(std::move(printer));
    return 1;
}

std::string quoted(const std::string& argument)
{
    std::string result = "'";
    for (char c : argument)
        result += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return result + "'";
}

} // namespace

Printers read(int searchMs)
{
    Printers printers;
    Search search {.found = &printers.found};

    cups_dest_t* dests = nullptr;
    const int count = cupsGetDests2(CUPS_HTTP_DEFAULT, &dests);
    for (int i = 0; i < count; ++i) {
        if (dests[i].instance)
            continue;
        Printer printer = fromDest(dests[i]);
        // CUPS lists the driverless printers it finds too, to print on
        // without setting them up.
        if (printer.discovered) {
            search.known.insert(printer.name);
            printers.found.push_back(std::move(printer));
            continue;
        }
        cups_job_t* jobs = nullptr;
        printer.jobs = std::max(0, cupsGetJobs2(CUPS_HTTP_DEFAULT, &jobs, printer.name.c_str(), 0,
                                                CUPS_WHICHJOBS_ACTIVE));
        cupsFreeJobs(printer.jobs, jobs);
        search.known.insert(printer.name);
        search.known.insert(printer.uri);
        printers.queues.push_back(std::move(printer));
    }
    cupsFreeDests(count, dests);

    if (searchMs > 0)
        cupsEnumDests(CUPS_DEST_FLAGS_NONE, searchMs, nullptr, 0, 0, foundDest, &search);
    std::ranges::sort(printers.found, {}, &Printer::info);
    return printers;
}

std::string uriForAddress(const std::string& address)
{
    std::string text = address;
    std::erase_if(text, [](unsigned char c) { return std::isspace(c); });
    if (text.empty() || text.find("://") != std::string::npos)
        return text;
    return "ipp://" + text + "/ipp/print";
}

std::string queueName(const std::string& text)
{
    std::string name;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_')
            name += char(c);
        else if (!name.empty() && name.back() != '_')
            name += '_';
    }
    while (!name.empty() && name.back() == '_')
        name.pop_back();
    return name.empty() ? std::string("Printer") : name.substr(0, 127);
}

std::optional<std::string> run(const std::vector<std::string>& command)
{
    std::string line;
    for (const std::string& argument : command)
        line += quoted(argument) + ' ';
    // No terminal to ask a password on: lpadmin fails at once for a user who
    // may not manage printers.
    line += "</dev/null 2>&1";
    FILE* pipe = popen(line.c_str(), "r");
    if (!pipe)
        return std::string("Cannot run ") + command.front();
    std::string output;
    char buffer[512];
    while (const size_t size = fread(buffer, 1, sizeof buffer, pipe))
        output.append(buffer, size);
    if (pclose(pipe) == 0)
        return std::nullopt;
    while (!output.empty() && std::isspace(static_cast<unsigned char>(output.back())))
        output.pop_back();
    return output.empty() ? command.front() + " failed" : output;
}

} // namespace settings::printers
