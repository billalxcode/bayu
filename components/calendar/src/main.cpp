// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "calendarmodel.h"
#include "calendarstore.h"
#include "ui/calendarwindow.h"
#include "ui/glyphs.h"

#include <base/command_line.h>
#include <base/dbus.h>
#include <base/i18n.h>
#include <base/url.h>
#include <ui/application.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dbus = bayu::base::dbus;

int main(int argc, char** argv)
{
    bayu::base::CommandLineParser parser;
    parser.setApplicationDescription("Bayu Calendar");
    parser.setApplicationVersion(CALENDAR_VERSION);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"date", "Show this day.", "YYYY-MM-DD"});
    parser.addPositionalArgument("files", "Calendar files to import.", "[FILE...]");
    parser.process(argc, argv);

    std::vector<std::string> files;
    for (const std::string& argument : parser.positionalArguments()) {
        // Local files as absolute paths, other URLs as they are.
        std::string path = argument;
        if (argument.find("://") != std::string::npos) {
            const std::optional<std::string> local = bayu::base::url::toLocalFile(argument);
            if (!local) {
                files.push_back(argument);
                continue;
            }
            path = *local;
        }
        std::error_code error;
        files.push_back(std::filesystem::absolute(path, error).lexically_normal().string());
    }
    const std::string date = parser.value("date");

    bayu::ui::Application application(argc, argv);
    application.setApplicationId("bayu-calendar");
    bayu::i18n::setDomain("bayu-calendar");

    // A running calendar takes the files and the day, and this process ends.
    dbus::Connection* bus = application.sessionBus();
    if (bus && !bus->requestName("org.bayu.Calendar")) {
        dbus::Proxy existing(bus, "org.bayu.Calendar", "/Calendar", "org.bayu.Calendar");
        existing.setTimeout(std::chrono::seconds(5));
        existing.call("open", files, date);
        return 0;
    }

    registerCalendarGlyphs();

    CalendarStore store;
    CalendarModel model(&store);
    std::unique_ptr<dbus::Object> object;
    if (bus) {
        object = std::make_unique<dbus::Object>(bus, "/Calendar", "org.bayu.Calendar");
        object->addMethod("activateWindow", [&model] { model.activateWindow(); });
        object->addMethod("open", [&model](const std::vector<std::string>& files, const std::string& date) {
            model.open(files, date);
        });
        object->publish();
    }

    CalendarWindow window(&model);
    window.show();

    // Imports wait for the calendars to be read.
    bayu::base::Subscription ready;
    if (!files.empty() || !date.empty()) {
        ready = model.readyChanged.connect([&model, &ready, files, date] {
            model.open(files, date);
            ready.reset();
        });
    }
    store.loadAsync();
    return application.run();
}
