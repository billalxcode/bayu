// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "subscriptions.h"
#include "eventfile.h"
#include "text.h"

#include <base/i18n.h>

#include <algorithm>
#include <cctype>
#include <chrono>

using bayu::i18n::format;
using bayu::i18n::tr;

// How often due calendars are looked for.
static const auto CheckInterval = std::chrono::minutes(15);
// A failed refresh is tried again after this long.
static const int64_t RetrySeconds = 60 * 60;

static std::string lowered(std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
    return result;
}

static std::string hostOf(const std::string& url)
{
    const size_t start = url.find("://");
    if (start == std::string::npos)
        return {};
    const std::string_view rest = std::string_view(url).substr(start + 3);
    std::string_view host = rest.substr(0, rest.find_first_of("/?#"));
    if (const size_t at = host.rfind('@'); at != std::string_view::npos)
        host = host.substr(at + 1);
    if (!host.starts_with('['))
        host = host.substr(0, host.find(':'));
    return std::string(host);
}

Subscriptions::Subscriptions(CalendarStore* store)
    : m_store(store)
    , m_fetch([](const std::string& url, const std::string& etag, const std::string& lastModified) {
        return download(url, etag, lastModified);
    })
{
    now = [] {
        return int64_t(std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count());
    };
    m_timer.setInterval(CheckInterval);
    m_timer.onTimeout = [this] { refreshDue(); };
    m_timer.start();
    m_ready = m_store->readyChanged.connect([this] { refreshDue(); });
    if (m_store->isReady())
        refreshDue();
}

Subscriptions::~Subscriptions() = default;

std::string Subscriptions::normalizedUrl(std::string_view text)
{
    std::string url = trimmed(text);
    if (url.empty() || std::any_of(url.begin(), url.end(), [](unsigned char c) { return std::isspace(c); }))
        return {};

    const size_t separator = url.find("://");
    if (separator == std::string::npos) {
        url = "https://" + url;
    } else {
        const std::string scheme = lowered(std::string_view(url).substr(0, separator));
        const std::string rest = url.substr(separator + 3);
        if (scheme == "webcal" || scheme == "webcals" || scheme == "https")
            url = "https://" + rest;
        else if (scheme == "http")
            url = "http://" + rest;
        else
            return {};
    }
    return hostOf(url).empty() ? std::string() : url;
}

int64_t Subscriptions::refreshInterval(const FeedState& state)
{
    if (state.refreshSeconds <= 0)
        return DefaultRefreshSeconds;
    return std::clamp(state.refreshSeconds, ShortestRefreshSeconds, LongestRefreshSeconds);
}

Subscriptions::Result Subscriptions::fetchFeed(const Fetch& fetch, const std::string& url, const std::string& etag,
                                               const std::string& lastModified)
{
    Result result;
    result.download = fetch(url, etag, lastModified);
    const Download& answer = result.download;
    if (answer.notModified())
        return result;
    if (answer.status == 0) {
        result.error = tr("The calendar could not be reached.");
        return result;
    }
    if (!answer.succeeded()) {
        result.error = format(tr("The server answered with error {status}."),
                              {{"status", std::to_string(answer.status)}});
        return result;
    }
    result.files = EventFile::split(answer.body);
    // A calendar without events is still a calendar.
    if (result.files.empty() && !containsIgnoringCase(answer.body, "BEGIN:VCALENDAR")) {
        result.error = tr("There is no calendar at this address.");
        return result;
    }
    result.info = EventFile::feedInfo(answer.body);
    // Not kept; the events hold it.
    result.download.body.clear();
    return result;
}

int Subscriptions::subscribe(const std::string& url, Rgb color, Subscribed done)
{
    const int number = m_nextSubscription++;
    const std::string address = normalizedUrl(url);
    if (address.empty()) {
        if (done)
            done({}, tr("This is not a web address."));
        return number;
    }

    m_subscribing[number] = bayu::base::runInThread(
        [fetch = m_fetch, address] { return fetchFeed(fetch, address, {}, {}); },
        [this, number, address, color, done](Result result) {
            const auto finished = [this, number] { m_subscribing.erase(number); };
            if (!result.error.empty() || result.download.notModified()) {
                finished();
                if (done)
                    done({}, result.error.empty() ? std::string(tr("The calendar could not be read.")) : result.error);
                return;
            }

            FeedState state;
            state.url = address;
            state.etag = result.download.etag;
            state.lastModified = result.download.lastModified;
            state.fetched = now();
            state.refreshSeconds = result.info.refreshSeconds;
            const std::string name = result.info.name.empty() ? hostOf(address) : result.info.name;
            const std::string id = m_store->createSubscription(name, color, state);
            if (!id.empty())
                m_store->replaceEvents(id, std::move(result.files));
            finished();
            if (done)
                done(id, id.empty() ? std::string(tr("The calendar could not be saved.")) : std::string());
        });
    return number;
}

void Subscriptions::cancelSubscribe(int number)
{
    m_subscribing.erase(number);
}

void Subscriptions::refresh(const std::string& id, Refreshed done)
{
    const FeedState state = m_store->feedState(id);
    if (state.url.empty()) {
        if (done)
            done(tr("This calendar is not a subscription."));
        return;
    }
    if (isRefreshing(id)) {
        // The refresh under way answers for this one too.
        if (done)
            done({});
        return;
    }
    m_refreshing[id] = bayu::base::runInThread(
        [fetch = m_fetch, state] { return fetchFeed(fetch, state.url, state.etag, state.lastModified); },
        [this, id, done](Result result) {
            finishRefresh(id, std::move(result), done);
        });
}

void Subscriptions::finishRefresh(const std::string& id, Result result, const Refreshed& done)
{
    m_refreshing.erase(id);
    // Unsubscribed while it was being fetched.
    if (!m_store->isReadOnly(id))
        return;

    if (!result.error.empty()) {
        m_failed[id] = now();
        if (done)
            done(result.error);
        return;
    }
    m_failed.erase(id);

    FeedState state = m_store->feedState(id);
    state.fetched = now();
    if (!result.download.notModified()) {
        state.etag = result.download.etag;
        state.lastModified = result.download.lastModified;
        state.refreshSeconds = result.info.refreshSeconds;
        m_store->replaceEvents(id, std::move(result.files));
    }
    m_store->setFeedState(id, state);
    if (done)
        done({});
}

bool Subscriptions::isRefreshing(const std::string& id) const
{
    const auto it = m_refreshing.find(id);
    return it != m_refreshing.end() && it->second.isPending();
}

void Subscriptions::refreshDue()
{
    if (!m_store->isReady())
        return;
    const int64_t time = now();
    for (const CalendarInfo& calendar : m_store->calendars()) {
        if (!calendar.isReadOnly() || isRefreshing(calendar.id))
            continue;
        if (const auto failed = m_failed.find(calendar.id);
                failed != m_failed.end() && time - failed->second < RetrySeconds && time >= failed->second)
            continue;
        const FeedState state = m_store->feedState(calendar.id);
        // A clock set back does not hold the refresh off.
        if (state.fetched > 0 && state.fetched <= time && time - state.fetched < refreshInterval(state))
            continue;
        refresh(calendar.id);
    }
}
