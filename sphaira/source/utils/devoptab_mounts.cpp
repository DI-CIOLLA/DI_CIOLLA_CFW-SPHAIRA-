#include "utils/devoptab.hpp"
#include "utils/devoptab_common.hpp"
#include "defines.hpp"
#include "log.hpp"
#include "location.hpp"

#include <cstring>
#include <cerrno>
#include <array>
#include <memory>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

namespace sphaira::devoptab {
namespace {

struct File {
    int fd;
};

struct Dir {
    DIR* dir{};
    location::StdioEntries* entries{};
    u32 index{};
};

struct Device final : common::MountDevice {
    Device(const common::MountConfig& _config)
        : MountDevice{_config} {}

private:
    bool Mount() override { return true; }
    int devoptab_open(void *fileStruct, const char *path, int flags, int mode) override;
    int devoptab_close(void *fd) override;
    ssize_t devoptab_read(void *fd, char *ptr, size_t len) override;
    ssize_t devoptab_seek(void *fd, off_t pos, int dir) override;
    int devoptab_fstat(void *fd, struct stat *st) override;
    int devoptab_unlink(const char *path) override;
    int devoptab_rename(const char *oldName, const char *newName) override;
    int devoptab_mkdir(const char *path, int mode) override;
    int devoptab_rmdir(const char *path) override;
    int devoptab_diropen(void* fd, const char *path) override;
    int devoptab_dirreset(void* fd) override;
    int devoptab_dirnext(void* fd, char *filename, struct stat *filestat) override;
    int devoptab_dirclose(void* fd) override;
    int devoptab_lstat(const char *path, struct stat *st) override;
    int devoptab_ftruncate(void *fd, off_t len) override;
    int devoptab_statvfs(const char *path, struct statvfs *buf) override;
    int devoptab_fsync(void *fd) override;
    int devoptab_utimes(const char *path, const struct timeval times[2]) override;
};

// Fixes devoptab paths
auto FixPath(const char* path) -> std::pair<fs::FsPath, std::string_view> {
    while (*path == '/') path++;

    std::string_view mount_name = path;
    const auto pos = mount_name.find_first_of(':');

    if (pos == std::string_view::npos)
        return {path, {}};

    mount_name = mount_name.substr(0, pos + 1);

    fs::FsPath fixed_path = path;
    if (fixed_path.ends_with(":")) fixed_path += '/';

    return {fixed_path, mount_name};
}

int Device::devoptab_open(void *fileStruct, const char *_path, int flags, int mode) {
    auto file = static_cast<File*>(fileStruct);

    const auto [path, mount_name] = FixPath(_path);
    if (mount_name.empty()) return -ENOENT;

    file->fd = open(path, flags, mode);
    if (file->fd < 0) return -errno;

    return 0;
}

int Device::devoptab_close(void *fd) {
    auto file = static_cast<File*>(fd);
    std::memset(file, 0, sizeof(*file));
    return 0;
}

ssize_t Device::devoptab_read(void *fd, char *ptr, size_t len) {
    return read(static_cast<File*>(fd)->fd, ptr, len);
}

ssize_t Device::devoptab_seek(void *fd, off_t pos, int dir) {
    return lseek(static_cast<File*>(fd)->fd, pos, dir);
}

int Device::devoptab_fstat(void *fd, struct stat *st) {
    return fstat(static_cast<File*>(fd)->fd, st);
}

int Device::devoptab_unlink(const char *_path) {
    const auto [path, mount_name] = FixPath(_path);
    if (mount_name.empty()) return -ENOENT;
    return unlink(path);
}

int Device::devoptab_rename(const char *_oldName, const char *_newName) {
    const auto [oldName, m1] = FixPath(_oldName);
    const auto [newName, m2] = FixPath(_newName);
    if (m1.empty() || m2.empty() || m1 != m2) return -ENOENT;
    return rename(oldName, newName);
}

int Device::devoptab_mkdir(const char *_path, int mode) {
    const auto [path, mount_name] = FixPath(_path);
    if (mount_name.empty()) return -ENOENT;
    return mkdir(path, mode);
}

int Device::devoptab_rmdir(const char *_path) {
    const auto [path, mount_name] = FixPath(_path);
    if (mount_name.empty()) return -ENOENT;
    return rmdir(path);
}

int Device::devoptab_diropen(void* fd, const char *_path) {
    auto dir = static_cast<Dir*>(fd);
    const auto [path, mount_name] = FixPath(_path);

    // Root of mount list → show StdioEntries
    if (mount_name.empty()) {
        dir->entries = new location::StdioEntries();
        auto entries = location::GetStdio(false);

        // --- SYSTEM PARTITION FILTER ---
        static const std::vector<std::string> hidden = {
            "ALBUM", "GAMES", "PRODINFOF", "SAFE", "USER", "SYSTEM"
        };

        auto is_hidden = [&](const std::string& name) {
            for (auto& s : hidden) {
                if (name.find(s) != std::string::npos)
                    return true;
            }
            return false;
        };
        // --------------------------------

        for (auto& entry : entries) {
            if (is_hidden(entry.name))
                continue;

            if (!entry.fs_hidden)
                dir->entries->emplace_back(std::move(entry));
        }

        return 0;
    }

    dir->dir = opendir(path);
    if (!dir->dir) return -errno;

    return 0;
}

int Device::devoptab_dirreset(void* fd) {
    auto dir = static_cast<Dir*>(fd);
    if (dir->dir) rewinddir(dir->dir);
    else dir->index = 0;
    return 0;
}

int Device::devoptab_dirnext(void* fd, char *filename, struct stat *filestat) {
    auto dir = static_cast<Dir*>(fd);

    if (dir->dir) {
        const auto entry = readdir(dir->dir);
        if (!entry) return -ENOENT;

        filestat->st_nlink = 1;
        filestat->st_mode = entry->d_type == DT_DIR ? S_IFDIR : S_IFREG;
        std::snprintf(filename, NAME_MAX, "%s", entry->d_name);
    } else {
        if (dir->index >= dir->entries->size()) return -ENOENT;

        const auto& entry = (*dir->entries)[dir->index];
        filestat->st_nlink = 1;
        filestat->st_mode = S_IFDIR;

        if (entry.mount.ends_with(":/"))
            std::snprintf(filename, NAME_MAX, "%s", entry.mount.substr(0, entry.mount.size() - 1).c_str());
        else
            std::snprintf(filename, NAME_MAX, "%s", entry.mount.c_str());
    }

    dir->index++;
    return 0;
}

int Device::devoptab_dirclose(void* fd) {
    auto dir = static_cast<Dir*>(fd);
    if (dir->dir) closedir(dir->dir);
    if (dir->entries) delete dir->entries;
    return 0;
}

int Device::devoptab_lstat(const char *_path, struct stat *st) {
    const auto [path, mount_name] = FixPath(_path);
    if (!mount_name.empty()) return lstat(path, st);

    st->st_nlink = 1;
    st->st_mode = S_IFDIR;
    return 0;
}

int Device::devoptab_ftruncate(void *fd, off_t len) {
    return ftruncate(static_cast<File*>(fd)->fd, len);
}

int Device::devoptab_statvfs(const char *_path, struct statvfs *buf) {
    const auto [path, mount_name] = FixPath(_path);
    if (mount_name.empty()) return -ENOENT;
    return statvfs(path, buf);
}

int Device::devoptab_fsync(void *fd) {
    return fsync(static_cast<File*>(fd)->fd);
}

int Device::devoptab_utimes(const char *_path, const struct timeval times[2]) {
    const auto [path, mount_name] = FixPath(_path);
    if (mount_name.empty()) return -ENOENT;
    return utimes(path, times);
}

} // namespace

Result MountInternalMounts() {
    common::MountConfig config{};
    config.fs_hidden = true;
    config.dump_hidden = true;

    if (!common::MountNetworkDevice2(
        std::make_unique<Device>(config),
        config,
        sizeof(File), sizeof(Dir),
        "mounts", "mounts:/"
    ))
    {
        R_THROW(0x1);
    }

    return 0;
}

} // namespace sphaira::devoptab
