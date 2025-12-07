#include "location.hpp"
#include "i18n.hpp"
#include "log.hpp"
#include "fs.hpp"

namespace sphaira::location {

static void add_from_fs(StdioEntries& out, bool write)
{
    StdioEntries entries;
    FsStdio(entries);     // <-- einzig richtige Funktion aus deinem Projekt!

    for (auto& e : entries)
    {
        // --------------------
        // BENENNUNGEN (Variante B)
        // --------------------

        if (e.name == "ums0")
            e.name = "USB-DEVICE";

        else if (e.name == "microSD card")
            e.name = "SD-CARD";

        else if (e.name == "games")
            e.name = "GAMES";

        else if (e.name == "Album")
            e.name = "ALBUM";


        // --------------------
        // READ ONLY HANDLING
        // --------------------
        if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly))
        {
            log_write("[STDIO] skipping read only mount: %s\n", e.name.c_str());
            continue;
        }

        if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)
            e.name += i18n::get(" (Read Only)");

        out.emplace_back(e);
    }
}

StdioEntries GetStdio(bool write)
{
    StdioEntries out;
    add_from_fs(out, write);  // <-- einzig benötigte Quelle
    return out;
}

} // namespace sphaira::location
