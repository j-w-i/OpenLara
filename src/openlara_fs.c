#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nano_fs.h"

typedef struct {
    nano_file_t *nf;
    uint32_t size;
    uint32_t pos;
    const uint8_t *mem;   /* emulator RAM file (LARA_EMU_FILES_ADDR), else NULL */
} nano_stdio_file_t;

#ifdef LARA_EMU_FILES_ADDR
/* Emulator-only file table staged into RAM by the host (NanoEmu has no iPod disk):
 * "LEMU", count, then count x {char name[32]; uint32 offset; uint32 size} (offset
 * from the table start). Paths match by their last component. */
static const uint8_t *emu_lookup(const char *path, uint32_t *size) {
    const uint32_t *t = (const uint32_t *)(uintptr_t)LARA_EMU_FILES_ADDR;
    if (t[0] != 0x554d454cu) return NULL;
    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    for (uint32_t i = 0; i < t[1] && i < 16u; ++i) {
        const uint8_t *e = (const uint8_t *)(t + 2) + i * 40u;
        if (strncmp((const char *)e, base, 32) == 0) {
            const uint32_t *w = (const uint32_t *)(e + 32);
            *size = w[1];
            return (const uint8_t *)t + w[0];
        }
    }
    return NULL;
}
#endif

static int g_fs_mounted = 0;

int openlara_fs_init(void) {
#ifdef LARA_EMU_FILES_ADDR
    return 0;   /* emulator: files come from the RAM table only, no NAND mount */
#endif
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
#ifdef LARA_EMU_FILES_ADDR
    {
        uint32_t esz = 0; const uint8_t *em = emu_lookup(path, &esz);
        if (em) {
            nano_stdio_file_t *f = (nano_stdio_file_t *)malloc(sizeof(nano_stdio_file_t));
            if (!f) return NULL;
            f->nf = NULL; f->size = esz; f->pos = 0; f->mem = em;
            return (FILE *)f;
        }
    }
#endif
    if (openlara_fs_init() != 0) return NULL;
#ifdef LARA_EMU_FILES_ADDR
    return NULL;
#endif

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
    f->mem = NULL;
    return (FILE *)f;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) return 0;
    nano_stdio_file_t *f = (nano_stdio_file_t *)stream;
    size_t want = size * nmemb;
    uint8_t *dst = (uint8_t *)ptr;
    size_t done = 0;
    if (f->mem) {
        if (want > f->size - f->pos) want = f->size - f->pos;
        memcpy(dst, f->mem + f->pos, want); f->pos += (uint32_t)want;
        return want / size;
    }
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
    if (f->mem) { f->pos = (uint32_t)target; return 0; }
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
