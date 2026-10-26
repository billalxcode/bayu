// SPDX-License-Identifier: GPL-3.0-or-later

#include "fdroid.h"

#include <algorithm>
#include <cstdlib>
#include <map>

#include "base/i18n.h"
#include "catalog.h"

using bayu::i18n::trNoop;

namespace store::fdroid {

namespace {

// A JSON reader that calls back for each event instead of building a tree.
class Reader
{
public:
    struct Handler
    {
        virtual ~Handler() = default;
        virtual void begin(bool array) = 0;
        virtual void end(bool array) = 0;
        virtual void key(std::string key) = 0;
        // Strings unescaped; numbers and literals as written.
        virtual void value(std::string value, bool string) = 0;
    };

    Reader(std::string_view text, Handler& handler)
        : m_text(text)
        , m_handler(handler)
    {
    }

    bool run()
    {
        skipSpace();
        if (!parseValue())
            return false;
        skipSpace();
        return m_pos == m_text.size();
    }

private:
    void skipSpace()
    {
        while (m_pos < m_text.size()
               && (m_text[m_pos] == ' ' || m_text[m_pos] == '\n' || m_text[m_pos] == '\r' || m_text[m_pos] == '\t'))
            ++m_pos;
    }

    bool parseValue()
    {
        if (m_pos >= m_text.size())
            return false;
        const char c = m_text[m_pos];
        if (c == '{' || c == '[')
            return parseContainer(c == '[');
        if (c == '"') {
            std::string text;
            if (!parseString(text))
                return false;
            m_handler.value(std::move(text), true);
            return true;
        }
        const size_t start = m_pos;
        while (m_pos < m_text.size() && m_text[m_pos] != ',' && m_text[m_pos] != '}' && m_text[m_pos] != ']'
               && m_text[m_pos] != ' ' && m_text[m_pos] != '\n' && m_text[m_pos] != '\r' && m_text[m_pos] != '\t')
            ++m_pos;
        if (m_pos == start)
            return false;
        m_handler.value(std::string(m_text.substr(start, m_pos - start)), false);
        return true;
    }

