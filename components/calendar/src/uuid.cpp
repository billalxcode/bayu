// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "uuid.h"

#include <cstdint>
#include <cstdio>
#include <random>

#include <sys/random.h>

std::string createUuid()
{
    uint8_t bytes[16];
    if (getrandom(bytes, sizeof(bytes), 0) != sizeof(bytes)) {
        std::random_device device;
        for (uint8_t& byte : bytes)
            byte = uint8_t(device());
    }
    bytes[6] = uint8_t((bytes[6] & 0x0f) | 0x40);
    bytes[8] = uint8_t((bytes[8] & 0x3f) | 0x80);

    char text[37];
    std::snprintf(text, sizeof(text), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8],
                  bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return text;
}
