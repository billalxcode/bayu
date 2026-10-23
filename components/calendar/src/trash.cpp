// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "trash.h"

#include <base/paths.h>

#include <cstdio>
#include <ctime>
#include <filesystem>

#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string percentEncoded(const std::string& path)
{
    std::string encoded;
    for (unsigned char c : path) {
        const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'
                || c == '_' || c == '.' || c == '~' || c == '/';
        if (plain) {
            encoded.push_back(char(c));
        } else {
            char escape[4];
            std::snprintf(escape, sizeof(escape), "%%%02X", c);
            encoded += escape;
        }
    }
    return encoded;
}

bool moveToTrash(const std::string& path)
{
    std::error_code error;
    const fs::path source = fs::absolute(path, error);
    if (error || !fs::exists(source, error))
        return false;

    const fs::path trash = fs::path(bayu::base::paths::dataHome()) / "Trash";
    fs::create_directories(trash / "files", error);
    fs::create_directories(trash / "info", error);

    // The info file is created first and exclusively, which claims the name.
    const std::string base = source.filename().string();
    for (int i = 1; i < 10000; ++i) {
        const std::string name = i == 1 ? base : base + " " + std::to_string(i);
        const fs::path info = trash / "info" / (name + ".trashinfo");
        const int fd = ::open(info.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0)
            continue;

        char date[32];
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &local);
        const std::string contents = "[Trash Info]\nPath=" + percentEncoded(source.string()) + "\nDeletionDate="
                + date + "\n";
        const bool written = ::write(fd, contents.data(), contents.size()) == ssize_t(contents.size());
        ::close(fd);

        fs::rename(source, trash / "files" / name, error);
        if (!written || error) {
            fs::remove(info, error);
            return false;
        }
        return true;
    }
    return false;
}
