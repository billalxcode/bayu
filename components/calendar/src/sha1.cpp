// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "sha1.h"

#include <cstdint>
#include <cstdio>

static uint32_t rotate(uint32_t value, int bits)
{
    return (value << bits) | (value >> (32 - bits));
}

std::string sha1Hex(std::string_view data)
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    std::string message(data);
    const uint64_t bits = uint64_t(data.size()) * 8;
    message.push_back(char(0x80));
    while (message.size() % 64 != 56)
        message.push_back('\0');
    for (int i = 7; i >= 0; --i)
        message.push_back(char(bits >> (i * 8)));

    for (size_t chunk = 0; chunk < message.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            const auto byte = [&](int j) { return uint32_t(uint8_t(message[chunk + size_t(i) * 4 + size_t(j)])); };
            w[i] = byte(0) << 24 | byte(1) << 16 | byte(2) << 8 | byte(3);
        }
        for (int i = 16; i < 80; ++i)
            w[i] = rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f;
            uint32_t k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            const uint32_t next = rotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotate(b, 30);
            b = a;
            a = next;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }

    char hex[41];
    std::snprintf(hex, sizeof(hex), "%08x%08x%08x%08x%08x", h[0], h[1], h[2], h[3], h[4]);
    return hex;
}
