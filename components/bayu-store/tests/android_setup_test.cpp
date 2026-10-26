// SPDX-License-Identifier: GPL-3.0-or-later
// What the store makes of `lapis init`'s output, line by line.

#include <cstdio>
#include <string>

#include "android_setup.h"

namespace {

int failures = 0;

void expect(bool ok, const char* what)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

} // namespace

int main()
{
    using store::AndroidSetup;
    constexpr uint64_t MiB = uint64_t(1) << 20;

    AndroidSetup state;
    state.stage = AndroidSetup::Stage::Authorizing;
    store::readSetupLine("Fetching the image list", state);
    expect(state.stage == AndroidSetup::Stage::Preparing, "any output means the password was given");

    store::readSetupLine("Downloading lineage-20.0-20250809-MAINLINE-waydroid_x86_64-system.zip (1003 MiB)", state);
    expect(state.stage == AndroidSetup::Stage::Downloading, "Downloading starts the download");
    expect(state.image == "system", "the system image");
    expect(state.bytesTotal == 1003 * MiB, "its size");
    expect(state.progress == 0, "from the start");

    store::readSetupLine("  620 / 1003 MiB (61%)", state);
    expect(state.bytesDone == 620 * MiB, "bytes done");
    expect(state.progress > 0.61 && state.progress < 0.62, "the fraction");

    store::readSetupLine("  download interrupted or too slow; retrying with another mirror (1/10)", state);
    expect(state.bytesDone == 620 * MiB, "a retry keeps the progress");

    store::readSetupLine("Resuming at 640 MiB", state);
    expect(state.bytesDone == 640 * MiB, "resuming");

    store::readSetupLine("  1003 MiB done", state);
    expect(state.bytesDone == 1003 * MiB && state.progress == 1, "done");

    store::readSetupLine("Extracting system.img", state);
    expect(state.stage == AndroidSetup::Stage::Extracting && state.image == "system", "extracting");
    expect(state.progress < 0, "no fraction while extracting");

    store::readSetupLine("Downloading lineage-20.0-20250809-MAINLINE-waydroid_x86_64-vendor.zip (180 MiB)", state);
    expect(state.image == "vendor" && state.bytesTotal == 180 * MiB && state.bytesDone == 0, "the vendor image");

    store::readSetupLine("  42 MiB", state);
    expect(state.bytesDone == 42 * MiB, "a size without a total keeps the total");

    store::readSetupLine("vendor image is up to date", state);
    store::readSetupLine("", state);
    expect(state.stage == AndroidSetup::Stage::Downloading, "other lines change nothing");

    if (failures)
        return 1;
    printf("ok\n");
    return 0;
}
