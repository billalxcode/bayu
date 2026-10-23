// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "holidayfeeds.h"
#include "localedata.h"
#include "text.h"

#include <algorithm>
#include <cctype>

namespace {

struct Feed
{
    const char* region;
    const char* language;
    const char* name;
};

}

// The published feeds and the names they give themselves, checked 2026-10-18.
static const std::vector<Feed>& feeds()
{
    static const std::vector<Feed> list{
        {"AE", "ar", "أعياد الإمارات العربية المتحدة"},
        {"AT", "de", "Österreichische Feiertage"},
        {"AU", "en", "Australian Holidays"},
        {"BE", "fr", "Jours fériés belges"},
        {"BE", "nl", "Belgische feestdagen"},
        {"BG", "bg", "Български празници"},
        {"BR", "pt", "Feriados"},
        {"CA", "en", "Canadian Holidays"},
        {"CA", "fr", "Jours fériés canadiens"},
        {"CH", "de", "Schweizerische Feiertage"},
        {"CH", "fr", "Jours fériés suisses"},
        {"CH", "it", "Festività svizzere"},
        {"CN", "zh", "中国大陆节假日"},
        {"CO", "es", "Festivos de Colombia"},
        {"CZ", "cs", "České svátky"},
        {"DE", "de", "Deutsche Feiertage"},
        {"DK", "da", "Mærkedagskalender"},
        {"ES", "ca", "Festius a Espanya"},
        {"ES", "es", "Festivos en España"},
        {"FI", "fi", "Suomen juhlapäivät"},
        {"FR", "fr", "Fêtes (France)"},
        {"GB", "en", "UK Holidays"},
        {"GR", "el", "Ελληνικές γιορτές/αργίες"},
        {"HK", "en", "Hong Kong Holidays"},
        {"HK", "zh", "香港節日"},
        {"HR", "hr", "Hrvatski praznici"},
        {"HU", "hu", "Magyar ünnepnapok"},
        {"ID", "id", "Hari Libur Indonesia"},
        {"IL", "he", "חגים בישראל"},
        {"IN", "en", "India Holidays"},
        {"IN", "hi", "भारतीय अवकाश कैलेंडर"},
        {"IT", "it", "Festività italiane"},
        {"JP", "en", "Japan Holidays"},
        {"JP", "ja", "日本の祝日"},
        {"KR", "ko", "대한민국 공휴일"},
        {"KZ", "kk", "Қазақстан мерекелері"},
        {"MX", "en", "Mexican Holidays"},
        {"MX", "es", "Días festivos (México)"},
        {"MY", "en", "Holidays in Malaysia"},
        {"MY", "ms", "Cuti Malaysia"},
        {"NL", "nl", "Nederlandse feestdagen"},
        {"NO", "nb", "Merkedagskalender"},
        {"NO", "no", "Norske merkedager"},
        {"NZ", "en", "New Zealand Holidays"},
        {"PH", "en", "Philippines Holidays"},
        {"PL", "pl", "Święta w Polsce"},
        {"PT", "pt", "Feriados"},
        {"RO", "ro", "Sărbători în România"},
        {"RU", "ru", "Праздники России"},
        {"SA", "ar", "أعياد المملكة العربية السعودية"},
        {"SE", "sv", "Svenska helgdagar"},
        {"SG", "en", "Holidays in Singapore"},
        {"SG", "zh", "新加坡节假日"},
        {"SK", "sk", "Sviatky na Slovensku"},
        {"TH", "th", "วันหยุดไทย"},
        {"TR", "tr", "Tatiller (Türkiye)"},
        {"TW", "en", "Taiwan Holidays"},
        {"TW", "zh", "台灣節日"},
        {"UA", "uk", "Свята в Україні"},
        {"US", "en", "US Holidays"},
        {"US", "es", "Días festivos (EE.UU.)"},
        {"VN", "vi", "Ngày lễ Việt Nam"},
    };
    return list;
}

std::vector<HolidayFeed> HolidayFeeds::list(const std::string& language, const std::string& region)
{
    std::vector<HolidayFeed> list;
    for (const Feed& feed : feeds()) {
        std::string code = feed.region;
        std::transform(code.begin(), code.end(), code.begin(), [](unsigned char c) { return std::tolower(c); });
        list.push_back({feed.region, feed.language, feed.name,
                        "https://calendars.icloud.com/holidays/" + code + "_" + feed.language + ".ics"});
    }
    const auto rank = [&](const HolidayFeed& feed) {
        if (feed.region != region)
            return 2;
        return feed.language == language ? 0 : 1;
    };
    std::sort(list.begin(), list.end(), [&](const HolidayFeed& a, const HolidayFeed& b) {
        if (rank(a) != rank(b))
            return rank(a) < rank(b);
        const int order = naturalCompare(a.name, b.name);
        return order != 0 ? order < 0 : a.url < b.url;
    });
    return list;
}

std::vector<HolidayFeed> HolidayFeeds::list()
{
    return list(LocaleData::language(), LocaleData::region());
}
