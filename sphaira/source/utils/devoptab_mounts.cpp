#include "devoptab_mounts.hpp"
#include "devoptab.hpp"
#include "location.hpp"
#include "log.hpp"

namespace sphaira::utils::devoptab {

namespace {

struct MountDir {
    fs::FsPath mount_name{};
    bool write{};
    location::StdioEntries* entries{};
};

int MountRoot(DIR_ITER* dir_state, const char* path, struct stat* st) {
    auto* dir = reinterpret_cast<MountDir*>(dir_state->dirStruct);
    memset(st, 0, sizeof(struct stat));

    // Root mount entry is always a folder
    st->st_mode = S_IFDIR;
    return 0;
}

int OpenRoot(DIR_ITER* dir_state, const char* path) {
    dir_state->dirStruct = new MountDir{};
    auto* dir = reinterpret_cast<MountDir*>(dir_state->dirStruct);

    dir->mount_name = "";
    dir->write = false;
    dir->entries = nullptr;

    return 0;
}

int CloseRoot(DIR_ITER* dir_state) {
    auto* dir = reinterpret_cast<MountDir*>(dir_state->dirStruct);

    if (dir->entries) {
        delete dir->entries;
    }

    delete dir;
    dir_state->dirStruct = nullptr;
    return 0;
}

int ReadRoot(DIR_ITER* dir_state, const char* path, char* filename, struct stat* st) {
    auto* dir = reinterpret_cast<MountDir*>(dir_state->dirStruct);

    // Wenn entries==nullptr → lade neue Liste
    if (!dir->entries) {
        dir->entries = new location::StdioEntries();

        // Original: Einträge laden
        auto entries = location::GetStdio(false);

        // ⭐⭐⭐ FILTER BLOCK — EINZIGE ERWEITERUNG ⭐⭐⭐

        static const std::vector<std::string> hidden_mounts = {
            "ALBUM",
            "GAMES",
            "PRODINFOF",
            "SAFE",
            "USER",
            "SYSTEM"
        };

        // Für jedes entry prüfen, ob es gefiltert werden soll
        for (auto& e : entries) {
            // Versteckte Systempartitionen nicht anzeigen
            if (std::find(hidden_mounts.begin(), hidden_mounts.end(), e.name) != hidden_mounts.end()) {
                continue;
            }

            // Original: fs_hidden respektieren
            if (e.fs_hidden) {
                continue;
            }

            dir->entries->emplace_back(std::move(e));
        }

        // Wenn keine Einträge → fertig
        if (dir->entries->empty()) {
            return -1;
        }

        dir->mount_name = (*dir->entries)[0].mount;
        std::snprintf(filename, NAME_MAX, "%s", (*dir->entries)[0].name.c_str());
        memset(st, 0, sizeof(struct stat));
        st->st_mode = S_IFDIR;

        // ersten Eintrag entfernen
        dir->entries->erase(dir->entries->begin());
        return 0;
    }

    // Weitere Einträge
    if (dir->entries->empty()) {
        return -1;
    }

    std::snprintf(filename, NAME_MAX, "%s", (*dir->entries)[0].name.c_str());
    memset(st, 0, sizeof(struct stat));
    st->st_mode = S_IFDIR;

    dir->entries->erase(dir->entries->begin());
    return 0;
}

} // namespace

devoptab_t* GetRootDevoptab() {
    static devoptab_t root_devoptab = {};

    root_devoptab.name = "root";
    root_devoptab.structSize = sizeof(MountDir);
    root_devoptab.open_r = OpenRoot;
    root_devoptab.close_r = CloseRoot;
    root_devoptab.read_r = ReadRoot;
    root_devoptab.dirStateSize = sizeof(MountDir);
    root_devoptab.stat_r = MountRoot;

    return &root_devoptab;
}

} // namespace sphaira::utils::devoptab
