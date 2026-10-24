// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "model.h"
#include "ui/layout.h"

namespace bayu::ui {
class ConfirmDialog;
}

namespace installer {

class Banner;
class Card;
class DiskMap;
class Installer;

// The custom layout editor: each disk drawn as a bar of its partitions and
// free space, one piece selected at a time with what can be done to it
// beside, and the mount points and the changes so far.
class DiskEditor : public bayu::ui::Column
{
public:
    explicit DiskEditor(Installer& installer);
    ~DiskEditor() override;

    // Starts editing the disks of the storage plan.
    void begin(const std::vector<std::string>& disks, bool advanced, bool encrypt);
    // Reads the disks again after a change.
    void reload();
    // The report of the last check, shown under the lists.
    void setReport(const ValidationReport& report);
    // Has the mount points an installation needs.
    bool hasRoot() const;

    std::function<void()> onChanged;

private:
    void select(const std::string& disk, size_t segment);
    void showPanel();
    void showFreePanel(const Disk& disk, const DiskSegment& segment);
    void showPartitionPanel(const Disk& disk, const DiskSegment& segment);
    void showMountForm(const Disk& disk, const DiskSegment& segment);
    void showResizeForm(const DiskSegment& segment);
    void confirmDelete(const DiskSegment& segment);
    void fillLists();
    // Applies an edit: shows its error, or reloads.
    void apply(const std::string& error);

    Installer& m_installer;
    bayu::ui::Column* m_maps;
    Card* m_panel;
    bayu::ui::Column* m_mounts;
    bayu::ui::Column* m_actions;
    Banner* m_report;
    Banner* m_error;
    std::vector<Disk> m_disks;
    std::vector<DiskMap*> m_diskMaps;
    std::string m_selectedDisk;
    size_t m_selectedSegment = 0;
    bool m_advanced = false;
    bool m_encrypt = false;
    std::unique_ptr<bayu::ui::ConfirmDialog> m_confirm;
};

} // namespace installer
