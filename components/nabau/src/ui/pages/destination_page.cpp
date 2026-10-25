// SPDX-License-Identifier: GPL-3.0-or-later
// Step 4: the disk to install to and how it is laid out, automatically or in
// the disk editor.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "base/i18n.h"
#include "base/timer.h"
#include "ui/check_box.h"
#include "ui/dialog_window.h"
#include "ui/label.h"
#include "ui/scroll_view.h"
#include "ui/slider.h"
#include "ui/disk_editor.h"
#include "ui/page.h"
#include "ui/pages.h"
#include "ui/installer.h"
#include "ui/widgets.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace installer {

using style::Role;

namespace {

constexpr uint64_t GB = 1000ull * 1000 * 1000;

// What a disk holds, for its card: "Windows 11 detected", "empty".
std::string diskStatus(const Disk& disk)
{
    if (disk.installationMedia)
        return tr("installation media");
    if (!disk.systems.empty())
        return format(tr("{system} detected"), {{"system", disk.systems.front()}});
    size_t partitions = 0;
    for (const DiskSegment& segment : disk.segments)
        partitions += segment.free ? 0 : 1;
    if (partitions == 0)
        return tr("empty");
    return bayu::i18n::trn("{n} partition", "{n} partitions", partitions);
}

// The partition to make room from: the largest one with data.
// The partition to shrink for an installation beside it: Windows' when
// there is one, else the largest.
const DiskSegment* largestPartition(const Disk& disk)
{
    const DiskSegment* best = nullptr;
    for (const DiskSegment& segment : disk.segments) {
        if (segment.free || segment.format == "efi" || segment.format == "swap" || segment.format.empty())
            continue;
        const bool windows = segment.format == "ntfs";
        const bool bestWindows = best && best->format == "ntfs";
        if (!best || windows > bestWindows || (windows == bestWindows && segment.size > best->size))
            best = &segment;
    }
    return best;
}

// What to do with the data already on the chosen disk.
class ExistingDataCard : public Card
{
public:
    ExistingDataCard()
        : Card(0)
    {
        setMargins({18, 16, 18, 16});
        setSpacing(10);
        m_title = addText(this, {}, 13, 600);
        m_replace = addChild<RadioButton>();
        m_alongside = addChild<RadioButton>();
        m_split = addChild<Column>();
        m_split->setSpacing(6);
        m_split->setLayoutHints({.fillWidth = true, .margins = {28, 0, 0, 4}});
        m_slider = m_split->addChild<Slider>();
        m_slider->setLayoutHints({.fillWidth = true});
        m_splitText = addText(m_split, {}, 12, 400, Role::SecondaryText);
        m_free = addChild<RadioButton>();
        for (RadioButton* button : {m_replace, m_alongside, m_free})
            button->setGroup(&m_group);
        m_group.onChanged = [this](int index) {
            m_chosen = true;
            m_choice = index == 0   ? StoragePlan::ExistingData::Replace
                       : index == 1 ? StoragePlan::ExistingData::Alongside
                                    : StoragePlan::ExistingData::UseFreeSpace;
            m_split->setVisible(m_choice == StoragePlan::ExistingData::Alongside);
            if (onChanged)
                onChanged();
        };
        m_slider->onMoved = [this](double gb) {
            m_keep = uint64_t(gb * double(GB));
            updateSplitText();
        };
        m_slider->onPressedChanged = [this] {
            if (!m_slider->isPressed() && onChanged)
                onChanged();
        };
    }

