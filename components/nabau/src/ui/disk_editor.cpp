// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/disk_editor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "base/i18n.h"
#include "ui/check_box.h"
#include "ui/color_provider.h"
#include "ui/combo_box.h"
#include "ui/dialog_window.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/scroll_view.h"
#include "ui/slider.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/installer.h"
#include "ui/widgets.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace installer {

using style::Role;

namespace {

constexpr double GB = 1e9;

Color resolve(Role role)
{
    return style::color(role).resolve();
}

std::string formatName(const std::string& format)
{
    if (format.empty())
        return tr("unformatted");
    if (format == "efi")
        return "EFI";
    if (format == "biosboot")
        return "BIOS boot";
    if (format == "luks")
        return tr("encrypted");
    if (format == "lvmpv")
        return "LVM";
    if (format == "btrfs" || format == "ext4" || format == "xfs" || format == "ntfs" || format == "swap"
        || format == "vfat")
        return format;
    return format;
}

// The mount points in a piece: one, or the volumes' joined by ", ".
std::vector<std::string> mountPointsOf(const DiskSegment& segment)
{
    std::vector<std::string> points;
    size_t start = 0;
    while (start < segment.mountPoint.size()) {
        const size_t end = segment.mountPoint.find(", ", start);
        points.push_back(segment.mountPoint.substr(start, end == std::string::npos ? std::string::npos : end - start));
        start = end == std::string::npos ? segment.mountPoint.size() : end + 2;
    }
    return points;
}

// The name shown on a piece of the disk.
std::string segmentTitle(const DiskSegment& segment)
{
    if (segment.free)
        return tr("Free space");
    if (!segment.mountPoint.empty())
        return segment.mountPoint;
    if (!segment.description.empty())
        return segment.description;
    if (!segment.label.empty())
        return segment.label;
    return formatName(segment.format);
}

std::string gbText(uint64_t bytes)
{
    char text[32];
    snprintf(text, sizeof(text), "%.1f", double(bytes) / GB);
    return text;
}

} // namespace

// A disk as a bar of its partitions and free space; a click selects a piece.
class DiskMap : public View
{
public:
    explicit DiskMap(Disk disk)
        : m_disk(std::move(disk))
    {
        setLayoutHints({.fillWidth = true});
    }

    const Disk& disk() const { return m_disk; }
    void setSelected(int segment)
    {
        m_selected = segment;
        requestPaint();
    }
    std::function<void(size_t segment)> onSelected;

    SizeF sizeHint() const override { return {400, 64}; }
    bool acceptsPointer() const override { return true; }

protected:
    void layout() override { computeRects(); }

    void pointerPress(const PointerEvent& event) override
    {
        for (size_t i = 0; i < m_rects.size(); ++i) {
            if (m_rects[i].contains(event.position)) {
                if (onSelected)
                    onSelected(i);
                return;
            }
        }
    }

    void paint(Painter& painter) override
    {
        const RectF bar = localRect();
        painter.save();
        painter.clipRoundedRect(bar, 10);
        painter.fillRect(bar, resolve(Role::IconTile));
        for (size_t i = 0; i < m_rects.size(); ++i) {
            const DiskSegment& segment = m_disk.segments[i];
            const RectF rect = m_rects[i];
            Color fill;
            Color text = Color::rgb(0xFFFFFF);
            if (segment.free) {
                fill = resolve(Role::Card);
                text = Theme::current().text;
            } else if (!segment.mountPoint.empty()) {
                fill = mountColor(segment.mountPoint);
            } else {
                fill = Theme::current().darkMode ? Color::rgb(0x55585F) : Color::rgb(0xB9BDC6);
                text = Theme::current().darkMode ? Color::rgb(0xFFFFFF) : Color::rgb(0x2B2D33);
            }
            painter.fillRect(rect, fill);
            if (segment.free) {
                painter.save();
                painter.clip(rect);
                for (double x = rect.x - rect.height; x < rect.right(); x += 8)
                    painter.drawLine({x, rect.bottom()}, {x + rect.height, rect.y},
                                     Theme::current().text.withAlpha(0.08), 1);
                painter.restore();
            }
            if (i + 1 < m_rects.size())
                painter.fillRect({rect.right() - 1, 0, 2, bar.height}, resolve(Role::Content));
            (void)text;
        }
        painter.restore();
        // The outline of the selected piece.
        if (m_selected >= 0 && size_t(m_selected) < m_rects.size()) {
            const RectF rect = m_rects[size_t(m_selected)];
            painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), {8, 0, Corners::Circular}, Color{0, 0, 0, 0}, 2.5,
                                    Theme::current().highlight);
        }
        painter.drawRoundedRect(bar, {10, 0, Corners::Circular}, Color{0, 0, 0, 0}, 1, resolve(Role::CardBorder));
    }

    void childrenLaidOut() override {}

