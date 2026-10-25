// SPDX-License-Identifier: GPL-3.0-or-later
// nabau: installs the system from the live session, a native
// frontend for Anaconda's installation modules.

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "backend.h"
#include "base/i18n.h"
#include "ui/application.h"
#include "ui/installer.h"
#include "ui/style.h"
#include "ui/theme.h"

using namespace installer;

static void printHelp(const char* program)
{
    printf("Usage: %s [options]\n"
           "Install the system to this computer's disk.\n"
           "\n"
           "Options:\n"
           "  -h, --help              Displays help on commandline options.\n"
           "  --autostart             Starts only on live media, from the session's autostart\n"
           "  --mock                  Sample data instead of the installation backend\n"
           "  --capture=<directory>   With --mock: write every step as a PNG, headless\n"
           "  --dark                  The dark theme, for captures\n",
           program);
}

int main(int argc, char** argv)
{
    bool mock = false;
    bool dark = false;
    std::string capture;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-h" || argument == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (argument == "--autostart") {
            std::stringstream cmdline;
            cmdline << std::ifstream("/proc/cmdline").rdbuf();
            if (cmdline.str().find("rd.live.image") == std::string::npos)
                return 0;
        } else if (argument == "--mock") {
            mock = true;
        } else if (argument == "--dark") {
            dark = true;
        } else if (argument.starts_with("--capture=")) {
            capture = argument.substr(10);
        } else {
            fprintf(stderr, "Unknown option '%s'.\n", argument.c_str());
            return 1;
        }
    }

    bayu::ui::Application application(argc, argv,
                                      capture.empty() ? bayu::ui::Application::Platform::Wayland
                                                      : bayu::ui::Application::Platform::Headless);
    application.setApplicationId("nabau");
    bayu::i18n::setDomain("nabau");
    if (dark)
        bayu::ui::Theme::setCurrent(bayu::ui::Theme::dark());
    style::install();

    Installer installer(mock || !capture.empty() ? createMockBackend() : createAnacondaBackend());
    if (!capture.empty())
        return installer.capture(capture);
    installer.start();
    return application.run();
}
