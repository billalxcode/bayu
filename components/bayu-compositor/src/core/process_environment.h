// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_CORE_PROCESS_ENVIRONMENT_H_
#define BAYU_CORE_PROCESS_ENVIRONMENT_H_

#include <string>
#include <vector>

namespace bayu {

// The environment to start a child process from, as NAME=value entries: the
// compositor's current one.
std::vector<std::string> childEnvironment();

} // namespace bayu

#endif // BAYU_CORE_PROCESS_ENVIRONMENT_H_