    // Shows the choices for a disk; false when it has nothing to keep.
    bool set(const Disk& disk, const std::string& product, uint64_t required, uint64_t minimumKeep)
    {
        m_disk = disk;
        m_product = product;
        const DiskSegment* largest = largestPartition(disk);
        bool hasData = false;
        for (const DiskSegment& segment : disk.segments)
            hasData = hasData || !segment.free;
        if (!hasData)
            return false;
        const std::string system = disk.systems.empty() ? tr("the data on it") : disk.systems.front();
        m_title->setText(format(tr("{disk} already holds data"), {{"disk", disk.model}}));
        m_replace->setText(format(tr("Erase everything on {path}"), {{"path", disk.path}}));
        m_shrink = largest ? largest->device : std::string();
        const bool canShrink = largest && largest->size > minimumKeep + required;
        m_alongside->setText(canShrink ? format(tr("Install alongside {system}"), {{"system", system}})
                                       : format(tr("Install alongside {system} (it cannot be shrunk now)"),
                                                {{"system", system}}));
        m_alongside->setEnabled(canShrink);
        m_free->setText(format(tr("Use the free space only ({size})"), {{"size", style::formatSize(disk.freeSpace)}}));
        m_free->setEnabled(disk.freeSpace >= required);
        if (canShrink) {
            m_slider->setRange(double(minimumKeep) / double(GB), double(largest->size - required) / double(GB));
            m_keep = std::clamp<uint64_t>(largest->size / 2, minimumKeep, largest->size - required);
            m_slider->setValue(double(m_keep) / double(GB));
            m_partitionSize = largest->size;
            m_systemName = system;
        }
        // The safest choice that works: free space, then beside. Erasing is
        // never chosen for the user.
        for (RadioButton* button : {m_replace, m_alongside, m_free})
            button->setChecked(false);
        m_chosen = true;
        if (disk.freeSpace >= required) {
            m_free->setChecked(true);
            m_choice = StoragePlan::ExistingData::UseFreeSpace;
        } else if (canShrink) {
            m_alongside->setChecked(true);
            m_choice = StoragePlan::ExistingData::Alongside;
        } else {
            m_chosen = false;
            m_choice = StoragePlan::ExistingData::Replace;
        }
        m_split->setVisible(m_choice == StoragePlan::ExistingData::Alongside);
        updateSplitText();
        return true;
    }

    StoragePlan::ExistingData choice() const { return m_choice; }
    // Something was picked, by the user or as a safe default.
    bool chosen() const { return m_chosen; }
    const std::string& shrinkDevice() const { return m_shrink; }
    uint64_t keepSize() const { return m_keep; }
    std::function<void()> onChanged;

private:
    void updateSplitText()
    {
        m_splitText->setText(format(tr("{system}: {keep}   ·   {product}: {new}"),
                                    {{"system", m_systemName},
                                     {"keep", style::formatSize(m_keep)},
                                     {"product", m_product},
                                     {"new", style::formatSize(m_partitionSize > m_keep ? m_partitionSize - m_keep : 0)}}));
    }

    Label* m_title;
    ButtonGroup m_group;
    RadioButton* m_replace;
    RadioButton* m_alongside;
    RadioButton* m_free;
    bool m_chosen = true;
    Column* m_split;
    Slider* m_slider;
    Label* m_splitText;
    Disk m_disk;
    std::string m_product;
    std::string m_systemName;
    std::string m_shrink;
    uint64_t m_keep = 0;
    uint64_t m_partitionSize = 0;
    StoragePlan::ExistingData m_choice = StoragePlan::ExistingData::Replace;
};

class DestinationPage : public Page
{
public:
    enum class Method {
        Automatic,
        Custom,
        Advanced,
    };

    explicit DestinationPage(Installer& installer)
        : Page(installer)
        , m_editor(new DiskEditor(installer))
    {
        m_choiceView = addChild<Row>();
        m_choiceView->setSpacing(style::columnSpacing);
        m_choiceView->setLayoutHints({.fillWidth = true, .fillHeight = true});
        addChild(std::unique_ptr<View>(m_editor));
        m_editor->setVisible(false);
        m_editor->onChanged = [this] { changed(); };

        ScrollView* leftScroll = m_choiceView->addChild<ScrollView>();
        leftScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
        leftScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.05, .preferredWidth = 0});
        Column* left = leftScroll->setContent<Column>();
        left->setSpacing(10);
        addSectionLabel(left, tr("LOCAL STANDARD DISKS"));
        m_disksColumn = left->addChild<Column>();
        m_disksColumn->setSpacing(10);
        m_disksColumn->setLayoutHints({.fillWidth = true});
        ActionButton* add = left->addChild<ActionButton>(tr("Add iSCSI, FCoE or multipath device"));
        add->setLeadingGlyph("plus");
        add->setLayoutHints({.fillWidth = true});
        add->onClicked = [this] { explainSpecialized(); };
        m_existing = left->addChild<ExistingDataCard>();
        m_existing->onChanged = [this] { replan(); };
        m_existing->setVisible(false);
        m_summary = left->addChild<DetailsCard>();
        m_summary->setLayoutHints({.fillWidth = true, .margins = {0, 8, 0, 0}});

