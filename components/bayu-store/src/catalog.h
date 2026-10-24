// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "model.h"

namespace store {

// The store's categories, in the order shown, and how the sources' own
// categories fall into them.
const std::vector<Category>& storeCategories();
// Flathub's main and sub-categories (freedesktop) to the store's.
std::vector<std::string> categoriesForFreedesktop(const std::vector<std::string>& categories);
// F-Droid's categories to the store's.
std::vector<std::string> categoriesForFDroid(const std::vector<std::string>& categories);
// The sub-filter of a store category an app falls in: "Photo", "Audio"...
std::vector<std::string> filtersFor(const App& app);

// Every app of both sources, in memory: a few thousand, searched and sorted
// as asked.
class Catalog
{
public:
    void setApps(Source source, std::vector<App> apps);
    const std::vector<App>& apps() const { return m_apps; }
    const App* find(const std::string& id, Source source) const;
    App* find(const std::string& id, Source source);

    int count(Source source) const;
    std::vector<Category> categories() const;
    Page query(const Query& query) const;
    std::vector<std::string> relatedSearches(const std::string& text) const;
    std::vector<std::pair<std::string, int>> topDevelopers(const Query& query) const;

private:
    std::vector<const App*> matching(const Query& query) const;
    void index();

    std::vector<App> m_apps;
    std::map<std::pair<std::string, int>, size_t> m_byId;
};

// Descriptions' little HTML (paragraphs, lists, entities) as plain text.
std::string plainText(const std::string& html);

// Lower-case ASCII, for matching.
std::string folded(std::string text);

} // namespace store
