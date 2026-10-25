// SPDX-License-Identifier: BSD-3-Clause
// Reads an F-Droid index-v2.json and prints what the store makes of it:
//   store-fdroid-test /path/index-v2.json [package...]

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "fdroid.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s index-v2.json [package...]\n", argv[0]);
        return 2;
    }
    std::stringstream buffer;
    buffer << std::ifstream(argv[1], std::ios::binary).rdbuf();
    const auto start = std::chrono::steady_clock::now();
    std::string error;
    const std::vector<store::App> apps = store::fdroid::parseIndex(buffer.str(), "en-US", &error);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    size_t arm = 0;
    for (const store::App& app : apps)
        arm += app.armOnly;
    printf("%zu apps (%zu ARM only) in %.2f s%s%s\n", apps.size(), arm, seconds, error.empty() ? "" : ": ",
           error.c_str());
    for (int i = 2; i < argc; ++i) {
        for (const store::App& app : apps) {
            if (app.id != argv[i])
                continue;
            printf("\n%s  %s (%lld)  %s\n  %s\n  dev=%s license=%s size=%llu arm=%d\n  icon=%s\n  apk=%s %s\n",
                   app.id.c_str(), app.version.c_str(), static_cast<long long>(app.versionCode), app.name.c_str(),
                   app.summary.c_str(), app.developer.c_str(), app.license.c_str(),
                   static_cast<unsigned long long>(app.downloadSize), int(app.armOnly), app.iconUrl.c_str(),
                   app.apkUrl.c_str(), app.apkSha256.c_str());
            printf("  categories:");
            for (const std::string& c : app.categories)
                printf(" %s", c.c_str());
            printf("\n  permissions:");
            for (const store::Permission& p : app.permissions)
                printf(" [%s]", p.name.c_str());
            printf("\n  anti-features:");
            for (const std::string& a : app.antiFeatures)
                printf(" %s", a.c_str());
            printf("\n  screenshots: %zu %s\n  notes: %.80s\n  description: %.160s\n", app.screenshots.size(),
                   app.screenshots.empty() ? "" : app.screenshots[0].url.c_str(), app.releaseNotes.c_str(),
                   app.description.c_str());
        }
    }
    return apps.empty() ? 1 : 0;
}