private:
    void computeRects()
    {
        // Labels of the pieces wide enough to carry one.
        while (!children().empty())
            takeChild(children().back().get());
        m_rects.clear();
        const double width = geometry().width;
        const double height = geometry().height;
        double total = 0;
        for (const DiskSegment& segment : m_disk.segments)
            total += double(segment.size);
        if (total <= 0)
            return;
        // Every piece at least 10 wide; the rest shared by size.
        const double minimum = 10;
        const double free = std::max(0.0, width - minimum * double(m_disk.segments.size()));
        double x = 0;
        for (const DiskSegment& segment : m_disk.segments) {
            const double w = minimum + free * double(segment.size) / total;
            m_rects.push_back({x, 0, w, height});
            x += w;
        }
        for (size_t i = 0; i < m_rects.size(); ++i) {
            const RectF rect = m_rects[i];
            if (rect.width < 70)
                continue;
            const DiskSegment& segment = m_disk.segments[i];
            const bool dark = segment.free || segment.mountPoint.empty();
            Label* title = addText(this, segmentTitle(segment), 12, 600);
            Label* size = addText(this, style::formatSize(segment.size), 11);
            for (Label* label : {title, size}) {
                label->setFadesWhenDisabled(false);
                if (!dark)
                    label->setColor(ColorRecipe(Color::rgb(0xFFFFFF)));
                else if (!segment.free && !Theme::current().darkMode)
                    label->setColor(ColorRecipe(Color::rgb(0x2B2D33)));
            }
            const double th = title->sizeHint().height;
            const double sh = size->sizeHint().height;
            const double top = std::round((height - th - sh) / 2);
            title->setGeometry({rect.x + 10, top, rect.width - 20, th});
            size->setGeometry({rect.x + 10, top + th, rect.width - 20, sh});
        }
    }

    Disk m_disk;
    std::vector<RectF> m_rects;
    int m_selected = -1;
};

namespace {

// A size in GB: a slider and a field kept together.
class SizeInput : public Column
{
public:
    SizeInput(uint64_t minimum, uint64_t maximum, uint64_t value)
        : m_minimum(minimum)
        , m_maximum(std::max(maximum, minimum))
    {
        setSpacing(8);
        setLayoutHints({.fillWidth = true});
        Row* row = addChild<Row>();
        row->setSpacing(12);
        row->setLayoutHints({.fillWidth = true});
        m_slider = row->addChild<Slider>();
        m_slider->setLayoutHints({.fillWidth = true});
        m_slider->setRange(double(m_minimum) / GB, double(m_maximum) / GB);
        m_field = row->addChild<Field>();
        m_field->setLayoutHints({.preferredWidth = 110});
        Label* unit = addText(row, "GB", 13, 400, Role::SecondaryText);
        (void)unit;
        Label* limits = addText(this,
                                format(tr("From {min} to {max}"), {{"min", style::formatSize(m_minimum)},
                                                                   {"max", style::formatSize(m_maximum)}}),
                                12, 400, Role::TertiaryText);
        (void)limits;
        set(value);
        m_slider->onMoved = [this](double gb) {
            m_value = uint64_t(gb * GB);
            if (m_value > m_maximum - uint64_t(GB / 20))
                m_value = m_maximum;
            m_updating = true;
            m_field->setText(gbText(m_value));
            m_updating = false;
        };
        m_field->onChanged = [this] {
            if (m_updating)
                return;
            try {
                const double gb = std::stod(m_field->text());
                m_value = std::clamp(uint64_t(gb * GB), m_minimum, m_maximum);
                m_slider->setValue(double(m_value) / GB);
            } catch (...) {
            }
        };
    }

