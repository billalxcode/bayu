// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "log.h"

namespace services {

const bayu::base::LogCategory &log()
{
    static const bayu::base::LogCategory category("bayu.services");
    return category;
}

} // namespace services
