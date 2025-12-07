#include "location.hpp"
#include "i18n.hpp"
#include "log.hpp"
#include "utils/devoptab.hpp"
#include "fs.hpp"

namespace sphaira::location {

// ---------------------------------------------------------
// Einträge von FS / Devoptab übernehmen + Umbenennungen
// ---------------------------------------------------------
static void add_from_entries(StdioEntries& entries, StdioEntries& out, bool write)
{
    for (auto& e : entries)
    {
        // --------------------
        // BENENNUNGEN (ZIEL B)
        // --------------------

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

        // --------------------
        // READ ONLY HANDLING
        // --------------------
        if (write && (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly))
        {
            log_write("[STDIO] skipping read only mount: %s\n", e.name.c_str());
            continue;
        }

        if (e.flags & FsEntryFlag::FsEntryFlag_ReadOnly)
        {
            e.name += i18n::get(" (Read Only)");
        }

        out.emplace_back(e);
    }
}

// ---------------------------------------------------------
// Hauptfunktion: Stdio-Liste abrufen
// ---------------------------------------------------------
StdioEntries GetStdio(bool write)
{
    StdioEntries out;

    // Einträge aus devoptab / FS holen
    {
        StdioEntries entries;
        devoptab::GetStdio(entries);
        add_from_entries(entries, out, write);
    }

    // USB-Geräte, SD-Karte usw. aus fs.cpp holen
    {
        StdioEntries entries;
        fs::ListStdio(entries);
        add_from_entries(entries, out, write);
    }

    return out;
}

} // namespace sphaira::location

