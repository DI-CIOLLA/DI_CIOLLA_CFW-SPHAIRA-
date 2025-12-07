#include "location.hpp"
#include "fs.hpp"
#include "app.hpp"
#include "utils/devoptab.hpp"
#include "i18n.hpp"

#ifdef ENABLE_LIBUSBDVD
#include "usbdvd.hpp"
#endif

#ifdef ENABLE_LIBUSBHSFS
#include <usbhsfs.h>
#endif

namespace sphaira::location {

auto GetStdio(bool write) -> StdioEntries {
    StdioEntries out{};

    // ---------------------------------------------------------
    // Helper: rename and push normal stdio entries
    // ---------------------------------------------------------
    const auto add_from_entries = [&](StdioEntries& entries, StdioEntries& out, bool write) {
        for (auto& e : entries) {

            // Rename titles
            if (e.name == "ums0")            e.name = "USB-DEVICE";
            if (e.name == "microSD card")    e.name = "SD-CARD";
            if (e.name == "games")           e.name = "GAMES";
            if (e.name == "Album")           e.name = "ALBUM";

            // Enforce read-write rules
            if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly))
                continue;

            // Append "(Read Only)"
            if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)
                e.name += " (Read Only)";

            out.emplace_back(e);
        }
    };

    // ---------------------------------------------------------
    // Normal FS devices
    // ---------------------------------------------------------
    {
        StdioEntries entries;
        if (R_SUCCEEDED(devoptab::GetNetworkDevices(entries)))
            add_from_entries(entries, out, write);
    }

#ifdef ENABLE_LIBUSBDVD
    if (!write) {
        StdioEntry dvd;
        if (usbdvd::GetMountPoint(dvd))
            out.emplace_back(dvd);
    }
#endif

#ifdef ENABLE_LIBUSBHSFS
    if (App::GetHddEnable()) {

        UsbHsFsDevice devices[0x20];
        const s32 count = usbHsFsListMountedDevices(devices, 0x20);

        for (s32 i = 0; i < count; i++) {
            const auto& d = devices[i];

            if (write && (d.write_protect || (d.flags & UsbHsFsMountFlags_ReadOnly)))
                continue;

            // -----------------------------------------
            // Build USB entry — IMPORTANT:
            // --------------------------------------------------
            // YOUR location.hpp uses:
            //   name
            //   path        ← this is the mount_point
            //   flags
            // --------------------------------------------------

            StdioEntry usb;

            usb.name = "USB-DEVICE";
            usb.path = d.mount_point;        // FIXED: correct mount point (required for FS)
            usb.flags = 0;

            if (d.write_protect || (d.flags & UsbHsFsMountFlags_ReadOnly)) {
                usb.flags |= FsEntryFlag::FsEntryFlag_ReadOnly;
                usb.name += " (Read Only)";
            }

            usb.dump_path = "";
            usb.fs_hidden = false;
            usb.dump_hidden = false;

            out.emplace_back(usb);
        }
    }
#endif

    // ---------------------------------------------------------
    // SORTING — USB FIRST
    // ---------------------------------------------------------
    std::sort(out.begin(), out.end(),
        [](const StdioEntry& a, const StdioEntry& b) {

            bool a_usb = a.name.rfind("USB-DEVICE", 0) == 0;
            bool b_usb = b.name.rfind("USB-DEVICE", 0) == 0;

            if (a_usb != b_usb)
                return a_usb;  // USB first

            return a.name < b.name;
        }
    );

    return out;
}

} // namespace sphaira::location
