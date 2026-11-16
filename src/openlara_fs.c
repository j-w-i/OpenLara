#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nano_fs.h"

typedef struct {
    nano_file_t *nf;
    uint32_t size;
    uint32_t pos;
} nano_stdio_file_t;

static int g_fs_mounted = 0;

int openlara_fs_init(void) {
    if (g_fs_mounted) return 0;
    int rc = nano_fs_mount();
    if (rc == 0) g_fs_mounted = 1;
    return rc;
}

static void build_hfs_path(const char *name, char *out, size_t cap) {
    static const char prefix[] = "/Apps/Data/OpenLara/";
    size_t plen = sizeof(prefix) - 1;
    size_t i = 0;
    if (!name || !out || cap == 0) return;

    while (*name == '/' || (*name == '.' && name[1] == '/')) {
        if (*name == '.') name += 2;
        else ++name;
    }

    if (strncmp(name, "Apps/", 5) == 0 || strncmp(name, "/Apps/", 6) == 0) {
        if (name[0] != '/' && i + 1 < cap) out[i++] = '/';
        while (*name && i + 1 < cap) {
            char c = *name++;
            out[i++] = (c == '\\') ? '/' : c;
        }
        out[i] = 0;
        return;
    }

    if (plen + 1 >= cap) {
        out[0] = 0;
        return;
    }
    memcpy(out, prefix, plen);
    i = plen;
    while (*name && i + 1 < cap) {
        char c = *name++;
        out[i++] = (c == '\\') ? '/' : c;
    }
    out[i] = 0;
}

FILE *fopen(const char *path, const char *mode) {
    if (!path || !mode) return NULL;
    if (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')) return NULL;
    if (openlara_fs_init() != 0) return NULL;

    char hpath[512];
    build_hfs_path(path, hpath, sizeof(hpath));
    nano_file_t *nf = nano_fs_open(hpath);
    if (!nf) return NULL;

    nano_stdio_file_t *f = (nano_stdio_file_t *)malloc(sizeof(nano_stdio_file_t));
    if (!f) {
        nano_fs_close(nf);
        return NULL;
    }
    f->nf = nf;
    f->size = (uint32_t)nano_fs_size(nf);
    f->pos = 0;
    return (FILE *)f;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) return 0;
    nano_stdio_file_t *f = (nano_stdio_file_t *)stream;
    size_t want = size * nmemb;
    uint8_t *dst = (uint8_t *)ptr;
    size_t done = 0;
    while (done < want) {
        size_t chunk = want - done;
        if (chunk > 65536u) chunk = 65536u;
        int got = nano_fs_read(f->nf, dst + done, (uint32_t)chunk);
        if (got <= 0) break;
        done += (size_t)got;
        f->pos += (uint32_t)got;
    }
    return done / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    nano_stdio_file_t *f = (nano_stdio_file_t *)stream;
    int64_t target = 0;
    if (whence == SEEK_SET) target = offset;
    else if (whence == SEEK_CUR) target = (int64_t)f->pos + offset;
    else if (whence == SEEK_END) target = (int64_t)f->size + offset;
    else return -1;
    if (target < 0) target = 0;
    if (target > (int64_t)f->size) target = (int64_t)f->size;
    if (nano_fs_seek(f->nf, (int64_t)target, SEEK_SET) != 0) return -1;
    f->pos = (uint32_t)nano_fs_tell(f->nf);
    return 0;
}

long ftell(FILE *stream) {
    if (!stream) return -1L;
    nano_stdio_file_t *f = (nano_stdio_file_t *)stream;
    return (long)f->pos;
}

int fclose(FILE *stream) {
    if (!stream) return 0;
    nano_stdio_file_t *f = (nano_stdio_file_t *)stream;
    if (f->nf) nano_fs_close(f->nf);
    free(f);
    return 0;
}
