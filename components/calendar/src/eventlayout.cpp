// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "eventlayout.h"
#include "text.h"

#include <algorithm>

namespace {

struct Placed
{
    int index = 0;
    int first = 0;
    int last = 0;
    int lane = 0;
    bool bar = false;
};

}

static DateTime startOf(const Occurrence& occurrence)
{
    return occurrence.allDay ? DateTime(occurrence.firstDay, Time(0, 0)) : occurrence.start;
}

static int minuteOf(const Time& time)
{
    return time.hour() * 60 + time.minute();
}

EventLayout::Rows EventLayout::layoutRows(const std::vector<Occurrence>& occurrences, Date first, int rows, int lanes,
                                          bool allDayOnly, int columns)
{
    Rows result;
    for (int row = 0; row < rows; ++row) {
        const Date rowStart = first.addDays(int64_t(row) * 7);
        const Date rowEnd = rowStart.addDays(columns - 1);

        std::vector<Placed> items;
        for (int i = 0; i < int(occurrences.size()); ++i) {
            const Occurrence& occurrence = occurrences[i];
            if (allDayOnly && !occurrence.allDay)
                continue;
            if (occurrence.lastDay < rowStart || occurrence.firstDay > rowEnd)
                continue;
            Placed item;
            item.index = i;
            item.first = int(rowStart.daysTo(std::max(occurrence.firstDay, rowStart)));
            item.last = int(rowStart.daysTo(std::min(occurrence.lastDay, rowEnd)));
            item.bar = occurrence.allDay || occurrence.firstDay != occurrence.lastDay;
            items.push_back(item);
        }

        // Bars first, longest first, then the timed events of each day in time order.
        std::stable_sort(items.begin(), items.end(), [&occurrences](const Placed& a, const Placed& b) {
            if (a.bar != b.bar)
                return a.bar;
            if (a.first != b.first)
                return a.first < b.first;
            const Occurrence& x = occurrences[a.index];
            const Occurrence& y = occurrences[b.index];
            if (a.bar && x.lastDay != y.lastDay)
                return x.lastDay > y.lastDay;
            if (x.allDay != y.allDay)
                return x.allDay;
            if (startOf(x) != startOf(y))
                return startOf(x) < startOf(y);
            return localeCompare(x.summary, y.summary) < 0;
        });

        std::vector<std::vector<bool>> taken;
        std::vector<int> needed(columns, 0);
        for (Placed& item : items) {
            int lane = 0;
            for (;; ++lane) {
                if (lane == int(taken.size()))
                    taken.emplace_back(columns, false);
                bool free = true;
                for (int column = item.first; column <= item.last && free; ++column)
                    free = !taken[lane][column];
                if (free)
                    break;
            }
            item.lane = lane;
            for (int column = item.first; column <= item.last; ++column) {
                taken[lane][column] = true;
                needed[column] = std::max(needed[column], lane + 1);
            }
        }

        std::vector<int> visible(columns);
        for (int column = 0; column < columns; ++column) {
            result.lanes = std::max(result.lanes, needed[column]);
            visible[column] = lanes <= 0 || needed[column] <= lanes ? needed[column] : lanes - 1;
        }

        std::vector<int> hidden(columns, 0);
        for (const Placed& item : items) {
            const Occurrence& occurrence = occurrences[item.index];
            int column = item.first;
            while (column <= item.last) {
                if (item.lane >= visible[column]) {
                    ++hidden[column];
                    ++column;
                    continue;
                }
                const int start = column;
                while (column <= item.last && item.lane < visible[column])
                    ++column;
                Bar bar;
                bar.index = item.index;
                bar.row = row;
                bar.column = start;
                bar.span = column - start;
                bar.lane = item.lane;
                bar.continuesBefore = start > item.first || occurrence.firstDay < rowStart;
                bar.continuesAfter = column - 1 < item.last || occurrence.lastDay > rowEnd;
                result.bars.push_back(bar);
            }
        }
        for (int column = 0; column < columns; ++column) {
            if (hidden[column] > 0)
                result.more.push_back({ row, column, hidden[column] });
        }
    }
    return result;
}

std::vector<EventLayout::Block> EventLayout::layoutDays(const std::vector<Occurrence>& occurrences, Date first, int days,
                                                        int minimumMinutes)
{
    std::vector<Block> result;
    for (int column = 0; column < days; ++column) {
        const Date day = first.addDays(column);

        std::vector<Block> blocks;
        for (int i = 0; i < int(occurrences.size()); ++i) {
            const Occurrence& occurrence = occurrences[i];
            if (occurrence.allDay || occurrence.firstDay > day || occurrence.lastDay < day)
                continue;
            Block block;
            block.index = i;
            block.column = column;
            block.continuesBefore = occurrence.start.date() < day;
            block.startMinute = block.continuesBefore ? 0 : minuteOf(occurrence.start.time());
            block.continuesAfter = occurrence.lastDay > day;
            block.endMinute = block.continuesAfter || occurrence.end.date() > day ? 24 * 60
                                                                                   : minuteOf(occurrence.end.time());
            block.endMinute = std::max(block.endMinute, block.startMinute);
            blocks.push_back(block);
        }

        std::stable_sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b) {
            if (a.startMinute != b.startMinute)
                return a.startMinute < b.startMinute;
            return a.endMinute > b.endMinute;
        });

        const auto reach = [minimumMinutes](const Block& block) {
            return std::max(block.endMinute, block.startMinute + minimumMinutes);
        };

        // Blocks that overlap, directly or through others, share the width.
        size_t clusterStart = 0;
        int clusterEnd = -1;
        std::vector<int> slotEnds;
        const auto finish = [&](size_t end) {
            const int slotCount = int(slotEnds.size());
            for (size_t i = clusterStart; i < end; ++i) {
                Block& block = blocks[i];
                block.slotCount = slotCount;
                block.span = 1;
                for (int slot = block.slot + 1; slot < slotCount; ++slot) {
                    bool free = true;
                    for (size_t j = clusterStart; j < end && free; ++j) {
                        const Block& other = blocks[j];
                        free = other.slot != slot || other.startMinute >= reach(block) || reach(other) <= block.startMinute;
                    }
                    if (!free)
                        break;
                    ++block.span;
                }
            }
            slotEnds.clear();
        };

        for (size_t i = 0; i < blocks.size(); ++i) {
            Block& block = blocks[i];
            if (i > clusterStart && block.startMinute >= clusterEnd) {
                finish(i);
                clusterStart = i;
                clusterEnd = -1;
            }
            int slot = 0;
            while (slot < int(slotEnds.size()) && slotEnds[slot] > block.startMinute)
                ++slot;
            if (slot == int(slotEnds.size()))
                slotEnds.push_back(0);
            slotEnds[slot] = reach(block);
            block.slot = slot;
            clusterEnd = std::max(clusterEnd, reach(block));
        }
        finish(blocks.size());

        for (const Block& block : blocks)
            result.push_back(block);
    }
    return result;
}
