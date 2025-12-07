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
namespace {

} // namespace


auto GetStdio(bool write) -> StdioEntries {
    StdioEntries out{};

    const auto add_from_entries = [](StdioEntries& entries, StdioEntries& out, bool write) {
        for (auto& e : entries) {

            if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)) {
                log_write("[STDIO] skipping read only mount: %s\n", e.name.c_str());
                continue;
            }

            if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)
                e.name += i18n::get(" (Read Only)");

            out.emplace_back(e);
        }
    };

    // devoptab mounts
    {
        StdioEntries entries;
        if (R_SUCCEEDED(devoptab::GetNetworkDevices(entries))) {
            log_write("[LOCATION] got devoptab mounts: %zu\n", entries.size());
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

    if (!App::GetHddEnable()) {
        log_write("[USBHSFS] not enabled\n");
        return out;
    }

    // USB devices
    static UsbHsFsDevice devices[0x20];
    const auto count = usbHsFsListMountedDevices(devices, std::size(devices));
    log_write("[USBHSFS] got connected: %u\n", usbHsFsGetPhysicalDeviceCount());
    log_write("[USBHSFS] got count: %u\n", count);

    for (s32 i = 0; i < count; i++) {
        const auto& e = devices[i];

        if (write && (e.write_protect || (e.flags & UsbHsFsMountFlags_ReadOnly))) {
            log_write("[USBHSFS] skipping write protect\n");
            continue;
        }

        char display_name[0x100];
        std::snprintf(display_name, sizeof(display_name),
                      "USB-DEVICE (%s - %s - %zu GB)",
                      LIBUSBHSFS_FS_TYPE_STR(e.fs_type),
                      (e.product_name[0] ? e.product_name : "Unknown"),
                      (size_t)(e.capacity / 1024 / 1024 / 1024));

        u32 flags = 0;
        if (e.write_protect || (e.flags & UsbHsFsMountFlags_ReadOnly))
            flags |= FsEntryFlag::FsEntryFlag_ReadOnly;

        // name verändert → wichtig für Sortierung
        out.emplace_back("USB-DEVICE", display_name, flags);

        log_write("\t[USBHSFS] USB: %s serial: %s man: %s\n",
                  e.product_name, e.serial_number, e.manufacturer);
    }

#endif // ENABLE_LIBUSBHSFS


    // Richtige Reihenfolge
    static const std::vector<std::string> ORDER = {
        "USB-DEVICE",
        "SD-CARD",
        "GAMES",
        "ALBUM",
        "USER",
        "SYSTEM",
        "SAFE",
        "PRODINFO"
    };

    std::sort(out.begin(), out.end(), [&](auto& a, auto& b){
        auto ia = std::find(ORDER.begin(), ORDER.end(), a.name);
        auto ib = std::find(ORDER.begin(), ORDER.end(), b.name);
        return ia < ib;
    });

    return out;
}

} // namespace sphaira::location