    uint64_t value() const { return m_value; }
    void set(uint64_t value)
    {
        m_value = std::clamp(value, m_minimum, m_maximum);
        m_slider->setValue(double(m_value) / GB);
        m_updating = true;
        m_field->setText(gbText(m_value));
        m_updating = false;
    }

private:
    uint64_t m_minimum;
    uint64_t m_maximum;
    uint64_t m_value = 0;
    bool m_updating = false;
    Slider* m_slider;
    Field* m_field;
};

// A labelled control in a form.
template<typename T, typename... Args>
T* addFormRow(View* parent, const std::string& label, Args&&... args)
{
    Column* column = parent->addChild<Column>();
    column->setSpacing(6);
    column->setLayoutHints({.fillWidth = true});
    addText(column, label, 13, 400, Role::SecondaryText);
    T* control = column->addChild<T>(std::forward<Args>(args)...);
    control->setLayoutHints({.fillWidth = true});
    return control;
}

// One line of a list: a glyph and text.
void addListLine(View* parent, const std::string& glyph, Role tint, const std::string& text,
                 const std::string& trailing = {})
{
    Row* row = parent->addChild<Row>();
    row->setSpacing(10);
    row->setLayoutHints({.fillWidth = true});
    Icon* icon = row->addChild<Icon>(glyph, 15);
    icon->setColor(style::color(tint));
    Label* label = addText(row, text, 13);
    label->setLayoutHints({.fillWidth = true});
    if (!trailing.empty())
        addText(row, trailing, 13, 400, Role::SecondaryText);
}

} // namespace

DiskEditor::DiskEditor(Installer& installer)
    : m_installer(installer)
{
    setSpacing(16);
    setLayoutHints({.fillWidth = true, .fillHeight = true});
    m_maps = addChild<Column>();
    m_maps->setSpacing(10);
    m_maps->setLayoutHints({.fillWidth = true});

    Row* bottom = addChild<Row>();
    bottom->setSpacing(style::columnSpacing);
    bottom->setLayoutHints({.fillWidth = true, .fillHeight = true});

    m_panelScroll = bottom->addChild<ScrollView>();
    m_panelScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.05, .preferredWidth = 0});
    m_panelScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
    m_panel = m_panelScroll->setContent<Card>(18);
    m_reveal.setSingleShot(true);
    m_reveal.onTimeout = [this] { m_panelScroll->setScrollPosition({0, 1e6}); };
    m_panel->setSpacing(12);

    ScrollView* listScroll = bottom->addChild<ScrollView>();
    listScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});
    listScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
    Column* lists = listScroll->setContent<Column>();
    lists->setSpacing(10);
    m_error = lists->addChild<Banner>(Banner::Kind::Error, std::string(), std::string());
    m_error->setVisible(false);
    addSectionLabel(lists, tr("MOUNT POINTS"));
    m_mounts = lists->addChild<Card>(16);
    m_mounts->setSpacing(8);
    Row* changesHeading = lists->addChild<Row>();
    changesHeading->setLayoutHints({.fillWidth = true, .margins = {0, 8, 0, 0}});
    addSectionLabel(changesHeading, tr("PENDING CHANGES"));
    changesHeading->addStretch();
    ActionButton* reset = changesHeading->addChild<ActionButton>(tr("Reset all"), ActionButton::Kind::Link);
    reset->onClicked = [this] {
        m_installer.backend().resetCustom();
        m_selectedDisk.clear();
        reload();
    };
    m_actions = lists->addChild<Card>(16);
    m_actions->setSpacing(8);
    m_report = lists->addChild<Banner>(Banner::Kind::Warning, std::string(), std::string());
    m_report->setVisible(false);
}

