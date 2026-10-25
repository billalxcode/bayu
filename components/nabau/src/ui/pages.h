// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <vector>

namespace installer {

class Installer;
class Page;

// The steps, in order.
enum Step : size_t {
    LanguageStep,
    LocalizationStep,
    NetworkStep,
    DestinationStep,
    SoftwareStep,
    AccountStep,
    ReviewStep,
    InstallStep,
};

std::unique_ptr<Page> createLanguagePage(Installer& installer);
std::unique_ptr<Page> createLocalizationPage(Installer& installer);
std::unique_ptr<Page> createNetworkPage(Installer& installer);
std::unique_ptr<Page> createDestinationPage(Installer& installer);
std::unique_ptr<Page> createSoftwarePage(Installer& installer);
std::unique_ptr<Page> createAccountPage(Installer& installer);
std::unique_ptr<Page> createReviewPage(Installer& installer);
std::unique_ptr<Page> createInstallPage(Installer& installer);

inline std::vector<std::unique_ptr<Page>> createPages(Installer& installer)
{
    std::vector<std::unique_ptr<Page>> pages;
    pages.push_back(createLanguagePage(installer));
    // Network before localization: once online, the time zone can come from it.
    pages.push_back(createNetworkPage(installer));
    pages.push_back(createLocalizationPage(installer));
    pages.push_back(createDestinationPage(installer));
    pages.push_back(createSoftwarePage(installer));
    pages.push_back(createAccountPage(installer));
    pages.push_back(createReviewPage(installer));
    pages.push_back(createInstallPage(installer));
    return pages;
}

} // namespace installer
