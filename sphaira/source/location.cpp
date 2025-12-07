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

static int GetPriority(const std::string& name) {
    // USB ganz oben (ums0, ums1, ...)
    if (name.rfind("ums", 0) == 0)
        return 0;

    if (name == "sdmc")     return 1;
    if (name == "games")    return 2;
    if (name == "album")    return 3;
    if (name == "user")     return 4;
    if (name == "system")   return 5;
    if (name == "safe")     return 6;
    if (name == "prodinfo") return 7;

    return 99;
}

auto GetStdio(bool write) -> StdioEntries {
    StdioEntries out{};

    const auto add_from_entries = [&](StdioEntries& entries, bool write) {
        for (auto e : entries) {

            if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly))
                continue;

            if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)
                e.label += i18n::get(" (Read Only)");

            out.emplace_back(e);
        }
    };

    // devoptab mounts
    {
        StdioEntries entries;
        if (R_SUCCEEDED(devoptab::GetNetworkDevices(entries))) {
            add_from_entries(entries, write);
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

    if (!App::GetHddEnable()) {
        return out;
    }

    static UsbHsFsDevice devices[0x20];
    const auto count = usbHsFsListMountedDevices(devices, std::size(devices));

    for (s32 i = 0; i < count; i++) {
        const auto& d = devices[i];

        if (write && (d.write_protect || (d.flags & UsbHsFsMountFlags_ReadOnly)))
            continue;

        char label[256];
        snprintf(label, sizeof(label),
                "USB-DEVICE (%s - %s - %zu GB)",
                LIBUSBHSFS_FS_TYPE_STR(d.fs_type),
                (d.product_name[0] ? d.product_name : "Unknown"),
                (size_t)(d.capacity / 1024 / 1024 / 1024));

        StdioEntry entry;
        entry.name = d.name;   // WICHTIG: ums0 → Dateizugriff funktioniert
        entry.label = label;   // Was im Menü angezeigt wird
        entry.flags = (d.write_protect || (d.flags & UsbHsFsMountFlags_ReadOnly))
                      ? FsEntryFlag::FsEntryFlag_ReadOnly
                      : 0;

        out.emplace_back(entry);
    }

#endif // ENABLE_LIBUSBHSFS


    // Sortierung nach Priorität
    std::sort(out.begin(), out.end(), [&](auto& a, auto& b){
        int pa = GetPriority(a.name);
        int pb = GetPriority(b.name);

        if (pa != pb)
            return pa < pb;

        return a.label < b.label;
    });

    return out;
}

} // namespace sphaira::location