        // Scrolls on short screens (1366×768).
        ScrollView* rightScroll = m_choiceView->addChild<ScrollView>();
        rightScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
        rightScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});
        Column* right = rightScroll->setContent<Column>();
        right->setSpacing(10);
        addSectionLabel(right, tr("STORAGE CONFIGURATION"));
        m_automatic = right->addChild<OptionCard>("wand");
        m_automatic->setTitle(tr("Automatic"));
        m_automatic->setDetail(tr("Laid out for you, safely"));
        m_automatic->setNote(tr("recommended"), true);
        m_custom = right->addChild<OptionCard>("sliders");
        m_custom->setTitle(tr("Custom"));
        m_custom->setDetail(tr("Your own mount points"));
        m_advanced = right->addChild<OptionCard>("terminal");
        m_advanced->setTitle(tr("Advanced"));
        m_advanced->setDetail(tr("LVM or Btrfs volumes, by hand"));
        m_automatic->onClicked = [this] { setMethod(Method::Automatic); };
        m_custom->onClicked = [this] { setMethod(Method::Custom); };
        m_advanced->onClicked = [this] { setMethod(Method::Advanced); };
        m_encrypt = right->addChild<ToggleCard>(tr("Encrypt my data"),
                                                tr("LUKS2 · passphrase at next step"));
        m_encrypt->onToggled = [this](bool on) {
            m_encrypted = on;
            changed();
        };
        right->addStretch();
        Label* layoutLabel = addSectionLabel(right, tr("RESULTING LAYOUT"));
        layoutLabel->setLayoutHints({.margins = {0, 10, 0, 0}});
        m_layout = right->addChild<LayoutBar>();
        m_layoutNote = addText(right, {}, 12, 400, Role::SecondaryText);
        m_layoutNote->setWrap(true);
        m_layoutNote->setLayoutHints({.fillWidth = true});

        m_replan.setSingleShot(true);
        m_replan.onTimeout = [this] { plan(); };
        setMethod(Method::Automatic);
    }

    std::string stepName() const override { return tr("Destination"); }
    std::string title() const override
    {
        return m_editing ? tr("Lay out the disk yourself.") : tr("Where should it live?");
    }
    std::string description() const override
    {
        if (m_editing)
            return tr("Click a part of a disk to use it, resize it or delete it. Changes are only planned here; "
                      "nothing is written until you confirm on the review screen.");
        return tr("Pick a disk, then decide how it is laid out. Nothing is written to any device until you confirm "
                  "on the review screen.");
    }
    std::string summary() const override
    {
        const Disk* disk = selectedDisk();
        if (!disk)
            return {};
        return disk->bus + " · " + style::formatSize(disk->size);
    }
    std::string help() const override
    {
        return tr("Automatic lays the system out for you: a boot partition, the system, your files and swap. If the "
                  "disk already holds Windows or another system, you can install beside it; drag the slider to "
                  "share the space.\n\nCustom opens the disk editor: click free space to add a mount point (at least "
                  "“/”), or click a partition to use it, resize it or delete it. Advanced also offers LVM, RAID and "
                  "Btrfs volumes.\n\nNothing is written to the disks until you begin the installation on the review "
                  "screen.");
    }

    bool canContinue() const override
    {
        if (m_editing)
            return m_editor->hasRoot();
        return selectedDisk() != nullptr && !m_planning && (!m_existing->isVisible() || m_existing->chosen());
    }

    bool handleBack() override
    {
        if (!m_editing)
            return false;
        m_editing = false;
        m_editor->setVisible(false);
        m_choiceView->setVisible(true);
        changed();
        return true;
    }

    void enter() override
    {
        m_required = installer().backend().requiredSpace();
        if (m_disks.empty())
            fillDisks();
        replan();
    }

    void commit(std::function<void(bool)> done) override
    {
        if (!m_editing && m_method != Method::Automatic) {
            // Custom and Advanced continue into the editor.
            m_editing = true;
            m_choiceView->setVisible(false);
            m_editor->setVisible(true);
            m_editor->begin({m_selected}, m_method == Method::Advanced, m_encrypted);
            changed();
            done(false);
            return;
        }
        const StoragePlan plan = currentPlan();
        m_planning = true;
        installer().backend().planStorage(plan, [this, plan, done](const std::vector<LayoutPart>& layout,
                                                                  const ValidationReport& report) {
            m_planning = false;
            if (m_editing)
                m_editor->setReport(report);
            if (!report.ok()) {
                if (!m_editing)
                    showLayout(layout, report);
                done(false);
                return;
            }
            Choices& choices = installer().choices();
            choices.storage = plan;
            choices.layout = layout;
            choices.erasedDisks.clear();
            if (plan.method == StoragePlan::Method::Automatic && plan.existing == StoragePlan::ExistingData::Replace) {
                if (const Disk* disk = selectedDisk())
                    choices.erasedDisks.push_back(disk->path);
            }
            done(true);
        });
    }

