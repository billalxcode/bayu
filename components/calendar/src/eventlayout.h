// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "occurrence.h"

#include <vector>

namespace EventLayout
{
struct Bar
{
    int index = 0;
    int row = 0;
    int column = 0;
    int span = 1;
    int lane = 0;
    // The occurrence goes on before or after this piece.
    bool continuesBefore = false;
    bool continuesAfter = false;
};

struct More
{
    int row = 0;
    int column = 0;
    int count = 0;
};

struct Rows
{
    std::vector<Bar> bars;
    std::vector<More> more;
    // Lanes the fullest day needs, before anything is hidden.
    int lanes = 0;
};

// A day with more than `lanes` occurrences shows lanes - 1 and a count of the
// rest; `lanes` <= 0 shows everything.
Rows layoutRows(const std::vector<Occurrence>& occurrences, Date first, int rows, int lanes, bool allDayOnly = false,
                int columns = 7);

struct Block
{
    int index = 0;
    int column = 0;
    int startMinute = 0;
    int endMinute = 0;
    // Side by side with overlapping blocks: its slot out of `slotCount`, and how
    // many free slots to its right it may spread over, itself included.
    int slot = 0;
    int slotCount = 1;
    int span = 1;
    bool continuesBefore = false;
    bool continuesAfter = false;
};

// Blocks shorter than `minimumMinutes` take that much room when overlaps are found.
std::vector<Block> layoutDays(const std::vector<Occurrence>& occurrences, Date first, int days, int minimumMinutes);
}
