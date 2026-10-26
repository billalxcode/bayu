// SPDX-License-Identifier: GPL-3.0-or-later
// android-launch: opens an Android app, as the store and the app menu's
// lapis entries do. When Android runs, `lapis app launch` at once; when it
// was turned off, a dialog while it starts again; when lapis is not set up,
// the store's settings, where it is turned on.
//   android-launch <package>

#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "base/desktop_entry.h"
#include "base/i18n.h"
#include "base/paths.h"
#include "base/process.h"
#include "base/thread_pool.h"
#include "lapis.h"
#include "ui/application.h"
#include "ui/dialog_window.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/progress_bar.h"
#include "ui/style.h"
#include "ui/widgets.h"

using namespace bayu;
using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;
using store::style::Role;

namespace {

// The app's name, from the menu entry lapis wrote for it.
std::string appName(const std::string& package)
{
    const std::string path = base::paths::dataHome() + "/applications/lapis." + package + ".desktop";
    std::ifstream in(path);
    std::stringstream contents;
    contents << in.rdbuf();
    const std::optional<base::DesktopEntry> entry =
            base::DesktopEntry::parse("lapis." + package + ".desktop", path, contents.str());
    return entry && !entry->name.empty() ? entry->name : package;
}

int launchNow(const std::string& package)
{
    execlp("lapis", "lapis", "app", "launch", package.c_str(), nullptr);
    perror("lapis");
    return 127;
}

// The dialog while Android starts, then the app's launch.
class Starting
{
public:
    explicit Starting(std::string package)
        : m_package(std::move(package))
        , m_dialog(nullptr, tr("Starting Android"), 380)
    {
        auto content = std::make_unique<Column>();
        content->setSpacing(14);
        content->setMargins({24, 4, 24, 24});
        Row* row = content->addChild<Row>();
        row->setSpacing(14);
        m_spinner = row->addChild<BusyIndicator>();
        m_spinner->setRunning(true);
        m_text = store::addText(row,
                                format(tr("Opening {name}. Android was off and starts again; the first start takes "
                                          "a minute."),
                                       {{"name", appName(m_package)}}),
                                14, 400, Role::SecondaryText);
        m_text->setWrap(true);
        m_text->setLayoutHints({.fillWidth = true});
        m_dialog.setContentView(std::move(content));
        m_dialog.onClosed = [] { Application::instance()->quit(1); };
    }

    void start()
    {
        m_dialog.show();
        m_job = base::runInThread([] { return store::lapis::startSession(std::chrono::minutes(3)); },
                                  [this](std::string error) {
                                      if (!error.empty()) {
                                          failed(error);
                                          return;
                                      }
                                      launch();
                                  });
    }

private:
    void launch()
    {
        m_launch = std::make_unique<base::Process>();
        m_launch->onFinished = [this](int exitCode, bool crashed) {
            if (exitCode == 0 && !crashed) {
                Application::instance()->quit(0);
                return;
            }
            std::string errors(m_launch->standardError());
            while (!errors.empty() && errors.back() == '\n')
                errors.pop_back();
            failed(errors.empty() ? std::string(tr("The app did not open.")) : errors.substr(errors.rfind('\n') + 1));
        };
        if (!m_launch->start("lapis", {"app", "launch", m_package}))
            failed(tr("lapis is missing."));
    }

    void failed(const std::string& error)
    {
        m_dialog.setTitle(tr("Android did not start"));
        m_spinner->setRunning(false);
        m_spinner->setVisible(false);
        m_text->setText(error);
        m_text->setColor(store::style::color(Role::Danger));
    }

    std::string m_package;
    DialogWindow m_dialog;
    BusyIndicator* m_spinner;
    Label* m_text;
    base::Job m_job;
    std::unique_ptr<base::Process> m_launch;
};

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2 || argv[1][0] == '-') {
        fprintf(stderr, "Usage: %s <package>\nOpens an Android app, starting Android when it is off.\n", argv[0]);
        return 2;
    }
    const std::string package = argv[1];
    switch (store::lapis::state()) {
    case store::lapis::State::Running:
    case store::lapis::State::Frozen:
        return launchNow(package);
    case store::lapis::State::Missing:
        // Turned on in the store, which shows how its setup goes.
        execlp("bayu-store", "bayu-store", "--settings", nullptr);
        perror("bayu-store");
        return 127;
    case store::lapis::State::Stopped:
        break;
    }

    Application application(argc, argv);
    application.setApplicationId("bayu-store");
    bayu::i18n::setDomain("bayu-store");
    store::style::install();
    Starting starting(package);
    starting.start();
    return application.run();
}
