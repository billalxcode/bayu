// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <chrono>

#include "base/application_registry.h"
#include "base/desktop_entry.h"
#include "base/event_loop.h"

using namespace bayu::base;
using namespace std::chrono_literals;

static constexpr const char* kEntry = R"(# A comment
[Desktop Entry]
Type=Application
Name=Text Editor
Name[de]=Texteditor
Name[sr@latin]=Uređivač
Comment=Edit text\sfiles
Icon=accessories-text-editor
Exec=editor --new-window %F --icon=%i --name "%c" 'quoted arg'
Categories=Utility;TextEditor;
Keywords=text;editor;
NotShowIn=Other;
Actions=new;

[Desktop Action new]
Name=New Window
Exec=editor --new
)";

TEST(DesktopEntryTest, ParsesLocalizedEntries)
{
    const auto entry = DesktopEntry::parse("org.example.editor", "/x/org.example.editor.desktop", kEntry, "de_DE");
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->name, "Texteditor");
    EXPECT_EQ(entry->comment, "Edit text files");
    EXPECT_EQ(entry->categories, (std::vector<std::string>{"Utility", "TextEditor"}));
    ASSERT_EQ(entry->actions.size(), 1u);
    EXPECT_EQ(entry->actions[0].name, "New Window");
    EXPECT_EQ(DesktopEntry::parse("a", "a", kEntry, "sr_RS@latin")->name, "Uređivač");
    EXPECT_EQ(DesktopEntry::parse("a", "a", kEntry, "en_US")->name, "Text Editor");
    EXPECT_TRUE(entry->shouldShow());
    EXPECT_FALSE(entry->shouldShow({"Other"}));

    EXPECT_FALSE(DesktopEntry::parse("a", "a", "[Desktop Entry]\nType=Link\nName=x\n", "en_US"));
    EXPECT_TRUE(DesktopEntry::parse("a", "a", "[Desktop Entry]\nType=Application\nHidden=true\n", "en_US"));
}

TEST(DesktopEntryTest, ExpandsFieldCodes)
{
    const auto entry = DesktopEntry::parse("e", "/x/e.desktop", kEntry, "en_US");
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->command(), (std::vector<std::string>{"editor", "--new-window", "--icon=", "--name",
                                                          "Text Editor", "quoted arg"}));
    EXPECT_EQ(entry->commandForArguments({"/a", "/b"}),
              (std::vector<std::string>{"editor", "--new-window", "/a", "/b", "--icon=", "--name", "Text Editor",
                                        "quoted arg"}));
    EXPECT_EQ(DesktopEntry::parseExec("app %u %i"), std::vector<std::string>{"app"});
}

TEST(ApplicationRegistryTest, ScansAndResolves)
{
    EventLoop loop;
    ApplicationRegistry& registry = ApplicationRegistry::instance(&loop);
    EXPECT_TRUE(registry.isLoading());
    Subscription subscription = registry.changed.connect([&loop] { loop.quit(); });
    const int timer = loop.startTimer(5s, false, [&loop] { loop.quit(); });
    loop.run();
    loop.stopTimer(timer);
    EXPECT_FALSE(registry.isLoading());
    ASSERT_FALSE(registry.entries().empty());

    const auto& first = registry.entries().front();
    EXPECT_EQ(registry.byId(first->id), first);
    EXPECT_EQ(registry.byPath(first->path), first);
    if (const auto settings = registry.byId("bayu-settings")) {
        EXPECT_EQ(registry.resolveWindow("", "", "/usr/bin/bayu-settings", "bayu-settings"), settings);
        EXPECT_EQ(registry.resolveWindow("bayu-settings", "", "", ""), settings);
    }
}

TEST(DesktopEntryTest, KeepsEmptyQuotedArguments)
{
    EXPECT_EQ(DesktopEntry::parseExec(R"(app --title "" '' --next %f)"),
              (std::vector<std::string>{"app", "--title", "", "", "--next"}));
}
