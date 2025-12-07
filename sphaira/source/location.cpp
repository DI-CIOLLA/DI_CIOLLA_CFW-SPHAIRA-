#include "location.hpp"
#include "fs.hpp"
#include <algorithm>

namespace sphaira::location {

static StdioEntries g_entries;

// ---------------------------------------------------------
// Hilfsfunktion: Anzeigename anpassen
// ---------------------------------------------------------
static void FixLabels(StdioEntry& e) {
    if (e.name == "ums0")
        e.label = "USB-DEVICE";

    if (e.name == "sdcard" || e.name == "microSD card")
        e.label = "SD-CARD";

    if (e.name == "games")
        e.label = "GAMES";

    if (e.name == "Album" || e.name == "album")
        e.label = "ALBUM";
}

// ---------------------------------------------------------
// Sortierregeln (harte feste Reihenfolge)
// ---------------------------------------------------------
static int SortOrder(const StdioEntry& e) {
    if (e.name == "ums0")               return 0;   // USB ganz oben
    if (e.label == "SD-CARD")           return 1;
    if (e.label == "ALBUM")             return 2;
    if (e.label == "GAMES")             return 3;
    if (e.name == "PRODINFOF")          return 4;
    if (e.name == "SAFE")               return 5;
    if (e.name == "SYSTEM")             return 6;
    if (e.name == "USER")               return 7;

    return 99; // alles andere unten
}

// ---------------------------------------------------------
// Hauptfunktion
// ---------------------------------------------------------
const StdioEntries& GetStdio(bool refresh)
{
    if (!refresh)
        return g_entries;

    g_entries.clear();

    auto entries = fs::ListStdio();  // holt echte Geräte

    // Labels korrigieren
    for (auto& e : entries)
        FixLabels(e);

    // Sortieren nach Regeln
    std::sort(entries.begin(), entries.end(),
        [](const StdioEntry& a, const StdioEntry& b) {
            return SortOrder(a) < SortOrder(b);
        }
    );

    g_entries = entries;
    return g_entries;
}

} // namespace sphaira::location
