// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <filesystem>
#include <unistd.h>

#include "base/file.h"
#include "base/paths.h"

using namespace bayu::base;
using paths::UserDirectory;

// The expectations are the reference results for the same file.
TEST(UserDirectoryTest, ReadsUserDirsFile)
{
    const std::string file = std::filesystem::temp_directory_path() / ("user-dirs-" + std::to_string(getpid()));
    ASSERT_TRUE(file::writeAtomically(file, "XDG_DESKTOP_DIR=\"$HOME\"\n"
                                            "XDG_DOCUMENTS_DIR=\"/srv/docs/\"\n"
                                            "XDG_DOWNLOAD_DIR=$HOME/Dl\n"
                                            "XDG_MUSIC_DIR=\"\"\n"
                                            "XDG_PICTURES_DIR=\"$HOME/Pics\"\r\n"
                                            "XDG_PICTURES_DIR=\"$HOME/Later\"\n"
                                            "XDG_VIDEOS_DIR=\n"
                                            "XDG_TEMPLATES_DIR=\"$HOMEX\"\n"
                                            "  XDG_PUBLICSHARE_DIR=\"/x\"\n"));
    const std::string home = paths::home();
    EXPECT_EQ(paths::userDirectory(UserDirectory::Desktop, file), home);
    EXPECT_EQ(paths::userDirectory(UserDirectory::Documents, file), "/srv/docs");
    EXPECT_EQ(paths::userDirectory(UserDirectory::Downloads, file), home + "/Dl");
    // Two quotes alone are not stripped.
    EXPECT_EQ(paths::userDirectory(UserDirectory::Music, file), "\"\"");
    // The last line counts.
    EXPECT_EQ(paths::userDirectory(UserDirectory::Pictures, file), home + "/Later");
    EXPECT_EQ(paths::userDirectory(UserDirectory::Videos, file), "");
    EXPECT_EQ(paths::userDirectory(UserDirectory::Templates, file), home + "X");
    // A line that does not start with XDG_ is not read.
    EXPECT_EQ(paths::userDirectory(UserDirectory::PublicShare, file), home + "/Public");
    std::filesystem::remove(file);

    EXPECT_EQ(paths::userDirectory(UserDirectory::Pictures, "/nonexistent/user-dirs.dirs"), home + "/Pictures");
    EXPECT_EQ(paths::userDirectory(UserDirectory::Videos, "/nonexistent/user-dirs.dirs"), home + "/Videos");
}

TEST(UserDirectoryTest, DataDirectoriesFollowEnvironment)
{
    const char* saved = getenv("XDG_DATA_DIRS");
    const std::string previous = saved ? saved : "";
    setenv("XDG_DATA_DIRS", "/var/lib/flatpak/exports/share::/usr/share", 1);
    EXPECT_EQ(paths::dataDirectories(), (std::vector<std::string>{"/var/lib/flatpak/exports/share", "/usr/share"}));
    setenv("XDG_DATA_DIRS", "", 1);
    EXPECT_EQ(paths::dataDirectories(), (std::vector<std::string>{"/usr/local/share", "/usr/share"}));
    if (saved)
        setenv("XDG_DATA_DIRS", previous.c_str(), 1);
    else
        unsetenv("XDG_DATA_DIRS");
}