DiskEditor::~DiskEditor() = default;

void DiskEditor::begin(const std::vector<std::string>& disks, bool advanced, bool encrypt)
{
    m_advanced = advanced;
    m_encrypt = encrypt;
    m_installer.backend().beginCustom(disks);
    m_selectedDisk.clear();
    reload();
}

void DiskEditor::reload()
{
    m_disks = m_installer.backend().customDisks();
    while (!m_maps->children().empty())
        m_maps->takeChild(m_maps->children().back().get());
    m_diskMaps.clear();
    for (const Disk& disk : m_disks) {
        Row* header = m_maps->addChild<Row>();
        header->setSpacing(10);
        header->setLayoutHints({.fillWidth = true});
        Icon* icon = header->addChild<Icon>(disk.removable ? "usb" : "hard-drive", 16);
        icon->setColor(style::color(Role::SecondaryText));
        addText(header, disk.model, 14, 600);
        addText(header, disk.path + "  ·  " + style::formatSize(disk.size), 13, 400, Role::SecondaryText);
        header->addStretch();
        addText(header, format(tr("{size} free"), {{"size", style::formatSize(disk.freeSpace)}}), 13, 400,
                Role::SecondaryText);
        DiskMap* map = m_maps->addChild<DiskMap>(disk);
        const std::string id = disk.id;
        map->onSelected = [this, id](size_t segment) { select(id, segment); };
        m_diskMaps.push_back(map);
    }
    // Keep the selection where it was when the piece is still there;
    // otherwise the first free space, else the first piece.
    bool kept = false;
    for (const Disk& disk : m_disks) {
        if (disk.id == m_selectedDisk && m_selectedSegment < disk.segments.size())
            kept = true;
    }
    if (!kept) {
        m_selectedDisk.clear();
        for (const Disk& disk : m_disks) {
            for (size_t i = 0; i < disk.segments.size() && m_selectedDisk.empty(); ++i) {
                if (disk.segments[i].free && disk.segments[i].size >= uint64_t(GB)) {
                    m_selectedDisk = disk.id;
                    m_selectedSegment = i;
                }
            }
        }
        if (m_selectedDisk.empty() && !m_disks.empty() && !m_disks.front().segments.empty()) {
            m_selectedDisk = m_disks.front().id;
            m_selectedSegment = 0;
        }
    }
    select(m_selectedDisk, m_selectedSegment);
    fillLists();
    if (onChanged)
        onChanged();
}

void DiskEditor::select(const std::string& disk, size_t segment)
{
    m_selectedDisk = disk;
    m_selectedSegment = segment;
    for (DiskMap* map : m_diskMaps)
        map->setSelected(map->disk().id == disk ? int(segment) : -1);
    m_error->setVisible(false);
    showPanel();
}

void DiskEditor::showPanel()
{
    while (!m_panel->children().empty())
        m_panel->takeChild(m_panel->children().back().get());
    for (const Disk& disk : m_disks) {
        if (disk.id != m_selectedDisk || m_selectedSegment >= disk.segments.size())
            continue;
        const DiskSegment& segment = disk.segments[m_selectedSegment];
        if (segment.free)
            showFreePanel(disk, segment);
        else
            showPartitionPanel(disk, segment);
        return;
    }
    addText(m_panel, tr("Select a part of a disk above."), 14, 400, Role::SecondaryText);
}

