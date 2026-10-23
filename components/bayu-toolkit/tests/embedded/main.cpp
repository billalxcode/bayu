// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "base/event_loop.h"
#include "ui/application.h"

bayu::base::EventLoop* g_hostLoop = nullptr;

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    // As the compositor does: the host's loop exists first and is shared.
    bayu::base::EventLoop hostLoop;
    g_hostLoop = &hostLoop;
    bayu::ui::Application application(argc, argv,
                                          bayu::ui::Application::Platform::Embedded);
    return RUN_ALL_TESTS();
}
