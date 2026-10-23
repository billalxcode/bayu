// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/logging.h"

inline const bayu::base::LogCategory& sessionLog()
{
    static const bayu::base::LogCategory category("bayu.session");
    return category;
}
