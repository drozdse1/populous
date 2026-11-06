/* Windows stand-ins for the POSIX calls the asset loaders and the save
 * requester use. Linux and macOS keep their real headers. Directory handles
 * are fake fds so open/openat can reject non-directories and reparse points
 * the way O_DIRECTORY and O_NOFOLLOW do. */
#ifndef POPULOUS_WIN_POSIX_H
#define POPULOUS_WIN_POSIX_H

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x100000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x200000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x400000
#endif

#ifndef S_ISREG
#ifdef _S_IFMT
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#else
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
#endif
#ifndef S_ISDIR
#ifdef _S_IFMT
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#endif

#ifndef ELOOP
#define ELOOP 114
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG 38
#endif
#ifndef EMFILE
#define EMFILE 24
#endif
#ifndef ENOTDIR
#define ENOTDIR 20
#endif
#ifndef EISDIR
#define EISDIR 21
#endif

typedef intptr_t pop_ssize_t;
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef pop_ssize_t ssize_t;
#endif

struct dirent {
    char d_name[260];
};

typedef struct WinDir {
    HANDLE handle;
    WIN32_FIND_DATAW data;
    int pending;
    struct dirent ent;
} DIR;

#define WIN_DIR_FD_BASE 100000
#define WIN_DIR_FD_COUNT 16

typedef struct WinDirSlot {
    int used;
    wchar_t path[4096];
} WinDirSlot;

static WinDirSlot g_win_dir_slots[WIN_DIR_FD_COUNT];

static int win_widen(const char *path, wchar_t *out, int cap)
{
    int wrote = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out, cap);
    if (wrote > 0) return 1;
    wrote = MultiByteToWideChar(CP_ACP, 0, path, -1, out, cap);
    return wrote > 0;
}

static int win_dir_store(const wchar_t *path)
{
    int index;
    for (index = 0; index < WIN_DIR_FD_COUNT; ++index) {
        if (g_win_dir_slots[index].used) continue;
        if (wcslen(path) >= 4096) {
            errno = ENAMETOOLONG;
            return -1;
        }
        wcscpy(g_win_dir_slots[index].path, path);
        g_win_dir_slots[index].used = 1;
        return WIN_DIR_FD_BASE + index;
    }
    errno = EMFILE;
    return -1;
}

static WinDirSlot *win_dir_slot(int fd)
{
    int index = fd - WIN_DIR_FD_BASE;
    if (index < 0 || index >= WIN_DIR_FD_COUNT || !g_win_dir_slots[index].used) return NULL;
    return &g_win_dir_slots[index];
}

static int win_posix_open_wide(const wchar_t *path, int flags)
{
    DWORD attr = GetFileAttributesW(path);
    int is_dir;
    int is_link;
    if (attr == INVALID_FILE_ATTRIBUTES) {
        errno = ENOENT;
        return -1;
    }
    is_dir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    is_link = (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    if ((flags & O_NOFOLLOW) && is_link) {
        errno = ELOOP;
        return -1;
    }
    if (flags & O_DIRECTORY) {
        if (!is_dir) {
            errno = ENOTDIR;
            return -1;
        }
        return win_dir_store(path);
    }
    if (is_dir) {
        errno = EISDIR;
        return -1;
    }
    return _wopen(path, _O_RDONLY | _O_BINARY);
}

static int win_posix_open(const char *path, int flags)
{
    wchar_t wide[4096];
    if (!path || !win_widen(path, wide, 4096)) {
        errno = EINVAL;
        return -1;
    }
    return win_posix_open_wide(wide, flags);
}

static int win_posix_openat(int dirfd, const char *name, int flags)
{
    WinDirSlot *slot = win_dir_slot(dirfd);
    wchar_t name_wide[1024];
    wchar_t full[4096];
    size_t dir_len;
    size_t name_len;
    if (!slot || !name || strchr(name, '/') || strchr(name, '\\')) {
        errno = EBADF;
        return -1;
    }
    if (!win_widen(name, name_wide, 1024)) {
        errno = EINVAL;
        return -1;
    }
    dir_len = wcslen(slot->path);
    name_len = wcslen(name_wide);
    if (dir_len + 1 + name_len + 1 > 4096) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(full, slot->path, dir_len * sizeof(wchar_t));
    full[dir_len] = L'\\';
    memcpy(full + dir_len + 1, name_wide, (name_len + 1) * sizeof(wchar_t));
    return win_posix_open_wide(full, flags);
}

static pop_ssize_t win_posix_read(int fd, void *buffer, size_t size)
{
    unsigned chunk;
    int count;
    if (fd >= WIN_DIR_FD_BASE) {
        errno = EISDIR;
        return -1;
    }
    chunk = size > 0x7fffffffu ? 0x7fffffffu : (unsigned)size;
    count = _read(fd, buffer, chunk);
    return count;
}

static int win_posix_fstat(int fd, struct stat *status)
{
    struct __stat64 info;
    if (!status) {
        errno = EINVAL;
        return -1;
    }
    if (fd >= WIN_DIR_FD_BASE) {
        if (!win_dir_slot(fd)) {
            errno = EBADF;
            return -1;
        }
        memset(status, 0, sizeof *status);
        status->st_mode = _S_IFDIR;
        return 0;
    }
    if (_fstat64(fd, &info) != 0) return -1;
    memset(status, 0, sizeof *status);
    status->st_mode = (unsigned short)info.st_mode;
    status->st_size = (off_t)info.st_size;
    return 0;
}

static int win_posix_close(int fd)
{
    WinDirSlot *slot = win_dir_slot(fd);
    if (slot) {
        slot->used = 0;
        slot->path[0] = 0;
        return 0;
    }
    return _close(fd);
}

static DIR *opendir(const char *path)
{
    wchar_t wide[4000];
    wchar_t pattern[4100];
    DIR *dir = (DIR *)calloc(1, sizeof *dir);
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }
    if (!path || !win_widen(path, wide, 4000) ||
        swprintf(pattern, sizeof pattern / sizeof pattern[0], L"%ls\\*", wide) < 0) {
        free(dir);
        errno = EINVAL;
        return NULL;
    }
    dir->handle = FindFirstFileW(pattern, &dir->data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        errno = ENOENT;
        return NULL;
    }
    dir->pending = 1;
    return dir;
}

static struct dirent *readdir(DIR *dir)
{
    if (!dir || dir->handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return NULL;
    }
    for (;;) {
        if (!dir->pending && !FindNextFileW(dir->handle, &dir->data)) return NULL;
        dir->pending = 0;
        if (!wcscmp(dir->data.cFileName, L".") || !wcscmp(dir->data.cFileName, L"..")) continue;
        if (!WideCharToMultiByte(CP_UTF8, 0, dir->data.cFileName, -1,
                                 dir->ent.d_name, (int)sizeof dir->ent.d_name, NULL, NULL))
            continue;
        return &dir->ent;
    }
}

static int closedir(DIR *dir)
{
    if (!dir) return -1;
    if (dir->handle != INVALID_HANDLE_VALUE) FindClose(dir->handle);
    free(dir);
    return 0;
}

#ifdef POPULOUS_WIN_POSIX_MACROS
#ifdef open
#undef open
#endif
#ifdef openat
#undef openat
#endif
#ifdef read
#undef read
#endif
#ifdef close
#undef close
#endif
#ifdef fstat
#undef fstat
#endif
#define open win_posix_open
#define openat win_posix_openat
#define read win_posix_read
#define close win_posix_close
#define fstat win_posix_fstat
#endif

#endif /* _WIN32 */
#endif /* POPULOUS_WIN_POSIX_H */