void DiskEditor::showFreePanel(const Disk& disk, const DiskSegment& segment)
{
    Row* title = m_panel->addChild<Row>();
    title->setSpacing(10);
    addText(title, tr("Free space"), 16, 600);
    addText(title, style::formatSize(segment.size), 14, 400, Role::SecondaryText);

    if (segment.size < uint64_t(GB)) {
        Label* small = addText(m_panel, tr("Too small to hold a mount point."), 13, 400, Role::SecondaryText);
        small->setWrap(true);
        return;
    }

    // Until there is a system on the disks, the quick way to one.
    if (!hasRoot()) {
        ActionButton* recommended = m_panel->addChild<ActionButton>(tr("Create the recommended layout here"),
                                                                    ActionButton::Kind::Primary);
        recommended->setLeadingGlyph("wand");
        recommended->setLayoutHints({.fillWidth = true});
        recommended->onClicked = [this] { apply(m_installer.backend().createRecommended(m_encrypt)); };
        Label* or_ = addText(m_panel, tr("or add a mount point yourself:"), 13, 400, Role::SecondaryText);
        or_->setLayoutHints({.margins = {0, 4, 0, 0}});
    }

    ComboBox* mount = addFormRow<ComboBox>(m_panel, tr("Mount point"), m_installer.backend().suggestedMountPoints());
    mount->setEditable(true);
    // The first mount point the layout lacks.
    std::vector<std::string> used;
    for (const Disk& each : m_disks) {
        for (const DiskSegment& piece : each.segments) {
            for (const std::string& point : mountPointsOf(piece))
                used.push_back(point);
        }
    }
    for (const std::string& point : m_installer.backend().suggestedMountPoints()) {
        if (std::find(used.begin(), used.end(), point) == used.end()) {
            mount->setEditText(point);
            break;
        }
    }
    auto* size = addFormRow<SizeInput>(m_panel, tr("Size"), uint64_t(GB / 10), segment.size, segment.size);
    std::vector<std::string> formats = m_installer.backend().fileSystems();
    Dropdown* fs = addFormRow<Dropdown>(m_panel, tr("File system"));
    fs->setItems(formats);
    fs->setCurrentIndex(0);
    // Mount points that want a format of their own.
    auto suggestFormat = [fs, formats](const std::string& point) {
        std::string wanted;
        if (point == "swap")
            wanted = "swap";
        else if (point == "/boot/efi")
            wanted = "efi";
        else if (point == "/boot")
            wanted = "ext4";
        for (size_t i = 0; i < formats.size() && !wanted.empty(); ++i) {
            if (formats[i] == wanted || (wanted == "efi" && formats[i] == "vfat"))
                fs->setCurrentIndex(int(i));
        }
    };
    suggestFormat(mount->editText());
    mount->onEditTextChanged = [mount, suggestFormat] { suggestFormat(mount->editText()); };
    // Advanced: what the mount point lives on.
    const std::vector<std::string> types = m_installer.backend().deviceTypes();
    Dropdown* type = nullptr;
    if (m_advanced && types.size() > 1) {
        type = addFormRow<Dropdown>(m_panel, tr("Device type"));
        std::vector<std::string> names;
        for (const std::string& id : types) {
            names.push_back(id == "lvm"        ? tr("LVM")
                            : id == "lvm-thin" ? tr("LVM thin provisioning")
                            : id == "btrfs"    ? tr("Btrfs")
                                               : tr("Standard partition"));
        }
        type->setItems(names);
        type->setCurrentIndex(0);
    }
    CheckBox* encrypt = m_panel->addChild<CheckBox>(tr("Encrypt"));
    encrypt->setChecked(m_encrypt);
    ActionButton* add = m_panel->addChild<ActionButton>(tr("Add mount point"), ActionButton::Kind::Secondary);
    add->setLeadingGlyph("plus");
    add->setLayoutHints({.fillWidth = true, .margins = {0, 4, 0, 0}});
    const std::string diskId = disk.id;
    add->onClicked = [this, mount, size, fs, encrypt, diskId, type, types] {
        Backend::MountRequest request;
        if (type && type->currentIndex() >= 0 && size_t(type->currentIndex()) < types.size())
            request.type = types[size_t(type->currentIndex())];
        request.disk = diskId;
        request.mountPoint = mount->editText();
        request.size = size->value();
        request.format = fs->currentText();
        request.encrypt = encrypt->isChecked();
        apply(m_installer.backend().addMount(request));
    };
}

