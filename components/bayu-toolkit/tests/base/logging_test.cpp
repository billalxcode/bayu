// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <sys/prctl.h>
#include <sys/resource.h>

#include <string>
#include <vector>

#include "base/geometry.h"
#include "base/logging.h"

using namespace bayu::base;

namespace {

enum class Mode { Off = 0, On = 3 };

struct Captured
{
    std::string category;
    LogLevel level;
    std::string message;
};

class LoggingTest : public testing::Test
{
protected:
    void SetUp() override
    {
        logging::setHandler([this](const LogCategory& category, LogLevel level, std::string_view message) {
            m_messages.push_back({category.name(), level, std::string(message)});
        });
    }

    void TearDown() override
    {
        logging::setHandler(nullptr);
        logging::setRules({});
    }

    std::vector<Captured> m_messages;
};

} // namespace

TEST_F(LoggingTest, DefaultLevels)
{
    const LogCategory category("test.levels");
    EXPECT_FALSE(category.isEnabled(LogLevel::Debug));
    EXPECT_TRUE(category.isEnabled(LogLevel::Info));
    EXPECT_TRUE(category.isEnabled(LogLevel::Critical));

    const LogCategory quiet("test.quiet", LogLevel::Warning);
    EXPECT_FALSE(quiet.isEnabled(LogLevel::Info));
    EXPECT_TRUE(quiet.isEnabled(LogLevel::Warning));
    EXPECT_TRUE(quiet.isEnabled(LogLevel::Fatal));
}

TEST_F(LoggingTest, StreamSeparatesWithSpaces)
{
    const LogCategory category("test.stream");
    const int* pointer = nullptr;
    CF_INFO(category) << "size" << 3 << 1.5 << true << 'x' << std::string("s") << Rect(1, 2, 3, 4)
                      << Mode::On << pointer;
    ASSERT_EQ(m_messages.size(), 1u);
    EXPECT_EQ(m_messages[0].category, "test.stream");
    EXPECT_EQ(m_messages[0].level, LogLevel::Info);
    EXPECT_EQ(m_messages[0].message, "size 3 1.5 true x s 1,2 3x4 3 0x0");

    CF_WARNING(category).nospace() << "a" << 1 << "b";
    EXPECT_EQ(m_messages.back().message, "a1b");
    EXPECT_EQ(m_messages.back().level, LogLevel::Warning);
}

TEST_F(LoggingTest, FormatAndPrintf)
{
    const LogCategory category("test.format");
    CF_CRITICAL(category).format("{} of {}", 2, "three");
    EXPECT_EQ(m_messages.back().message, "2 of three");
    CF_WARNING(category).printf("Failed to map %d bytes: %s", 64, "No memory");
    EXPECT_EQ(m_messages.back().message, "Failed to map 64 bytes: No memory");
    CF_INFO(category) << "prefix" << std::string_view("x");
    EXPECT_EQ(m_messages.back().message, "prefix x");
}

TEST_F(LoggingTest, DisabledMessagesAreNotEvaluated)
{
    const LogCategory category("test.lazy");
    int evaluated = 0;
    const auto touch = [&] { return ++evaluated; };
    CF_DEBUG(category) << touch();
    EXPECT_EQ(evaluated, 0);
    EXPECT_TRUE(m_messages.empty());
    CF_INFO(category) << touch();
    EXPECT_EQ(evaluated, 1);

    // The macro is one statement inside an unbraced if/else.
    bool other = false;
    if (evaluated == 0)
        CF_INFO(category) << "never";
    else
        other = true;
    EXPECT_TRUE(other);
}

TEST_F(LoggingTest, Rules)
{
    const LogCategory input("bayu.input");
    const LogCategory transitions("bayu.input.transitions");
    const LogCategory drm("bayu.drm", LogLevel::Warning);

    logging::setRules("bayu.input*.debug=true\nbayu.drm=true; bayu.drm.warning=false");
    EXPECT_TRUE(input.isEnabled(LogLevel::Debug));
    EXPECT_TRUE(transitions.isEnabled(LogLevel::Debug));
    EXPECT_TRUE(drm.isEnabled(LogLevel::Debug));
    EXPECT_TRUE(drm.isEnabled(LogLevel::Info));
    EXPECT_FALSE(drm.isEnabled(LogLevel::Warning));
    EXPECT_TRUE(drm.isEnabled(LogLevel::Critical));

    // A category created later gets the rules too.
    const LogCategory later("bayu.input.later");
    EXPECT_TRUE(later.isEnabled(LogLevel::Debug));

    // New rules replace the old ones.
    logging::setRules("*=false\n*.critical=true\nbad rule\n*.debug=maybe");
    EXPECT_FALSE(input.isEnabled(LogLevel::Debug));
    EXPECT_FALSE(input.isEnabled(LogLevel::Warning));
    EXPECT_TRUE(input.isEnabled(LogLevel::Critical));
    EXPECT_TRUE(input.isEnabled(LogLevel::Fatal));

    logging::setRules("*.transitions.debug=true");
    EXPECT_TRUE(transitions.isEnabled(LogLevel::Debug));
    EXPECT_FALSE(input.isEnabled(LogLevel::Debug));
    EXPECT_TRUE(input.isEnabled(LogLevel::Info));

    logging::setRules("*input*.info=false");
    EXPECT_FALSE(transitions.isEnabled(LogLevel::Info));
    EXPECT_TRUE(drm.isEnabled(LogLevel::Warning));
}

TEST_F(LoggingTest, ProgrammaticLevels)
{
    LogCategory category("test.programmatic");
    category.setEnabled(LogLevel::Debug, true);
    EXPECT_TRUE(category.isEnabled(LogLevel::Debug));
    category.setMinimumLevel(LogLevel::Critical);
    EXPECT_FALSE(category.isEnabled(LogLevel::Warning));
    category.setEnabled(LogLevel::Fatal, false);
    EXPECT_TRUE(category.isEnabled(LogLevel::Fatal));
}

TEST_F(LoggingTest, LineFormat)
{
    const LogCategory category("bayu.session");
    EXPECT_EQ(logging::formatLine(category, LogLevel::Info, "started"), "bayu.session: started");
    EXPECT_EQ(logging::formatLine(category, LogLevel::Warning, "x"), "bayu.session: warning: x");
    EXPECT_EQ(logging::formatLine(LogCategory::defaultCategory(), LogLevel::Critical, "y"), "critical: y");
}

static void disableCoreDumps()
{
    const rlimit limit{0, 0};
    setrlimit(RLIMIT_CORE, &limit);

    // A piped core pattern still runs its handler, which records the crash, under a zero limit.
    prctl(PR_SET_DUMPABLE, 0);
}

TEST(LoggingDeathTest, FatalAborts)
{
    const LogCategory category("test.fatal", LogLevel::Critical);
    EXPECT_DEATH(
        {
            disableCoreDumps();
            CF_FATAL(category) << "boom";
        },
        "test.fatal: fatal: boom");
}
