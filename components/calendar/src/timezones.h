// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "datetime.h"

#include <string_view>

namespace TimeZones
{
// An IANA id, one behind a vendor prefix ("/freeassociation.sourceforge.net/
// Europe/Berlin") or a Windows name; invalid when none is known.
TimeZone fromTzid(std::string_view tzid);

// The system's zone by its IANA id, or the system's local time without one.
TimeZone system();
}