    bool parseContainer(bool array)
    {
        ++m_pos;
        m_handler.begin(array);
        skipSpace();
        if (m_pos < m_text.size() && m_text[m_pos] == (array ? ']' : '}')) {
            ++m_pos;
            m_handler.end(array);
            return true;
        }
        while (true) {
            skipSpace();
            if (!array) {
                std::string key;
                if (m_pos >= m_text.size() || m_text[m_pos] != '"' || !parseString(key))
                    return false;
                skipSpace();
                if (m_pos >= m_text.size() || m_text[m_pos] != ':')
                    return false;
                ++m_pos;
                skipSpace();
                m_handler.key(std::move(key));
            }
            if (!parseValue())
                return false;
            skipSpace();
            if (m_pos >= m_text.size())
                return false;
            if (m_text[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_text[m_pos] == (array ? ']' : '}')) {
                ++m_pos;
                m_handler.end(array);
                return true;
            }
            return false;
        }
    }

    static void appendUtf8(std::string& out, uint32_t code)
    {
        if (code < 0x80) {
            out += char(code);
        } else if (code < 0x800) {
            out += char(0xC0 | (code >> 6));
            out += char(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += char(0xE0 | (code >> 12));
            out += char(0x80 | ((code >> 6) & 0x3F));
            out += char(0x80 | (code & 0x3F));
        } else {
            out += char(0xF0 | (code >> 18));
            out += char(0x80 | ((code >> 12) & 0x3F));
            out += char(0x80 | ((code >> 6) & 0x3F));
            out += char(0x80 | (code & 0x3F));
        }
    }

    bool hex4(uint32_t& code)
    {
        if (m_pos + 4 > m_text.size())
            return false;
        code = uint32_t(std::strtoul(std::string(m_text.substr(m_pos, 4)).c_str(), nullptr, 16));
        m_pos += 4;
        return true;
    }

    bool parseString(std::string& out)
    {
        ++m_pos;
        while (m_pos < m_text.size()) {
            const size_t run = m_text.find_first_of("\"\\", m_pos);
            if (run == std::string_view::npos)
                return false;
            out.append(m_text.substr(m_pos, run - m_pos));
            m_pos = run;
            if (m_text[m_pos] == '"') {
                ++m_pos;
                return true;
            }
            if (++m_pos >= m_text.size())
                return false;
            const char e = m_text[m_pos++];
            switch (e) {
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                break;
            case 'b':
            case 'f':
                break;
            case 'u': {
                uint32_t code = 0;
                if (!hex4(code))
                    return false;
                if (code >= 0xD800 && code < 0xDC00 && m_pos + 6 <= m_text.size() && m_text[m_pos] == '\\'
                    && m_text[m_pos + 1] == 'u') {
                    m_pos += 2;
                    uint32_t low = 0;
                    if (!hex4(low))
                        return false;
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                appendUtf8(out, code);
                break;
            }
            default:
                out += e;
            }
        }
        return false;
    }

    std::string_view m_text;
    Handler& m_handler;
    size_t m_pos = 0;
};

using Localized = std::map<std::string, std::string>;

struct Version
{
    int64_t versionCode = 0;
    std::string versionName;
    int64_t added = 0;
    std::string file;
    std::string sha256;
    uint64_t size = 0;
    int minSdk = 1;
    std::vector<std::string> nativecode;
    std::vector<std::string> permissions;
    std::vector<std::string> antiFeatures;
    Localized whatsNew;
};

struct Package
{
    std::string id;
    Localized name;
    Localized summary;
    Localized description;
    Localized icon;
    std::vector<std::string> categories;
    std::string license;
    std::string author;
    std::string website;
    int64_t added = 0;
    int64_t updated = 0;
    std::map<std::string, std::vector<std::string>> screenshots;
    Version best;
    Version bestArm;
};

std::string pick(const Localized& texts, const std::string& locale)
{
    if (texts.empty())
        return {};
    for (const std::string& want : {locale, locale.substr(0, locale.find('-')), std::string("en-US"),
                                    std::string("en")}) {
        const auto it = texts.find(want);
        if (it != texts.end() && !it->second.empty())
            return it->second;
    }
    return texts.begin()->second;
}

Permission permissionFor(const std::string& name)
{
    const std::string short_ = name.substr(name.rfind('.') + 1);
    if (short_ == "INTERNET")
        return {"wifi", trNoop("Network access"), trNoop("Required")};
    if (short_ == "CAMERA")
        return {"camera", trNoop("Camera"), trNoop("Asks")};
    if (short_ == "RECORD_AUDIO")
        return {"mic", trNoop("Microphone"), trNoop("Asks")};
    if (short_ == "ACCESS_FINE_LOCATION" || short_ == "ACCESS_COARSE_LOCATION")
        return {"map-pin", trNoop("Location"), trNoop("Asks")};
    if (short_ == "READ_CONTACTS" || short_ == "WRITE_CONTACTS")
        return {"contact", trNoop("Contacts"), trNoop("Asks")};
    if (short_.starts_with("READ_MEDIA") || short_ == "READ_EXTERNAL_STORAGE" || short_ == "WRITE_EXTERNAL_STORAGE"
        || short_ == "MANAGE_EXTERNAL_STORAGE")
        return {"folder", trNoop("Storage"),
                short_.starts_with("WRITE") || short_.starts_with("MANAGE") ? trNoop("Read & write") : trNoop("Read")};
    if (short_.starts_with("BLUETOOTH"))
        return {"bluetooth", trNoop("Bluetooth"), trNoop("Required")};
    if (short_ == "POST_NOTIFICATIONS")
        return {"bell", trNoop("Notifications"), trNoop("Asks")};
    return {};
}

class Builder : public Reader::Handler
{
public:
    explicit Builder(std::string locale)
        : m_locale(std::move(locale))
    {
    }
    std::vector<App> apps;

    void begin(bool array) override
    {
        // A container is named by its key; an array's element by "[]".
        m_path.push_back(m_pending.empty() && m_path.size() > 0 && m_inArray() ? "[]" : m_pending);
        m_arrays.push_back(array);
        m_pending.clear();
        // packages.<id>
        if (!array && m_path.size() == 3 && m_path[1] == "packages") {
            m_package = Package();
            m_package.id = m_path[2];
        }
        if (!array && m_path.size() == 5 && m_path[3] == "versions" && m_path[1] == "packages")
            m_version = Version();
    }

    void end(bool array) override
    {
        if (m_path.size() == 5 && m_path[3] == "versions" && m_path[1] == "packages" && !array)
            finishVersion();
        if (m_path.size() == 3 && m_path[1] == "packages" && !array)
            finishPackage();
        m_path.pop_back();
        m_arrays.pop_back();
    }

    void key(std::string key) override
    {
        m_pending = std::move(key);
        // Anti-features are keys of an object: their presence counts.
        if (m_path.size() == 6 && m_path[3] == "versions" && m_path[5] == "antiFeatures")
            m_version.antiFeatures.push_back(m_pending);
    }

    void value(std::string value, bool) override
    {
        const std::string k = std::move(m_pending);
        m_pending.clear();
        if (m_path.size() < 4 || m_path[1] != "packages")
            return;
        const size_t depth = m_path.size();
        const std::string& section = m_path[3];
        if (section == "metadata") {
            if (depth == 4) {
                if (k == "license")
                    m_package.license = value;
                else if (k == "authorName")
                    m_package.author = value;
                else if (k == "webSite")
                    m_package.website = value;
                else if (k == "added")
                    m_package.added = std::atoll(value.c_str()) / 1000;
                else if (k == "lastUpdated")
                    m_package.updated = std::atoll(value.c_str()) / 1000;
            } else if (depth == 5) {
                const std::string& field = m_path[4];
                if (field == "name")
                    m_package.name[k] = value;
                else if (field == "summary")
                    m_package.summary[k] = value;
                else if (field == "description")
                    m_package.description[k] = value;
                else if (field == "categories")
                    m_package.categories.push_back(value);
            } else if (depth == 6 && m_path[4] == "icon" && k == "name") {
                m_package.icon[m_path[5]] = value;
            } else if (depth == 8 && m_path[4] == "screenshots" && m_path[5] == "phone" && k == "name") {
                m_package.screenshots[m_path[6]].push_back(value);
            }
        } else if (section == "versions" && depth >= 5) {
            const std::string where = depth >= 6 ? m_path[5] : std::string();
            if (depth == 5 && k == "added") {
                m_version.added = std::atoll(value.c_str()) / 1000;
            } else if (where == "file" && depth == 6) {
                if (k == "name")
                    m_version.file = value;
                else if (k == "sha256")
                    m_version.sha256 = value;
                else if (k == "size")
                    m_version.size = uint64_t(std::atoll(value.c_str()));
            } else if (where == "manifest") {
                if (depth == 6 && k == "versionName")
                    m_version.versionName = value;
                else if (depth == 6 && k == "versionCode")
                    m_version.versionCode = std::atoll(value.c_str());
                else if (depth == 7 && m_path[6] == "usesSdk" && k == "minSdkVersion")
                    m_version.minSdk = std::atoi(value.c_str());
                else if (depth == 7 && m_path[6] == "nativecode")
                    m_version.nativecode.push_back(value);
                else if (depth == 8 && m_path[6] == "usesPermission" && k == "name")
                    m_version.permissions.push_back(value);
            } else if (where == "whatsNew" && depth == 6) {
                m_version.whatsNew[k] = value;
            }
        }
    }

private:
    void finishVersion()
    {
        if (m_version.minSdk > kSdk || m_version.file.empty())
            return;
        const bool native = m_version.nativecode.empty()
                            || std::any_of(m_version.nativecode.begin(), m_version.nativecode.end(),
                                           [](const std::string& abi) { return abi == "x86_64" || abi == "x86"; });
        Version& slot = native ? m_package.best : m_package.bestArm;
        if (m_version.versionCode > slot.versionCode)
            slot = m_version;
    }

    void finishPackage()
    {
        const bool armOnly = m_package.best.file.empty();
        const Version& version = armOnly ? m_package.bestArm : m_package.best;
        if (version.file.empty())
            return;
        App app;
        app.id = m_package.id;
        app.source = Source::FDroid;
        app.name = pick(m_package.name, m_locale);
        if (app.name.empty())
            app.name = app.id;
        app.summary = pick(m_package.summary, m_locale);
        app.description = plainText(pick(m_package.description, m_locale));
        app.developer = m_package.author;
        app.categories = categoriesForFDroid(m_package.categories);
        app.keywords = m_package.categories;
        const std::string icon = pick(m_package.icon, m_locale);
        if (!icon.empty())
            app.iconUrl = std::string(kRepo) + icon;
        app.glyph = "smartphone";
        app.license = m_package.license;
        app.freeLicense = true;
        app.version = version.versionName;
        app.versionCode = version.versionCode;
        app.updated = m_package.updated ? m_package.updated : version.added;
        app.added = m_package.added;
        app.downloadSize = version.size;
        app.installedSize = version.size * 2;
        app.homepage = m_package.website;
        app.apkUrl = std::string(kRepo) + version.file;
        app.apkSha256 = version.sha256;
        app.armOnly = armOnly;
        app.antiFeatures = version.antiFeatures;
        app.releaseNotes = plainText(pick(version.whatsNew, m_locale));
        for (const std::string& name : version.permissions) {
            const Permission permission = permissionFor(name);
            if (permission.name.empty())
                continue;
            if (std::none_of(app.permissions.begin(), app.permissions.end(),
                             [&](const Permission& p) { return p.name == permission.name; }))
                app.permissions.push_back(permission);
        }
        const auto shots = m_package.screenshots.find(m_locale);
        const auto english = m_package.screenshots.find("en-US");
        const auto& list = shots != m_package.screenshots.end() ? shots->second
                           : english != m_package.screenshots.end() ? english->second
                           : m_package.screenshots.empty() ? std::vector<std::string>{}
                                                           : m_package.screenshots.begin()->second;
        const std::string language = shots != m_package.screenshots.end()      ? m_locale
                                     : english != m_package.screenshots.end() ? std::string("en-US")
                                     : m_package.screenshots.empty()           ? std::string()
                                                                               : m_package.screenshots.begin()->first;
        for (size_t i = 0; i < list.size() && i < 4; ++i) {
            // index-v2 names screenshots relative to the package's locale folder.
            const std::string& name = list[i];
            const std::string url = name.starts_with("/") ? std::string(kRepo) + name
                                                          : std::string(kRepo) + "/" + app.id + "/" + language
                                                                + "/phoneScreenshots/" + name;
            app.screenshots.push_back({url, {}, true});
        }
        apps.push_back(std::move(app));
    }

    std::string m_locale;
    std::vector<std::string> m_path;
    std::vector<bool> m_arrays;
    bool m_inArray() const { return !m_arrays.empty() && m_arrays.back(); }
    std::string m_pending;
    Package m_package;
    Version m_version;
};

} // namespace

std::vector<App> parseIndex(const std::string& json, const std::string& locale, std::string* error)
{
    Builder builder(locale);
    Reader reader(json, builder);
    if (!reader.run()) {
        if (error)
            *error = "The F-Droid index could not be read.";
        return {};
    }
    return std::move(builder.apps);
}

} // namespace store::fdroid
