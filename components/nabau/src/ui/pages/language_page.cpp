// SPDX-License-Identifier: BSD-3-Clause
// Step 1: the language of the installation and the system, and its regional
// format.

#include <algorithm>
#include <cctype>
#include <ctime>

#include "base/i18n.h"
#include "base/locale.h"
#include "ui/label.h"
#include "ui/page.h"
#include "ui/pages.h"
#include "ui/installer.h"
#include "ui/widgets.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace installer {

namespace {

std::string lower(std::string text)
{
    for (char& c : text)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string upper(std::string text)
{
    for (char& c : text)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

// The currency of a territory, for the preview.
std::string currencySymbol(const std::string& territory)
{
    static const std::pair<const char*, const char*> symbols[] = {
        {"US", "$"},  {"CA", "$"},  {"AU", "$"},  {"NZ", "$"},  {"SG", "$"},  {"GB", "£"},  {"ID", "Rp"},
        {"JP", "¥"},  {"CN", "¥"},  {"IN", "₹"},  {"KR", "₩"},  {"BR", "R$"}, {"RU", "₽"},  {"MY", "RM"},
        {"PH", "₱"},  {"TH", "฿"},  {"VN", "₫"},  {"TR", "₺"},  {"CH", "CHF"}, {"SE", "kr"}, {"NO", "kr"},
        {"DK", "kr"}, {"PL", "zł"}, {"MX", "$"},  {"ZA", "R"},  {"SA", "ر.س"}, {"AE", "د.إ"},
    };
    for (const auto& [code, symbol] : symbols) {
        if (territory == code)
            return symbol;
    }
    static const char* euro[] = {"DE", "FR", "ES", "IT", "NL", "BE", "AT", "PT", "FI", "IE", "GR", "SK", "SI",
                                 "LU", "EE", "LV", "LT", "MT", "CY", "HR"};
    for (const char* code : euro) {
        if (territory == code)
            return "€";
    }
    return "¤";
}

class LanguagePage : public Page
{
public:
    explicit LanguagePage(Installer& installer)
        : Page(installer)
    {
        setSpacing(0);
        Row* body = addChild<Row>();
        body->setSpacing(style::columnSpacing);
        body->setLayoutHints({.fillWidth = true, .fillHeight = true});

        Column* left = body->addChild<Column>();
        left->setSpacing(18);
        left->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.05, .preferredWidth = 0});
        m_search = left->addChild<Field>();
        m_search->setSearch(true);
        m_search->onChanged = [this] { fillLanguages(); };
        m_languages = left->addChild<ListCard>();

        Column* right = body->addChild<Column>();
        right->setSpacing(14);
        right->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});
        Column* formats = right->addChild<Column>();
        formats->setSpacing(10);
        formats->setLayoutHints({.fillWidth = true, .fillHeight = true});
        addSectionLabel(formats, tr("REGIONAL FORMAT"));
        m_locales = formats->addChild<ListCard>();
        m_preview = right->addChild<DetailsCard>();
        m_preview->setHeading("eye", tr("Preview"), {}, true);

        m_all = installer.backend().languages();
        m_search->setPlaceholder(format(tr("Search {n} languages"), {{"n", std::to_string(m_all.size())}}));
        const std::string current = installer.backend().locale();
        m_locale = current.empty() ? "en_US.UTF-8" : current;
        m_language = m_locale.substr(0, m_locale.find('_'));
        fillLanguages();
        fillLocales();
    }

    std::string stepName() const override { return tr("Language"); }
    std::string title() const override { return tr("Welcome. Let's pick your language."); }
    std::string description() const override
    {
        return tr("This language will be used during installation and set as the system default. You can change it "
                  "later in Settings.");
    }
    std::string summary() const override
    {
        const Language* language = find(m_language);
        if (!language)
            return m_locale;
        const size_t underscore = m_locale.find('_');
        std::string territory = underscore == std::string::npos ? std::string() : m_locale.substr(underscore + 1, 2);
        return territory.empty() ? language->englishName : language->englishName + " (" + territory + ")";
    }
    bool canContinue() const override { return !m_locale.empty(); }

    void commit(std::function<void(bool)> done) override
    {
        Choices& choices = installer().choices();
        choices.language = m_language;
        choices.locale = m_locale;
        installer().backend().setLocale(m_locale);
        done(true);
    }

private:
    const Language* find(const std::string& id) const
    {
        for (const Language& language : m_all) {
            if (language.id == id)
                return &language;
        }
        return nullptr;
    }

    void fillLanguages()
    {
        const std::string query = lower(m_search->text());
        m_languages->clearRows();
        ListRow* selected = nullptr;
        for (const Language& language : m_all) {
            if (!query.empty() && lower(language.nativeName).find(query) == std::string::npos
                && lower(language.englishName).find(query) == std::string::npos
                && lower(language.id).find(query) == std::string::npos)
                continue;
            ListRow* row = m_languages->addRow();
            row->setBadge(upper(language.id.substr(0, 2)));
            row->setTitle(language.nativeName);
            row->setDetail(language.territory.empty() ? language.englishName : language.territory);
            const std::string id = language.id;
            row->onClicked = [this, row, id] {
                m_language = id;
                m_languages->select(row);
                m_locale.clear();
                fillLocales();
                changed();
            };
            if (language.id == m_language)
                selected = row;
        }
        m_languages->select(selected);
    }

    void fillLocales()
    {
        m_locales->clearRows();
        m_localeList = installer().backend().locales(m_language);
        ListRow* selected = nullptr;
        for (const Locale& locale : m_localeList) {
            ListRow* row = m_locales->addRow();
            row->setBadge(locale.territoryCode);
            row->setTitle(locale.territoryName);
            row->setDetail(locale.id);
            const std::string id = locale.id;
            row->onClicked = [this, row, id] {
                m_locale = id;
                m_locales->select(row);
                updatePreview();
                changed();
            };
            if (locale.id == m_locale)
                selected = row;
        }
        if (!selected && !m_locales->rows().empty()) {
            selected = m_locales->rows().front();
            m_locale = m_localeList.front().id;
        }
        m_locales->select(selected);
        updatePreview();
    }

    void updatePreview()
    {
        m_preview->clearLines();
        if (m_locale.empty())
            return;
        const std::string name = m_locale.substr(0, m_locale.find('.'));
        const bayu::base::Locale locale(name);
        const bayu::base::DateTime now = bayu::base::DateTime::fromSecondsSinceEpoch(std::time(nullptr));
        std::string territory;
        const size_t underscore = name.find('_');
        if (underscore != std::string::npos)
            territory = name.substr(underscore + 1, 2);
        m_preview->addLine(tr("Date"), locale.toString(now.date, bayu::base::Locale::FormatType::Long));
        m_preview->addLine(tr("Time"), locale.toString(now.time, bayu::base::Locale::FormatType::Short));
        m_preview->addLine(tr("Numbers"), locale.toString(1234.56, 2));
        m_preview->addLine(tr("Currency"), currencySymbol(territory) + locale.toString(1234.56, 2));
    }

    Field* m_search;
    ListCard* m_languages;
    ListCard* m_locales;
    DetailsCard* m_preview;
    std::vector<Language> m_all;
    std::vector<Locale> m_localeList;
    std::string m_language;
    std::string m_locale;
};

} // namespace

std::unique_ptr<Page> createLanguagePage(Installer& installer)
{
    return std::make_unique<LanguagePage>(installer);
}

} // namespace installer
