// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "backend.h"

#include <string>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <sys/types.h>

struct fuse;

// A backend served at a local folder by its own FUSE session and thread.
class FuseMount
{
public:
    // finished runs on the session's thread once the mount has gone, whoever unmounted it.
    FuseMount(std::unique_ptr<Backend> backend, const std::string &mountPoint, const std::string &source,
              std::function<void(FuseMount *)> finished);
    ~FuseMount();

    // Extra mount options, such as allow_other, and the one user, other than
    // root, whose calls are served; every user's by default.
    void setAccess(std::string options, uid_t user);

    // Mounts and starts serving; false with message when the kernel mount fails.
    bool start(std::string *message);
    // Unmounts unless something has files open there; force detaches it regardless, and the
    // session ends once the last file there is closed.
    bool stop(bool force, std::string *message);

    const std::string &mountPoint() const
    {
        return m_mountPoint;
    }

    // Whether the session ended by itself, as when someone ran fusermount -u.
    bool finished() const;

    Backend *backend() const
    {
        return m_backend.get();
    }

    std::mutex &lock()
    {
        return m_lock;
    }

    bool allows(uid_t user) const
    {
        return m_user == uid_t(-1) || user == 0 || user == m_user;
    }

private:
    std::unique_ptr<Backend> m_backend;
    std::string m_mountPoint;
    std::string m_source;
    std::string m_options;
    uid_t m_user = uid_t(-1);
    struct fuse *m_fuse = nullptr;
    std::thread m_thread;
    std::mutex m_lock;
    std::atomic_bool m_finished = false;
    std::function<void(FuseMount *)> m_onFinished;
};
