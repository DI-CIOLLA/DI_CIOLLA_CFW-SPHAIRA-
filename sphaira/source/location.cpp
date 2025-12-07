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

enum SortKey {
    SORT_USB = 0,
    SORT_SD = 1,
    SORT_GAMES = 2,
    SORT_ALBUM = 3,
    SORT_USER = 4,
    SORT_SYSTEM = 5,
    SORT_SAFE = 6,
    SORT_PRODINFO = 7,
    SORT_OTHER = 99
};

static SortKey GetSortKey(const std::string& name) {

    if (name.rfind("ums", 0) == 0)      return SORT_USB;
    if (name == "sdmc")                 return SORT_SD;
    if (name == "games")                return SORT_GAMES;
    if (name == "album")                return SORT_ALBUM;
    if (name == "user")                 return SORT_USER;
    if (name == "system")               return SORT_SYSTEM;
    if (name == "safe")                 return SORT_SAFE;
    if (name == "prodinfo")             return SORT_PRODINFO;

    return SORT_OTHER;
}

auto GetStdio(bool write) -> StdioEntries {
    StdioEntries out{};

    const auto add_from_entries = [](StdioEntries& entries, StdioEntries& out, bool write) {
        for (auto& e : entries) {

            if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)) {
                log_write("[STDIO] skipping read only mount: %s\n", e.name.c_str());
                continue;
            }

            if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)
                e.display_name += i18n::get(" (Read Only)");

            e.sort_key = GetSortKey(e.name);
            out.emplace_back(e);
        }
    };


    // devoptab mounts
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
            entry.sort_key = SORT_OTHER;
            out.emplace_back(entry);
        }
    }
#endif


#ifdef ENABLE_LIBUSBHSFS

    if (!App::GetHddEnable()) {
        return out;
    }

    static UsbHsFsDevice devices[0x20];
    const auto count = usbHsFsListMountedDevices(devices, std::size(devices));

    for (s32 i = 0; i < count; i++) {
        const auto& d = devices[i];

        if (write && (d.write_protect || (d.flags & UsbHsFsMountFlags_ReadOnly)))
            continue;

        char display_name[256];
        snprintf(display_name, sizeof(display_name),
                "USB-DEVICE (%s - %s - %zu GB)",
                LIBUSBHSFS_FS_TYPE_STR(d.fs_type),
                (d.product_name[0] ? d.product_name : "Unknown"),
                (size_t)(d.capacity / 1024 / 1024 / 1024));

        // wichtig: interner mountname bleibt ORIGINAL → ums0 / ums1
        std::string mountname = d.name;

        u32 flags = 0;
        if (d.write_protect || (d.flags & UsbHsFsMountFlags_ReadOnly))
            flags |= FsEntryFlag::FsEntryFlag_ReadOnly;

        StdioEntry entry;
        entry.name = mountname;               // intern = ums0 → FUNKTIONIERT!
        entry.display_name = display_name;    // angezeigt = USB-DEVICE (…)
        entry.flags = flags;
        entry.sort_key = SORT_USB;            // immer ganz oben

        out.emplace_back(entry);
    }

#endif // ENABLE_LIBUSBHSFS


    std::sort(out.begin(), out.end(), [&](auto& a, auto& b) {
        if (a.sort_key != b.sort_key) 
            return a.sort_key < b.sort_key;

        return a.display_name < b.display_name;
    });

    return out;
}

} // namespace sphaira::location
