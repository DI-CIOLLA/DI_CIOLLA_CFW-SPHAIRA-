#include "location.hpp"
#include "fs.hpp"
#include "app.hpp"
#include "utils/devoptab.hpp"
#include "i18n.hpp"

#include <cstring>

#ifdef ENABLE_LIBUSBDVD
    #include "usbdvd.hpp"
#endif

#ifdef ENABLE_LIBUSBHSFS
    #include <usbhsfs.h>
#endif

namespace sphaira::location {

auto GetStdio(bool write) -> StdioEntries {
    StdioEntries out{};

    const auto add_from_entries = [](StdioEntries& entries, StdioEntries& out, bool write) {
        for (auto& e : entries) {

            // --------------------------------------
            // RENAME DEFAULT DEVICES
            // --------------------------------------
            if (e.name == "ums0")
                e.name = "USB-DEVICE";

            if (e.name == "microSD card")
                e.name = "SD-CARD";

            if (e.name == "games")
                e.name = "GAMES";

            if (e.name == "Album")
                e.name = "ALBUM";

            // --------------------------------------

            if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)) {
                continue;
            }

            if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly) {
                e.name += " (Read Only)";
            }

            out.emplace_back(e);
        }
    };

    // -------------------------------------------
    // NORMAL DEVOPTAB MOUNTS
    // -------------------------------------------
    {
        StdioEntries entries;
        if (R_SUCCEEDED(devoptab::GetNetworkDevices(entries))) {
            add_from_entries(entries, out, write);
        }
    }

#ifdef ENABLE_LIBUSBDVD
    if (!write) {
        StdioEntry entry;
        if (usbdvd::GetMountPoint(entry)) {
            out.emplace_back(entry);
        }
    }
#endif

#ifdef ENABLE_LIBUSBHSFS
    if (App::GetHddEnable()) {

        static UsbHsFsDevice devices[0x20];
        const auto count = usbHsFsListMountedDevices(devices, std::size(devices));

        for (s32 i = 0; i < count; i++) {
            const auto& e = devices[i];

            if (write && (e.write_protect || (e.flags & UsbHsFsMountFlags_ReadOnly)))
                continue;

            // -------------------------------------------
            // BUILD REAL USB ENTRY (NO FAKE DISPLAY NAME)
            // -------------------------------------------
            StdioEntry usb;

            usb.name = "USB-DEVICE";               // visible name

            usb.mount_point = e.mount_point;       // REAL mount path from USBHSFS
                                                   // (this fixes "cannot read files")

            usb.flags = 0;
            if (e.write_protect || (e.flags & UsbHsFsMountFlags_ReadOnly)) {
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

    // -------------------------------------------
    // SORTING: USB FIRST
    // -------------------------------------------
    std::sort(out.begin(), out.end(),
        [](const StdioEntry& a, const StdioEntry& b) {

            bool a_usb = a.name.rfind("USB-DEVICE", 0) == 0;
            bool b_usb = b.name.rfind("USB-DEVICE", 0) == 0;

            if (a_usb && !b_usb) return true;
            if (!a_usb && b_usb) return false;

            return a.name < b.name;
        }
    );

    return out;
}

} // namespace sphaira::location
