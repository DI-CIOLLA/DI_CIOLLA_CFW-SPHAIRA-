#include "location.hpp"
#include "fs.hpp"
#include "app.hpp"
#include "utils/devoptab.hpp"
#include "i18n.hpp"

#include <cstring>
#include <algorithm>

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

            //---------------------------------------------------
            // RENAME DEVICES (your requested names)
            //---------------------------------------------------
            if (e.name == "ums0")
                e.name = "USB-DEVICE";

            if (e.name == "microSD card")
                e.name = "SD-CARD";

            if (e.name == "games")
                e.name = "GAMES";

            if (e.name == "Album")
                e.name = "ALBUM";

            //---------------------------------------------------

            if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)) {
                log_write("[STDIO] skipping read only mount: %s\n", e.name.c_str());
                continue;
            }

            if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly) {
                e.name += " (Read Only)";
            }

            out.emplace_back(e);
        }
    };

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

            char display_name[0x100];
            std::snprintf(display_name, sizeof(display_name),
                          "USB-DEVICE (%s - %s - %zu GB)",
                          LIBUSBHSFS_FS_TYPE_STR(e.fs_type),
                          e.product_name,
                          e.capacity / 1024 / 1024 / 1024);

            u32 flags = 0;
            if (e.write_protect || (e.flags & UsbHsFsMountFlags_ReadOnly))
                flags |= FsEntryFlag::FsEntryFlag_ReadOnly;

            out.emplace_back("USB-DEVICE", display_name, flags);
        }
    }
#endif

    //---------------------------------------------------
    // SORTING: USB-DEVICE ALWAYS FIRST
    //---------------------------------------------------
    std::sort(out.begin(), out.end(),
        [](const StdioEntry& a, const StdioEntry& b) {

            // USB-DEVICE always at top
            if (a.name.rfind("USB-DEVICE", 0) == 0) return true;
            if (b.name.rfind("USB-DEVICE", 0) == 0) return false;

            return a.name < b.name;
        }
    );

    return out;
}

} // namespace sphaira::location