void DiskEditor::showPartitionPanel(const Disk& disk, const DiskSegment& segment)
{
    Row* title = m_panel->addChild<Row>();
    title->setSpacing(10);
    addText(title, segmentTitle(segment), 16, 600);
    addText(title, style::formatSize(segment.size), 14, 400, Role::SecondaryText);
    DetailsCard* details = m_panel->addChild<DetailsCard>();
    details->setFill(std::nullopt, std::nullopt);
    details->setMargins({0, 0, 0, 0});
    details->addLine(tr("Device"), "/dev/" + segment.device);
    details->addLine(tr("Format"), formatName(segment.format) + (segment.encrypted ? " · LUKS" : ""));
    if (!segment.label.empty())
        details->addLine(tr("Label"), segment.label);
    details->addLine(tr("Mount point"), segment.mountPoint.empty() ? tr("none") : segment.mountPoint);
    details->addLine(tr("State"), segment.existing ? tr("on the disk now") : tr("to be created"),
                     segment.existing ? std::nullopt : std::optional<Role>(Role::Success));

    Row* actions = m_panel->addChild<Row>();
    actions->setSpacing(8);
    actions->setLayoutHints({.fillWidth = true, .margins = {0, 6, 0, 0}});
    ActionButton* mount = actions->addChild<ActionButton>(segment.mountPoint.empty() ? tr("Use…") : tr("Change…"));
    mount->setLeadingGlyph("pencil");
    const std::pair<uint64_t, uint64_t> limits = m_installer.backend().sizeLimits(segment.device);
    ActionButton* resize = actions->addChild<ActionButton>(tr("Resize…"));
    resize->setLeadingGlyph("resize");
    resize->setEnabled(limits.first < limits.second);
    ActionButton* remove = actions->addChild<ActionButton>(tr("Delete"), ActionButton::Kind::Secondary);
    remove->setLeadingGlyph("trash");
    const Disk diskCopy = disk;
    const DiskSegment segmentCopy = segment;
    mount->onClicked = [this, diskCopy, segmentCopy] { showMountForm(diskCopy, segmentCopy); };
    resize->onClicked = [this, segmentCopy] { showResizeForm(segmentCopy); };
    remove->onClicked = [this, segmentCopy] { confirmDelete(segmentCopy); };
    // Three equal buttons across the panel.
    for (ActionButton* button : {mount, resize, remove})
        button->setLayoutHints({.fillWidth = true, .preferredWidth = 0});
    if (!segment.existing && segment.mountPoint.empty() && segment.format.empty())
        mount->setEnabled(false);
    // Volumes inside it are not edited one by one here.
    if (segment.format == "lvmpv" || segment.format == "btrfs") {
        mount->setEnabled(false);
        const std::vector<DiskSegment> volumes = m_installer.backend().volumes(segment.device);
        Label* note = addText(m_panel,
                              volumes.empty() ? tr("It holds LVM or Btrfs volumes. Delete it to use its space.")
                                              : tr("Volumes in it:"),
                              12, 400, Role::SecondaryText);
        note->setWrap(true);
        note->setLayoutHints({.fillWidth = true, .margins = {0, 6, 0, 0}});
        for (const DiskSegment& volume : volumes) {
            Row* row = m_panel->addChild<Row>();
            row->setSpacing(8);
            row->setLayoutHints({.fillWidth = true});
            addText(row, volume.mountPoint.empty() ? volume.description : volume.mountPoint, 13, 600);
            addText(row, formatName(volume.format) + " · " + style::formatSize(volume.size), 12, 400,
                    Role::SecondaryText);
            row->addStretch();
            ActionButton* drop = row->addChild<ActionButton>(tr("Delete"), ActionButton::Kind::Link);
            drop->onClicked = [this, volume] { confirmDelete(volume); };
        }
    }
}

