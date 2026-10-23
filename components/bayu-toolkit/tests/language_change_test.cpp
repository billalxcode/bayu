// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "ui/application.h"

using namespace bayu::ui;

TEST(LanguageChangeTest, OnlyAChangeNotifies)
{
    Application& application = *Application::instance();
    const char* saved = std::getenv("LANGUAGE");
    const std::string previous = saved ? saved : "";
    int changes = 0;
    const auto subscription = application.languageChanged.connect([&changes] { ++changes; });

    application.setLanguageEnvironment({{"LANGUAGE", "ja:de"}});
    EXPECT_EQ(changes, 1);
    EXPECT_STREQ(std::getenv("LANGUAGE"), "ja:de");
    application.setLanguageEnvironment({{"LANGUAGE", "ja:de"}});
    EXPECT_EQ(changes, 1);

    application.setLanguageEnvironment({{"LANGUAGE", previous}});
    EXPECT_EQ(changes, 2);
    if (previous.empty())
        EXPECT_EQ(std::getenv("LANGUAGE"), nullptr);
}
