#include "location.hpp"
#include "i18n.hpp"
#include "fonts.hpp"
#include "log.hpp"
#include "utils.hpp"
#include "devoptab/devoptab.hpp"
#include "usbhsfs.hpp"

namespace sphaira::location {

// ------------------------------------------------------------------------
// Hilfsfunktion: Einträge aus einem Vector in den Zieldatensatz übernehmen
// + Umbenennungen der Namen (ZIEL B)
// ------------------------------------------------------------------------
static void add_from_entries(StdioEntries& entries, StdioEntries& out, bool write)
{
    for (auto& e : entries)
    {
        // ==========================
        // BENENNUNGEN (Ziel B)
        // ==========================
        if (e.name == "ums0") {
            e.name = "USB-DEVICE";
        }
        else if (e.name == "microSD card") {
            e.name = "SD-CARD";
        }
        else if (e.name == "games") {
            e.name = "GAMES";
        }
        else if (e.name == "Album") {
            e.name = "ALBUM";
        }

        // ==========================
        // READ ONLY HANDLING
        // ==========================
        if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)) {
            log_write("[STDIO] skipping read only mount: %s\n", e.name.c_str());
            continue;
        }

        if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly) {
            e.name += i18n::get(" (Read Only)");
        }

        out.emplace_back(e);
    }
}

// ------------------------------------------------------------------------
// USBHSFS-Unterstützung
// ------------------------------------------------------------------------
#ifdef ENABLE_LIBUSBHSFS
static bool add_usbhfs_entries(StdioEntries& out, bool write)
{
    UsbHsFsDevice usb_list[0x20];
    s32 usb_cnt = usbHsFsListMountedDevices(usb_list, 0x20);

    if (usb_cnt < 0)
        return false;

    for (s32 i = 0; i < usb_cnt; i++)
    {
        const auto& d = usb_list[i];

        StdioEntry e;
        e.name = "USB-DEVICE"; // BEIBEHALTEN: umbenannter Anzeigename

        e.flags = 0;
        if (!write)
            e.flags |= FsEntryFlag::FsEntryFlag_ReadOnly;

        // Formatierung (Dateisystem, Produktname, Größe)
        e.name += fmt::format(" ({} - {} - {} GB)",
            LIBUSBHSFS_FS_TYPE_STR(d.fs_type),
            (d.product_name[0] ? d.product_name : "Unknown"),
            d.sec_cnt * 512 / (1024 * 1024 * 1024)
        );

        out.emplace_back(e);
    }

    return true;
}
#endif  // ENABLE_LIBUSBHSFS

// ------------------------------------------------------------------------
// Hauptfunktion: Stdio-Liste abrufen
// ------------------------------------------------------------------------
StdioEntries GetStdio(bool write)
{
    StdioEntries out;

    // Einträge des Systems (devoptab)
    {
        StdioEntries entries;
        devoptab::GetStdio(entries);
        add_from_entries(entries, out, write);
    }

    // USBHSFS-Geräte
#ifdef ENABLE_LIBUSBHSFS
    add_usbhfs_entries(out, write);
#endif

    return out;
}

} // namespace sphaira::location