void DiskEditor::showMountForm(const Disk&, const DiskSegment& segment)
{
    revealForm();
    while (m_panel->children().size() > 3)
        m_panel->takeChild(m_panel->children().back().get());
    addSectionLabel(m_panel, segment.existing ? tr("USE THIS PARTITION") : tr("CHANGE THIS MOUNT POINT"));
    ComboBox* mount = addFormRow<ComboBox>(m_panel, tr("Mount point"), m_installer.backend().suggestedMountPoints());
    mount->setEditable(true);
    mount->setEditText(segment.mountPoint);
    CheckBox* reformat = m_panel->addChild<CheckBox>(tr("Format it (erases what is on it)"));
    reformat->setChecked(!segment.existing);
    reformat->setEnabled(segment.existing);
    std::vector<std::string> formats = m_installer.backend().fileSystems();
    Dropdown* fs = addFormRow<Dropdown>(m_panel, tr("File system"));
    fs->setItems(formats);
    fs->setCurrentIndex(0);
    for (size_t i = 0; i < formats.size(); ++i) {
        if (formats[i] == segment.format)
            fs->setCurrentIndex(int(i));
    }
    Row* buttons = m_panel->addChild<Row>();
    buttons->setSpacing(8);
    buttons->setLayoutHints({.fillWidth = true, .margins = {0, 4, 0, 0}});
    buttons->addStretch();
    ActionButton* cancel = buttons->addChild<ActionButton>(tr("Cancel"));
    ActionButton* save = buttons->addChild<ActionButton>(tr("Apply"), ActionButton::Kind::Primary);
    cancel->onClicked = [this] { showPanel(); };
    const DiskSegment copy = segment;
    save->onClicked = [this, mount, reformat, fs, copy] {
        Backend::MountRequest request;
        request.device = copy.device;
        request.mountPoint = mount->editText();
        request.reformat = reformat->isChecked();
        request.format = reformat->isChecked() ? fs->currentText() : copy.format;
        request.size = copy.size;
        // A partition encrypted stays so when it is formatted again.
        request.encrypt = copy.encrypted;
        request.label = copy.label;
        apply(m_installer.backend().changeMount(request));
    };
}

void DiskEditor::revealForm()
{
    // Once laid out with the form.
    m_reveal.start(std::chrono::milliseconds(30));
}

void DiskEditor::showResizeForm(const DiskSegment& segment)
{
    revealForm();
    while (m_panel->children().size() > 3)
        m_panel->takeChild(m_panel->children().back().get());
    addSectionLabel(m_panel, tr("RESIZE"));
    const std::pair<uint64_t, uint64_t> limits = m_installer.backend().sizeLimits(segment.device);
    Label* note = addText(m_panel,
                          segment.existing ? tr("Shrinking keeps the data on it and frees the rest of the space for "
                                                "the new system.")
                                           : tr("The size the new partition will have."),
                          13, 400, Role::SecondaryText);
    note->setWrap(true);
    note->setLayoutHints({.fillWidth = true});
    auto* size = m_panel->addChild<SizeInput>(limits.first, limits.second, segment.size);
    Row* buttons = m_panel->addChild<Row>();
    buttons->setSpacing(8);
    buttons->setLayoutHints({.fillWidth = true, .margins = {0, 4, 0, 0}});
    buttons->addStretch();
    ActionButton* cancel = buttons->addChild<ActionButton>(tr("Cancel"));
    ActionButton* save = buttons->addChild<ActionButton>(tr("Resize"), ActionButton::Kind::Primary);
    cancel->onClicked = [this] { showPanel(); };
    const std::string device = segment.device;
    save->onClicked = [this, size, device] { apply(m_installer.backend().resize(device, size->value())); };
}

