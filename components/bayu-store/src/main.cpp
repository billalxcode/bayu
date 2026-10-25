// SPDX-License-Identifier: GPL-3.0-or-later
// bayu-store: apps from Flathub and F-Droid (Android, through lapis).

#include <cstdio>
#include <cstdlib>
#include <string>

#include "backend.h"
#include "base/i18n.h"
#include "ui/application.h"
#include "ui/store_window.h"
#include "ui/style.h"
#include "ui/theme.h"

using namespace store;

static void printHelp(const char* program)
{
    printf("Usage: %s [options]\n"
           "Find and install apps from Flathub and F-Droid.\n"
           "\n"
           "Options:\n"
           "  -h, --help              Displays help on commandline options.\n"
           "  --app=<id>              Open a Flathub app's page\n"
           "  --updates               Open the updates\n"
           "  --mock                  Sample apps instead of the real catalog\n"
           "  --capture=<directory>   With --mock: write the main pages as PNGs, headless\n"
           "  --dark                  The dark theme, for captures\n",
           program);
}

int main(int argc, char** argv)
{
    // An empty DISPLAY (no Xwayland) makes gpgme run `gpg --display=`, which
    // fails every Flatpak signature check.
    if (const char* display = std::getenv("DISPLAY"); display && !*display)
        unsetenv("DISPLAY");
    bool mock = false;
    bool dark = false;
    bool updates = false;
    std::string app;
    std::string capture;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-h" || argument == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (argument == "--mock") {
            mock = true;
        } else if (argument.starts_with("--app=")) {
            app = argument.substr(6);
        } else if (argument == "--updates") {
            updates = true;
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
    application.setApplicationId("bayu-store");
    bayu::i18n::setDomain("bayu-store");
    if (dark)
        bayu::ui::Theme::setCurrent(bayu::ui::Theme::dark());
    style::install();

    std::unique_ptr<Backend> backend = mock || !capture.empty() ? createMockBackend() : createBackend();
    backend->start();
    StoreWindow window(*backend);
    if (!capture.empty())
        return window.capture(capture);
    if (!app.empty())
        window.go({Route::Kind::App, app, Source::Flathub});
    else if (updates)
        window.go({Route::Kind::Library, "updates"});
    window.show();
    return application.run();
}