private:
    const Disk* selectedDisk() const
    {
        for (const Disk& disk : m_disks) {
            if (disk.id == m_selected)
                return &disk;
        }
        return nullptr;
    }

    void fillDisks()
    {
        m_disks = installer().backend().disks();
        while (!m_disksColumn->children().empty())
            m_disksColumn->takeChild(m_disksColumn->children().back().get());
        m_diskCards.clear();
        // An empty disk first, so that nothing on another has to give way
        // (Windows beside a second, empty drive); then the largest one.
        const auto rank = [](const Disk& disk) {
            const bool empty = std::all_of(disk.segments.begin(), disk.segments.end(),
                                           [](const DiskSegment& segment) { return segment.free; });
            return std::pair(empty, disk.size);
        };
        const Disk* best = nullptr;
        for (const Disk& disk : m_disks) {
            if (!disk.installationMedia && (!best || rank(disk) > rank(*best)))
                best = &disk;
        }
        if (m_selected.empty() && best)
            m_selected = best->id;
        for (const Disk& disk : m_disks) {
            OptionCard* card = m_disksColumn->addChild<OptionCard>(disk.removable ? "usb" : "hard-drive");
            card->setTitle(disk.model.empty() ? disk.id : disk.model);
            card->setDetail(disk.bus + " · " + disk.path + " · " + diskStatus(disk));
            card->setNote(style::formatSize(disk.size));
            card->setEnabled(!disk.installationMedia);
            const std::string id = disk.id;
            card->onClicked = [this, id] {
                m_selected = id;
                updateDisks();
                replan();
                changed();
            };
            m_diskCards.push_back(card);
        }
        updateDisks();
    }

    void updateDisks()
    {
        for (size_t i = 0; i < m_disks.size(); ++i)
            m_diskCards[i]->setSelected(m_disks[i].id == m_selected);
        const Disk* disk = selectedDisk();
        m_summary->clearLines();
        m_summary->addLine(tr("Selected"), disk ? tr("1 disk") : tr("none"));
        m_summary->addLine(tr("Capacity"), disk ? style::formatSize(disk->size) : "—");
        m_summary->addLine(tr("Free"), disk ? style::formatSize(disk->freeSpace) : "—");
        m_summary->addLine(tr("Reclaimable"), disk ? style::formatSize(disk->reclaimableSpace) : "—");
        if (disk && m_method == Method::Automatic) {
            const DiskSegment* largest = largestPartition(*disk);
            const uint64_t minimum = largest ? installer().backend().minimumSize(largest->device) : 0;
            const std::string product = installer().backend().systemInfo().productShortName;
            m_existing->setVisible(m_existing->set(*disk, product, std::max<uint64_t>(m_required, 20 * GB), minimum));
        } else {
            m_existing->setVisible(false);
        }
    }

    void setMethod(Method method)
    {
        m_method = method;
        m_automatic->setSelected(method == Method::Automatic);
        m_custom->setSelected(method == Method::Custom);
        m_advanced->setSelected(method == Method::Advanced);
        updateDisks();
        replan();
        changed();
    }

    StoragePlan currentPlan() const
    {
        StoragePlan plan;
        plan.disks = {m_selected};
        plan.method = m_method == Method::Automatic ? StoragePlan::Method::Automatic : StoragePlan::Method::Custom;
        plan.encrypt = m_encrypted;
        if (m_existing->isVisible()) {
            plan.existing = m_existing->choice();
            plan.shrinkDevice = m_existing->shrinkDevice();
            plan.shrinkTo = m_existing->keepSize();
        } else {
            plan.existing = StoragePlan::ExistingData::Replace;
        }
        return plan;
    }

    // Works the layout out again shortly, once the choices settle.
    void replan()
    {
        if (m_method != Method::Automatic || m_selected.empty()) {
            m_layout->setParts({});
            m_layoutNote->setText(m_method == Method::Automatic
                                      ? std::string()
                                      : tr("You lay the disk out yourself on the next screen."));
            return;
        }
        m_replan.start(std::chrono::milliseconds(250));
    }

    void plan()
    {
        const StoragePlan plan = currentPlan();
        m_planning = true;
        m_layoutNote->setText(tr("Working out the layout…"));
        changed();
        installer().backend().planStorage(plan, [this](const std::vector<LayoutPart>& layout,
                                                       const ValidationReport& report) {
            m_planning = false;
            showLayout(layout, report);
            changed();
        });
    }

    void showLayout(const std::vector<LayoutPart>& layout, const ValidationReport& report)
    {
        std::vector<LayoutBar::Part> parts;
        const StoragePlan plan = currentPlan();
        const Disk* disk = selectedDisk();
        double total = disk ? double(disk->size) : 0;
        // What stays on the disk, hatched before the new parts.
        if (disk && plan.existing != StoragePlan::ExistingData::Replace) {
            uint64_t kept = 0;
            for (const DiskSegment& segment : disk->segments) {
                if (segment.free)
                    continue;
                kept += plan.existing == StoragePlan::ExistingData::Alongside && segment.device == plan.shrinkDevice
                            ? plan.shrinkTo
                            : segment.size;
            }
            if (kept)
                parts.push_back({disk->systems.empty() ? tr("kept") : disk->systems.front(), style::formatSize(kept),
                                 double(kept), mountColor({}), true});
        }
        for (const LayoutPart& part : layout)
            parts.push_back({part.mountPoint, style::formatSize(part.size), double(part.size),
                             mountColor(part.mountPoint), false});
        if (total <= 0) {
            for (const LayoutBar::Part& part : parts)
                total += part.fraction;
        }
        for (LayoutBar::Part& part : parts)
            part.fraction = total > 0 ? part.fraction / total : 0;
        m_layout->setParts(std::move(parts));
        if (!report.errors.empty())
            m_layoutNote->setText(report.errors.front());
        else if (!report.warnings.empty())
            m_layoutNote->setText(report.warnings.front());
        else
            m_layoutNote->setText({});
        m_layoutNote->setColor(report.errors.empty() ? style::color(Role::SecondaryText) : style::color(Role::Danger));
    }

    void explainSpecialized()
    {
        m_dialog = std::make_unique<DialogWindow>(installer().window(), tr("Specialized disks"), 420);
        auto content = std::make_unique<Column>();
        content->setSpacing(10);
        Label* text = addText(content.get(),
                              tr("iSCSI, FCoE and multipath devices are set up from the network installation image, "
                                 "which has the tools to discover them. This live system installs to the disks "
                                 "attached to this computer."),
                              14);
        text->setWrap(true);
        text->setLayoutHints({.fillWidth = true});
        m_dialog->setContentView(std::move(content));
        m_dialog->show();
    }

    DiskEditor* m_editor;
    Row* m_choiceView;
    Column* m_disksColumn;
    std::vector<OptionCard*> m_diskCards;
    ExistingDataCard* m_existing;
    DetailsCard* m_summary;
    OptionCard* m_automatic;
    OptionCard* m_custom;
    OptionCard* m_advanced;
    ToggleCard* m_encrypt;
    LayoutBar* m_layout;
    Label* m_layoutNote;
    std::vector<Disk> m_disks;
    std::string m_selected;
    Method m_method = Method::Automatic;
    bool m_encrypted = false;
    bool m_editing = false;
    bool m_planning = false;
    uint64_t m_required = 0;
    bayu::base::Timer m_replan;
    std::unique_ptr<DialogWindow> m_dialog;
};

} // namespace

std::unique_ptr<Page> createDestinationPage(Installer& installer)
{
    return std::make_unique<DestinationPage>(installer);
}

} // namespace installer
