// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// The messages of an interface in the protocol file, in order: their order
// is their opcode, which the compositor's copy must share.
static std::vector<std::string> messages(const std::string& xml, const std::string& interface,
                                         const std::string& kind)
{
    std::vector<std::string> names;
    const size_t start = xml.find("<interface name=\"" + interface + "\"");
    if (start == std::string::npos)
        return names;
    const size_t end = xml.find("</interface>", start);
    const std::string tag = "<" + kind + " name=\"";
    for (size_t at = xml.find(tag, start); at < end; at = xml.find(tag, at + 1)) {
        const size_t from = at + tag.size();
        names.push_back(xml.substr(from, xml.find('"', from) - from));
    }
    return names;
}

// The desktop's trimmed bayu_shell_v1: the requests the toolkit sends
// keep these opcodes.
TEST(ShellProtocolTest, DesktopInterfaceIsTheTrimmedOne)
{
    std::ifstream file(BAYU_TOOLKIT_PROTOCOL_DIR "/bayu-shell-v1.xml");
    ASSERT_TRUE(file.good());
    std::stringstream stream;
    stream << file.rdbuf();
    const std::string xml = stream.str();

    EXPECT_EQ(messages(xml, "bayu_shell_v1", "request"),
              (std::vector<std::string>{"destroy", "get_shell_surface"}));
    EXPECT_EQ(messages(xml, "bayu_shell_surface_v1", "request"),
              (std::vector<std::string>{"destroy", "set_role", "set_focus_policy", "activate", "set_output"}));
    EXPECT_EQ(messages(xml, "bayu_shell_surface_v1", "event"), (std::vector<std::string>{"work_area"}));
}
