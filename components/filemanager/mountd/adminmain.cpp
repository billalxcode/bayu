// SPDX-License-Identifier: BSD-3-Clause
// bayu-admin-mount: the whole file system, as root, under
// /run/user/<uid>/bayu-admin for the user pkexec ran this for, and only them,
// so the file manager can open folders as an administrator (issue #98). A GUI
// cannot run as root on Wayland; the file manager stays the user's.
//
//   pkexec /usr/libexec/bayu-admin-mount
//
// Prints "ready <folder>" once mounted, and unmounts when its input closes:
// the file manager holds it open while it runs.

#include "fusemount.h"
#include "localbackend.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

int main()
{
    const char *variable = std::getenv("PKEXEC_UID");
    char *end = nullptr;
    const long uid = variable ? std::strtol(variable, &end, 10) : -1;
    if (getuid() != 0 || uid <= 0 || !end || *end) {
        std::cerr << "bayu-admin-mount: run through pkexec" << std::endl;
        return 2;
    }
    std::signal(SIGPIPE, SIG_IGN);

    const std::string runtime = "/run/user/" + std::to_string(uid);
    struct stat st;
    if (::stat(runtime.c_str(), &st) != 0 || st.st_uid != uid_t(uid)) {
        std::cerr << "bayu-admin-mount: no runtime folder " << runtime << std::endl;
        return 1;
    }
    const std::string mountPoint = runtime + "/bayu-admin";
    // A mount left by an earlier run that ended badly.
    ::umount2(mountPoint.c_str(), MNT_DETACH);

    ::mkdir(mountPoint.c_str(), 0700);
    ::chown(mountPoint.c_str(), uid_t(uid), st.st_gid);

    FuseMount mount(std::make_unique<LocalBackend>("/"), mountPoint, "admin", nullptr);
    // Other users would otherwise be kept out by FUSE, root too: the user is
    // let in, and only them (and root).
    mount.setAccess("allow_other", uid_t(uid));
    std::string message;
    if (!mount.start(&message)) {
        std::cerr << "bayu-admin-mount: " << message << std::endl;
        return 1;
    }
    std::cout << "ready " << mountPoint << std::endl;

    // Until the file manager goes, or closes its end.
    char byte;
    while (::read(STDIN_FILENO, &byte, 1) > 0) {
    }
    mount.stop(true, nullptr);
    return 0;
}
