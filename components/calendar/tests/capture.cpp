// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

// Renders the main window headless over a calendar directory, for comparing
// with the live application: calendar-capture ROOT OUTDIR [SCALE].
#include "calendarmodel.h"
#include "calendarstore.h"
#include "ui/calendarwindow.h"
#include "ui/glyphs.h"

#include <ui/application.h>
#include <ui/image.h>
#include <ui/theme.h>
#include <ui/window.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace bayu::ui;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: calendar-capture ROOT OUTDIR [SCALE]\n");
        return 1;
    }
    const std::string root = argv[1];
    const std::string out = argv[2];
    const double scale = argc > 3 ? std::stod(argv[3]) : 1.5;
    Application application(argc, argv, Application::Platform::Headless);
    registerCalendarGlyphs();
    const std::string settings = out + "/calendar.conf";
    std::filesystem::create_directories(out);
    std::ofstream(settings) << "[General]\ndefaultCalendar=personal\nsidebarVisible=true\nview=month\n";

    CalendarStore store(root);
    CalendarModel model(&store, settings);
    store.load();
    for (const auto& [name, theme] : {std::pair{"light", Theme::light()}, std::pair{"dark", Theme::dark()}}) {
        Theme::setCurrent(theme);
        CalendarWindow window(&model, false);
        window.window()->setScale(scale);
        window.show();
        for (const char* view : {"month", "week", "day", "year"}) {
            window.setView(view);
            window.window()->grab();
            const std::string file = out + "/" + name + "-" + view + ".png";
            if (!window.window()->grab().savePng(file))
                std::fprintf(stderr, "cannot write %s\n", file.c_str());
            if (std::string(name) == "light" && std::string(view) == "month") {
                std::ofstream(out + "/layout-month.txt") << window.window()->layoutDump();
            }
        }
        window.setView("month");
    }
    return 0;
}
