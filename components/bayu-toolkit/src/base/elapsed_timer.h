// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <chrono>
#include <cstdint>

namespace bayu::base {

// Time since start() on the monotonic clock. Invalid until started.
class ElapsedTimer
{
public:
    using Clock = std::chrono::steady_clock;

    void start() { m_start = Clock::now(); }
    // Starts again and returns the time elapsed before.
    std::chrono::nanoseconds restart()
    {
        const Clock::time_point now = Clock::now();
        const std::chrono::nanoseconds elapsed = now - m_start;
        m_start = now;
        return elapsed;
    }
    void invalidate() { m_start = {}; }
    bool isValid() const { return m_start != Clock::time_point(); }

    std::chrono::nanoseconds elapsed() const { return Clock::now() - m_start; }
    int64_t elapsedMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed()).count();
    }
    // True when more than `timeout` has passed.
    bool hasExpired(std::chrono::nanoseconds timeout) const { return elapsed() > timeout; }

private:
    Clock::time_point m_start;
};

} // namespace bayu::base
