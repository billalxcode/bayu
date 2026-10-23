// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "calendarstore.h"
#include "download.h"

#include <base/signal.h>
#include <base/thread_pool.h>
#include <base/timer.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

// Calendars published on the web, fetched into read-only calendars of the
// store: when one is added, whenever it is due again, and when asked.
class Subscriptions
{
public:
    using Fetch = std::function<Download(const std::string& url, const std::string& etag,
                                         const std::string& lastModified)>;
    using Subscribed = std::function<void(const std::string& id, const std::string& error)>;
    using Refreshed = std::function<void(const std::string& error)>;

    // Fetched again after this long when the calendar does not say.
    static constexpr int64_t DefaultRefreshSeconds = 24 * 60 * 60;
    // What a calendar may ask for is kept within these.
    static constexpr int64_t ShortestRefreshSeconds = 60 * 60;
    static constexpr int64_t LongestRefreshSeconds = 7 * 24 * 60 * 60;

    explicit Subscriptions(CalendarStore* store);
    ~Subscriptions();

    Subscriptions(const Subscriptions&) = delete;
    Subscriptions& operator=(const Subscriptions&) = delete;

    // Runs on a worker thread.
    void setFetch(Fetch fetch) { m_fetch = std::move(fetch); }

    // The address to fetch: webcal and webcals become https, an address
    // without a scheme gets https. Empty when it is no HTTP address.
    static std::string normalizedUrl(std::string_view text);
    static int64_t refreshInterval(const FeedState& state);

    // Fetches the calendar and adds it under the name it gives itself.
    // done(id, error): the new calendar, or why there is none. The number
    // returned cancels it while it is being fetched.
    int subscribe(const std::string& url, Rgb color, Subscribed done);
    void cancelSubscribe(int number);
    // done(error) is empty once the calendar is up to date.
    void refresh(const std::string& id, Refreshed done = {});
    bool isRefreshing(const std::string& id) const;
    // Refreshes the calendars that are due; runs on its own from time to time.
    void refreshDue();

    // Seconds since the epoch; tests move it.
    std::function<int64_t()> now;

private:
    struct Result
    {
        Download download;
        FeedInfo info;
        std::vector<std::unique_ptr<EventFile>> files;
        std::string error;
    };

    static Result fetchFeed(const Fetch& fetch, const std::string& url, const std::string& etag,
                            const std::string& lastModified);
    void finishRefresh(const std::string& id, Result result, const Refreshed& done);

    CalendarStore* m_store;
    Fetch m_fetch;
    bayu::base::Timer m_timer;
    std::map<std::string, bayu::base::Job> m_refreshing;
    std::map<int, bayu::base::Job> m_subscribing;
    int m_nextSubscription = 0;
    // When a refresh last failed, so it is tried again later and not at every check.
    std::unordered_map<std::string, int64_t> m_failed;
    bayu::base::Subscription m_ready;
};