void DiskEditor::confirmDelete(const DiskSegment& segment)
{
    m_confirm = std::make_unique<ConfirmDialog>(
        m_installer.window(), format(tr("Delete {name}?"), {{"name", segmentTitle(segment)}}),
        segment.existing ? tr("Everything on it is lost when the installation starts. Until then, Reset all brings "
                              "it back.")
                         : tr("The planned partition is removed from the layout."));
    m_confirm->setConfirmText(tr("Delete"));
    m_confirm->setDestructive(true);
    const std::string device = segment.device;
    m_confirm->onAccepted = [this, device] { apply(m_installer.backend().remove(device)); };
    m_confirm->show();
}

void DiskEditor::apply(const std::string& error)
{
    if (!error.empty()) {
        m_error->setText(tr("That change could not be made"), error);
        m_error->setVisible(true);
        return;
    }
    m_error->setVisible(false);
    reload();
}

void DiskEditor::fillLists()
{
    while (!m_mounts->children().empty())
        m_mounts->takeChild(m_mounts->children().back().get());
    bool any = false;
    for (const Disk& disk : m_disks) {
        for (const DiskSegment& segment : disk.segments) {
            if (segment.mountPoint.empty())
                continue;
            any = true;
            Row* row = m_mounts->addChild<Row>();
            row->setSpacing(10);
            row->setLayoutHints({.fillWidth = true});
            class Dot : public View
            {
            public:
                explicit Dot(Color color) : m_color(color) {}
                SizeF sizeHint() const override { return {10, 10}; }

            protected:
                void paint(Painter& painter) override { painter.fillEllipse(localRect(), m_color); }

            private:
                Color m_color;
            };
            row->addChild<Dot>(mountColor(segment.mountPoint));
            Label* point = addText(row, segment.mountPoint, 13, 600);
            point->setLayoutHints({.preferredWidth = 90});
            Label* what = addText(row, formatName(segment.format) + "  ·  /dev/" + segment.device, 13, 400,
                                  Role::SecondaryText);
            what->setLayoutHints({.fillWidth = true});
            addText(row, style::formatSize(segment.size), 13);
        }
    }
    if (!any)
        addText(m_mounts, tr("No mount points yet. Pick free space above and add “/”, or create the recommended "
                             "layout."),
                13, 400, Role::SecondaryText)
            ->setWrap(true);

    while (!m_actions->children().empty())
        m_actions->takeChild(m_actions->children().back().get());
    const std::vector<PendingAction> actions = m_installer.backend().pendingActions();
    for (const PendingAction& action : actions) {
        std::string glyph = "plus";
        Role tint = Role::Success;
        if (action.kind == "destroy") {
            glyph = "trash";
            tint = Role::Danger;
        } else if (action.kind == "resize") {
            glyph = "resize";
            tint = Role::Warning;
        } else if (action.kind == "format") {
            glyph = "pencil";
            tint = action.destructive ? Role::Warning : Role::SecondaryText;
        }
        addListLine(m_actions, glyph, tint, action.description);
    }
    if (actions.empty())
        addText(m_actions, tr("Nothing changes on the disks yet."), 13, 400, Role::SecondaryText);
}

void DiskEditor::setReport(const ValidationReport& report)
{
    if (report.errors.empty() && report.warnings.empty()) {
        m_report->setVisible(false);
        return;
    }
    std::string text;
    for (const std::string& line : report.errors.empty() ? report.warnings : report.errors)
        text += (text.empty() ? "" : "\n") + line;
    m_report->setKind(report.errors.empty() ? Banner::Kind::Warning : Banner::Kind::Error);
    m_report->setText(report.errors.empty() ? tr("Check before installing") : tr("This layout cannot be installed"),
                      text);
    m_report->setVisible(true);
}

bool DiskEditor::hasRoot() const
{
    for (const Disk& disk : m_disks) {
        for (const DiskSegment& segment : disk.segments) {
            for (const std::string& point : mountPointsOf(segment)) {
                if (point == "/")
                    return true;
            }
        }
    }
    return false;
}

} // namespace installer
