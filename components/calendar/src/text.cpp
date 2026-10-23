// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "text.h"

#include <base/locale.h>

#include <clocale>
#include <memory>
#include <mutex>

#include <unicode/ucol.h>
#include <unicode/ustring.h>

namespace {

struct CollatorDeleter
{
    void operator()(UCollator* collator) const { ucol_close(collator); }
};

using Collator = std::unique_ptr<UCollator, CollatorDeleter>;

}

// "en_SG.UTF-8@euro" as "en_SG".
static std::string collationLocale()
{
    static const std::string name = [] {
        const char* posix = std::setlocale(LC_COLLATE, "");
        std::string result = posix ? posix : "";
        result = result.substr(0, result.find_first_of(".@"));
        return result.empty() || result == "C" || result == "POSIX" ? std::string("en_US_POSIX") : result;
    }();
    return name;
}

// QCollator's numeric, case-insensitive sorting.
static Collator openCollator()
{
    UErrorCode status = U_ZERO_ERROR;
    Collator collator(ucol_open(collationLocale().c_str(), &status));
    if (U_FAILURE(status))
        return nullptr;
    ucol_setAttribute(collator.get(), UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
    ucol_setStrength(collator.get(), UCOL_SECONDARY);
    return collator;
}

static int compareWith(UCollator* collator, std::string_view a, std::string_view b)
{
    if (!collator)
        return a.compare(b);
    UErrorCode status = U_ZERO_ERROR;
    const UCollationResult result = ucol_strcollUTF8(collator, a.data(), int32_t(a.size()), b.data(),
                                                     int32_t(b.size()), &status);
    if (U_FAILURE(status))
        return a.compare(b);
    return result == UCOL_LESS ? -1 : result == UCOL_GREATER ? 1 : 0;
}

int localeCompare(std::string_view a, std::string_view b)
{
    return bayu::base::Locale::system().compare(a, b);
}

int naturalCompare(std::string_view a, std::string_view b)
{
    // Collators are not shared between threads.
    thread_local Collator collator = openCollator();
    return compareWith(collator.get(), a, b);
}

static std::u16string folded(std::string_view text)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = 0;
    u_strFromUTF8(nullptr, 0, &length, text.data(), int32_t(text.size()), &status);
    std::u16string utf16(size_t(length), u'\0');
    status = U_ZERO_ERROR;
    u_strFromUTF8(utf16.data(), length, nullptr, text.data(), int32_t(text.size()), &status);
    if (U_FAILURE(status))
        return {};

    status = U_ZERO_ERROR;
    const int32_t foldedLength = u_strFoldCase(nullptr, 0, utf16.data(), length, U_FOLD_CASE_DEFAULT, &status);
    std::u16string result(size_t(foldedLength), u'\0');
    status = U_ZERO_ERROR;
    u_strFoldCase(result.data(), foldedLength, utf16.data(), length, U_FOLD_CASE_DEFAULT, &status);
    return U_FAILURE(status) ? std::u16string() : result;
}

bool containsIgnoringCase(std::string_view text, std::string_view needle)
{
    if (needle.empty())
        return true;
    return folded(text).find(folded(needle)) != std::u16string::npos;
}

std::string trimmed(std::string_view text)
{
    const auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && space(text[begin]))
        ++begin;
    while (end > begin && space(text[end - 1]))
        --end;
    return std::string(text.substr(begin, end - begin));
}
