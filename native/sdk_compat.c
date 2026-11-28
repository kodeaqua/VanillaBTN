/* Native PSY-Q compatibility layer for symbols missing from pinned PsyZ 00fb42e:
 * GTE RotMatrixZXY, GPU MoveImage2/KanjiFnt*, libgs lighting/offset helpers,
 * libmcrd layered over PsyZ libcard/libapi, and libmcx (PocketStation absent). */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <psyz.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libapi.h>
#include "card_file.h"
#include "kanji_font.h"
#include "sdk_mcrd.h"
#include <libmcx.h>

/* ---- GTE ---- */

/* Build a Z-X-Y Euler rotation matrix (R_z * R_x * R_y) in Q4.12, preserving m->t. */
MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m) {
    int sx = rsin(r->vx);
    int cx = rcos(r->vx);
    int sy = rsin(r->vy);
    int cy = rcos(r->vy);
    int sz = rsin(r->vz);
    int cz = rcos(r->vz);
    int szsx = (sz * sx) >> 12;
    int czsx = (cz * sx) >> 12;

    m->m[0][0] = (short)(((cz * cy) >> 12) - ((szsx * sy) >> 12));
    m->m[0][1] = (short)(-((sz * cx) >> 12));
    m->m[0][2] = (short)(((cz * sy) >> 12) + ((szsx * cy) >> 12));
    m->m[1][0] = (short)(((sz * cy) >> 12) + ((czsx * sy) >> 12));
    m->m[1][1] = (short)((cz * cx) >> 12);
    m->m[1][2] = (short)(((sz * sy) >> 12) - ((czsx * cy) >> 12));
    m->m[2][0] = (short)(-((cx * sy) >> 12));
    m->m[2][1] = (short)sx;
    m->m[2][2] = (short)((cx * cy) >> 12);
    return m;
}

/* ---- GPU ---- */

int MoveImage2(RECT *rect, int x, int y) {
    return MoveImage(rect, x, y);
}

enum {
    KANJI_BUF_SIZE = 2048,
    KANJI_STRIP_WIDTH = 128,
    KANJI_MAX_STRIPS = 3,
};

static struct {
    int open;
    int x;
    int y;
    int w;
    int h;
    int dx;
    int dy;
    int cx;
    int cy;
    int isbg;
    int len;
    int cached_valid;
    u_short clut;
    char buf[KANJI_BUF_SIZE];
    char cached[KANJI_BUF_SIZE];
    u_short vram[KANJI_VRAM_HALFWORDS];
    DR_MODE dr[2];
    SPRT sprt[KANJI_MAX_STRIPS];
} kanji_state;

int KanjiFntOpen(int x, int y, int w, int h, int dx, int dy, int cx, int cy, int isbg, int n) {
    (void)n;
    if (w > KANJI_MAX_WIDTH) {
        w = KANJI_MAX_WIDTH;
    }
    if (h > KANJI_MAX_HEIGHT) {
        h = KANJI_MAX_HEIGHT;
    }
    w &= ~3;
    if (w <= 0 || h <= 0) {
        return -1;
    }

    kanji_state.open = 1;
    kanji_state.x = x;
    kanji_state.y = y;
    kanji_state.w = w;
    kanji_state.h = h;
    kanji_state.dx = dx;
    kanji_state.dy = dy;
    kanji_state.cx = cx;
    kanji_state.cy = cy;
    kanji_state.isbg = isbg;
    kanji_state.len = 0;
    kanji_state.cached_valid = 0;
    kanji_state.buf[0] = '\0';
    kanji_state.cached[0] = '\0';
    return 0;
}

void KanjiFntClose(int id) {
    (void)id;
    kanji_state.open = 0;
    kanji_state.len = 0;
    kanji_state.cached_valid = 0;
    kanji_state.buf[0] = '\0';
}

int KanjiFntPrint(int id, const char *fmt, ...) {
    va_list args;
    int written;

    (void)id;
    if (!kanji_state.open || fmt == NULL) {
        return 0;
    }
    int remaining = KANJI_BUF_SIZE - 1 - kanji_state.len;
    if (remaining <= 0) {
        return 0;
    }
    va_start(args, fmt);
    written = vsnprintf(kanji_state.buf + kanji_state.len, (size_t)remaining + 1, fmt, args);
    va_end(args);
    if (written <= 0) {
        return 0;
    }
    if (written > remaining) {
        written = remaining;
    }
    kanji_state.len += written;
    kanji_state.buf[kanji_state.len] = '\0';
    return written;
}

u_long *KanjiFntFlush(int id) {
    (void)id;
    if (!kanji_state.open) {
        return NULL;
    }
    if (kanji_state.len == 0) {
        return NULL;
    }

    if (!kanji_state.cached_valid || strcmp(kanji_state.buf, kanji_state.cached) != 0) {
        u_short clut_data[16];
        clut_data[0] = kanji_state.isbg ? 0x8000 : 0x0000;
        for (int i = 1; i < 16; i++) {
            u_short level = (u_short)(i * 2 + 1);
            clut_data[i] = (u_short)(0x8000 | level | (level << 5) | (level << 10));
        }
        kanji_state.clut = LoadClut2((u_long *)clut_data, kanji_state.cx, kanji_state.cy);
        if (!vanillabtn_kanji_rasterize(kanji_state.buf, kanji_state.w, kanji_state.h,
                                        kanji_state.vram)) {
            kanji_state.len = 0;
            kanji_state.buf[0] = '\0';
            return NULL;
        }
        RECT dst = {
            (short)kanji_state.dx,
            (short)kanji_state.dy,
            (short)(kanji_state.w / 4),
            (short)kanji_state.h,
        };
        LoadImage(&dst, (u_long *)kanji_state.vram);
        DrawSync(0);
        memcpy(kanji_state.cached, kanji_state.buf, (size_t)kanji_state.len + 1);
        kanji_state.cached_valid = 1;
    }

    RECT tw = {0, 0, 0, 0};
    u_short tpage0 = GetTPage(0, 0, kanji_state.dx, kanji_state.dy);
    u_short tpage1 = GetTPage(0, 0, kanji_state.dx + 64, kanji_state.dy);
    SetDrawMode(&kanji_state.dr[0], 0, 0, tpage0, &tw);
    SetDrawMode(&kanji_state.dr[1], 0, 0, tpage1, &tw);

    void *prev = &kanji_state.dr[0];
    int strip = 0;
    for (int offset = 0; offset < kanji_state.w && strip < KANJI_MAX_STRIPS;
         offset += KANJI_STRIP_WIDTH, strip++) {
        if (offset == 256) {
            CatPrim(prev, &kanji_state.dr[1]);
            prev = &kanji_state.dr[1];
        }
        int strip_w = kanji_state.w - offset;
        if (strip_w > KANJI_STRIP_WIDTH) {
            strip_w = KANJI_STRIP_WIDTH;
        }
        SPRT *sp = &kanji_state.sprt[strip];
        SetSprt(sp);
        setRGB0(sp, 0x80, 0x80, 0x80);
        setXY0(sp, kanji_state.x + offset, kanji_state.y);
        setWH(sp, strip_w, kanji_state.h);
        setUV0(sp, offset & 0xFF, 0);
        sp->clut = kanji_state.clut;
        CatPrim(prev, sp);
        prev = sp;
    }
    TermPrim(prev);
    DrawOTag((OT_TYPE *)&kanji_state.dr[0]);

    kanji_state.len = 0;
    kanji_state.buf[0] = '\0';
    return (u_long *)&kanji_state.dr[0];
}

/* ---- libgs ---- */

/* Exported by PsyZ libgs.c and read by GsSetDrawBuffOffset(). */
extern struct {
    short offx;
    short offy;
} POSITION;

static int gs_light_mode = 0;

void GsSetOffset(long x, long y) {
    POSITION.offx = (short)x;
    POSITION.offy = (short)y;
    GsSetDrawBuffOffset();
}

void GsSetProjection(long h) {
    SetGeomScreen(h);
}

void GsSetAmbient(long r, long g, long b) {
    SetBackColor(r >> 4, g >> 4, b >> 4);
}

void GsSetFlatLight(int id, GsF_LIGHT *light) {
    MATRIX lm;
    MATRIX cm;
    VECTOR dir;
    SVECTOR norm;

    if (id < 0 || id >= 3 || light == NULL) {
        return;
    }
    ReadLightMatrix(&lm);
    ReadColorMatrix(&cm);
    dir.vx = light->direction.vx;
    dir.vy = light->direction.vy;
    dir.vz = light->direction.vz;
    if (dir.vx == 0 && dir.vy == 0 && dir.vz == 0) {
        norm.vx = 0;
        norm.vy = 0;
        norm.vz = 0;
    } else {
        VectorNormalS(&dir, &norm);
    }
    lm.m[id][0] = norm.vx;
    lm.m[id][1] = norm.vy;
    lm.m[id][2] = norm.vz;
    cm.m[0][id] = (short)(light->r << 4);
    cm.m[1][id] = (short)(light->g << 4);
    cm.m[2][id] = (short)(light->b << 4);
    SetLightMatrix(&lm);
    SetColorMatrix(&cm);
}

void GsSetLightMode(int mode) {
    gs_light_mode = mode;
}

void GsInit3D(void) {
    MATRIX zero;

    InitGeom();
    SetGeomOffset(0, 0);
    SetGeomScreen(1000);
    memset(&zero, 0, sizeof(zero));
    SetLightMatrix(&zero);
    SetColorMatrix(&zero);
    SetBackColor(0, 0, 0);
    gs_light_mode = 0;
}

/* ---- libmcrd over PsyZ libcard / libapi ---- */

enum {
    MCRD_BLOCK_BYTES = 8192,
    MCRD_MAX_BLOCKS = 15,
    MCRD_SECTOR_ALIGN = 128,
    MCRD_MAX_NAME_LEN = 20,
    MCRD_PATH_CAP = 64,
    MCRD_HOST_PATH_CAP = 256,
    MCRD_SYNC_IDLE = -1,
    MCRD_SYNC_DONE = 1,
};

static int mcrd_sync_state = MCRD_SYNC_IDLE;
static long mcrd_sync_cmd = 0;
static long mcrd_sync_result = McErrNone;
static MemCB mcrd_callback = NULL;
static int mcrd_open_fd = -1;
static long mcrd_open_flag = 0;

static int mcrd_chan_prefix(long chan, char out[6]) {
    if (chan == 0x00) {
        memcpy(out, "bu00:", 6);
        return 1;
    }
    if (chan == 0x10) {
        memcpy(out, "bu10:", 6);
        return 1;
    }
    return 0;
}

static int mcrd_valid_filename(const char *file) {
    size_t i;

    if (file == NULL || file[0] == '\0') {
        return 0;
    }
    if (file[0] == '.') {
        return 0;
    }
    for (i = 0; file[i] != '\0'; i++) {
        unsigned char c = (unsigned char)file[i];
        if (i >= MCRD_MAX_NAME_LEN - 1) {
            return 0;
        }
        if (c < 0x20 || c > 0x7E || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?') {
            return 0;
        }
    }
    return 1;
}

static int mcrd_build_path(long chan, const char *file, char out[MCRD_PATH_CAP]) {
    char prefix[6];

    if (!mcrd_chan_prefix(chan, prefix) || !mcrd_valid_filename(file)) {
        return 0;
    }
    snprintf(out, MCRD_PATH_CAP, "%s%s", prefix, file);
    return 1;
}

static int mcrd_pattern_match(const char *pattern, const char *name) {
    if (pattern == NULL || name == NULL) {
        return 0;
    }
    while (*pattern != '\0') {
        if (*pattern == '*') {
            return 1;
        }
        if (*name == '\0') {
            return 0;
        }
        if (*pattern != '?' && *pattern != *name) {
            return 0;
        }
        pattern++;
        name++;
    }
    return *name == '\0';
}

static void mcrd_finish_async(long cmd, long result) {
    mcrd_sync_cmd = cmd;
    mcrd_sync_result = result;
    mcrd_sync_state = MCRD_SYNC_DONE;
    if (mcrd_callback != NULL) {
        mcrd_callback((unsigned long)cmd, (unsigned long)result);
    }
}

static long mcrd_used_blocks(long chan) {
    char query[8];
    struct DIRENTRY entry;
    struct DIRENTRY *it;
    long used = 0;

    if (!mcrd_chan_prefix(chan, query)) {
        return -1;
    }
    query[5] = '*';
    query[6] = '\0';
    for (it = firstfile(query, &entry); it != NULL; it = nextfile(&entry)) {
        long size = entry.size > 0 ? entry.size : 0;
        used += (size + MCRD_BLOCK_BYTES - 1) / MCRD_BLOCK_BYTES;
    }
    return used;
}

void MemCardInit(long val) {
    InitCARD(val);
    mcrd_sync_state = MCRD_SYNC_IDLE;
    mcrd_sync_cmd = 0;
    mcrd_sync_result = McErrNone;
    mcrd_open_fd = -1;
}

void MemCardStart(void) {
    StartCARD();
    _bu_init();
}

void MemCardStop(void) {
    if (mcrd_open_fd >= 0) {
        psyz_close(mcrd_open_fd);
        mcrd_open_fd = -1;
    }
    StopCARD();
}

void MemCardEnd(void) {
    mcrd_callback = NULL;
    mcrd_sync_state = MCRD_SYNC_IDLE;
}

MemCB MemCardCallback(MemCB func) {
    MemCB prev = mcrd_callback;
    mcrd_callback = func;
    return prev;
}

long MemCardExist(long chan) {
    char prefix[6];

    if (!mcrd_chan_prefix(chan, prefix)) {
        mcrd_finish_async(McFuncExist, McErrCardNotExist);
        return 1;
    }
    _card_info(chan);
    mcrd_finish_async(McFuncExist, McErrNone);
    return 1;
}

long MemCardAccept(long chan) {
    char prefix[6];

    if (!mcrd_chan_prefix(chan, prefix)) {
        mcrd_finish_async(McFuncAccept, McErrCardNotExist);
        return 1;
    }
    _card_info(chan);
    _card_load(chan);
    mcrd_finish_async(McFuncAccept, McErrNone);
    return 1;
}

long MemCardSync(long mode, long *cmds, long *result) {
    (void)mode;
    if (mcrd_sync_state == MCRD_SYNC_IDLE) {
        return MCRD_SYNC_IDLE;
    }
    if (cmds != NULL) {
        *cmds = mcrd_sync_cmd;
    }
    if (result != NULL) {
        *result = mcrd_sync_result;
    }
    mcrd_sync_state = MCRD_SYNC_IDLE;
    return MCRD_SYNC_DONE;
}

long MemCardGetDirentry(long chan, char *name, struct DIRENTRY *dir, long *files, long ofs,
                        long max) {
    char query[8];
    struct DIRENTRY entry;
    struct DIRENTRY *it;
    long matched = 0;
    long stored = 0;

    if (files != NULL) {
        *files = 0;
    }
    if (!mcrd_chan_prefix(chan, query)) {
        return McErrCardNotExist;
    }
    if (name == NULL || ofs < 0 || max < 0) {
        return McErrCardInvalid;
    }
    query[5] = '*';
    query[6] = '\0';
    for (it = firstfile(query, &entry); it != NULL; it = nextfile(&entry)) {
        if (!mcrd_pattern_match(name, entry.name)) {
            continue;
        }
        if (matched >= ofs && stored < max && dir != NULL) {
            dir[stored] = entry;
            stored++;
        }
        matched++;
    }
    if (files != NULL) {
        *files = stored;
    }
    return McErrNone;
}

long MemCardCreateFile(long chan, char *file, long blocks) {
    char path[MCRD_PATH_CAP];
    unsigned char zero_block[MCRD_BLOCK_BYTES];
    long used;
    long i;
    int fd;

    if (!mcrd_chan_prefix(chan, path)) {
        return McErrCardNotExist;
    }
    if (blocks <= 0 || blocks > MCRD_MAX_BLOCKS || !mcrd_build_path(chan, file, path)) {
        return McErrCardInvalid;
    }
    fd = psyz_open(path, FREAD);
    if (fd >= 0) {
        psyz_close(fd);
        return McErrAlreadyExist;
    }
    used = mcrd_used_blocks(chan);
    if (used < 0) {
        return McErrCardNotExist;
    }
    if (used + blocks > MCRD_MAX_BLOCKS) {
        return McErrBlockFull;
    }
    fd = psyz_open(path, FCREAT | FWRITE);
    if (fd < 0) {
        return McErrCardInvalid;
    }
    memset(zero_block, 0, sizeof(zero_block));
    for (i = 0; i < blocks; i++) {
        if (psyz_write(fd, zero_block, MCRD_BLOCK_BYTES) != MCRD_BLOCK_BYTES) {
            psyz_close(fd);
            erase(path);
            return McErrCardInvalid;
        }
    }
    psyz_close(fd);
    return McErrNone;
}

long MemCardDeleteFile(long chan, char *file) {
    char path[MCRD_PATH_CAP];

    if (!mcrd_chan_prefix(chan, path)) {
        return McErrCardNotExist;
    }
    if (!mcrd_build_path(chan, file, path)) {
        return McErrFileNotExist;
    }
    return erase(path) ? McErrNone : McErrFileNotExist;
}

long MemCardOpen(long chan, char *file, long flag) {
    char path[MCRD_PATH_CAP];
    int open_flags;

    if (mcrd_open_fd >= 0) {
        return -1;
    }
    if (!mcrd_chan_prefix(chan, path)) {
        return McErrCardNotExist;
    }
    if (!mcrd_build_path(chan, file, path)) {
        return McErrFileNotExist;
    }
    if (flag == O_RDONLY) {
        open_flags = FREAD;
    } else if (flag == O_WRONLY) {
        open_flags = FREAD | FWRITE;
    } else {
        return McErrCardInvalid;
    }
    mcrd_open_fd = psyz_open(path, open_flags);
    if (mcrd_open_fd < 0) {
        return McErrFileNotExist;
    }
    mcrd_open_flag = flag;
    return McErrNone;
}

void MemCardClose(void) {
    if (mcrd_open_fd >= 0) {
        psyz_close(mcrd_open_fd);
        mcrd_open_fd = -1;
        mcrd_open_flag = 0;
    }
}

static long mcrd_read_fd(int fd, unsigned long *adrs, long offset, long bytes) {
    if (adrs == NULL || offset < 0 || bytes <= 0 || (bytes & (MCRD_SECTOR_ALIGN - 1)) != 0) {
        return McErrCardInvalid;
    }
    if (psyz_lseek(fd, (unsigned int)offset, SEEK_SET) == (unsigned long)-1) {
        return McErrCardInvalid;
    }
    return psyz_read(fd, adrs, (unsigned int)bytes) == bytes ? McErrNone : McErrCardInvalid;
}

static long mcrd_write_fd(int fd, unsigned long *adrs, long offset, long bytes) {
    if (adrs == NULL || offset < 0 || bytes <= 0 || (bytes & (MCRD_SECTOR_ALIGN - 1)) != 0) {
        return McErrCardInvalid;
    }
    if (psyz_lseek(fd, (unsigned int)offset, SEEK_SET) == (unsigned long)-1) {
        return McErrCardInvalid;
    }
    return psyz_write(fd, adrs, (unsigned int)bytes) == bytes ? McErrNone : McErrCardInvalid;
}

long MemCardReadData(unsigned long *adrs, long offset, long bytes) {
    long err;

    if (mcrd_open_fd < 0 || mcrd_open_flag != O_RDONLY || bytes <= 0 ||
        (bytes & (MCRD_SECTOR_ALIGN - 1)) != 0) {
        return 0;
    }
    err = mcrd_read_fd(mcrd_open_fd, adrs, offset, bytes);
    mcrd_finish_async(McFuncReadData, err);
    return 1;
}

long MemCardWriteData(unsigned long *adrs, long offset, long bytes) {
    long err;

    if (mcrd_open_fd < 0 || mcrd_open_flag != O_WRONLY || bytes <= 0 ||
        (bytes & (MCRD_SECTOR_ALIGN - 1)) != 0) {
        return 0;
    }
    err = mcrd_write_fd(mcrd_open_fd, adrs, offset, bytes);
    mcrd_finish_async(McFuncWriteData, err);
    return 1;
}

long MemCardReadFile(long chan, char *file, unsigned long *adrs, long offset, long bytes) {
    char path[MCRD_PATH_CAP];
    int fd;
    long err;

    if (mcrd_open_fd >= 0 || bytes <= 0 || (bytes & (MCRD_SECTOR_ALIGN - 1)) != 0) {
        return 0;
    }
    if (!mcrd_chan_prefix(chan, path)) {
        mcrd_finish_async(McFuncReadFile, McErrCardNotExist);
        return 1;
    }
    if (!mcrd_build_path(chan, file, path)) {
        mcrd_finish_async(McFuncReadFile, McErrFileNotExist);
        return 1;
    }
    fd = psyz_open(path, FREAD);
    if (fd < 0) {
        mcrd_finish_async(McFuncReadFile, McErrFileNotExist);
        return 1;
    }
    err = mcrd_read_fd(fd, adrs, offset, bytes);
    psyz_close(fd);
    mcrd_finish_async(McFuncReadFile, err);
    return 1;
}

long MemCardWriteFile(long chan, char *file, unsigned long *adrs, long offset, long bytes) {
    char path[MCRD_PATH_CAP];
    char host_path[MCRD_HOST_PATH_CAP];

    if (mcrd_open_fd >= 0 || bytes <= 0 || (bytes & (MCRD_SECTOR_ALIGN - 1)) != 0) {
        return 0;
    }
    if (!mcrd_chan_prefix(chan, path)) {
        mcrd_finish_async(McFuncWriteFile, McErrCardNotExist);
        return 1;
    }
    if (!mcrd_build_path(chan, file, path)) {
        mcrd_finish_async(McFuncWriteFile, McErrFileNotExist);
        return 1;
    }
    Psyz_AdjustPath(host_path, path, (int)sizeof(host_path));
    switch (card_write_file(host_path, adrs, offset, (unsigned long)bytes)) {
    case CARD_WRITE_OK:
        mcrd_finish_async(McFuncWriteFile, McErrNone);
        break;
    case CARD_WRITE_MISSING:
        mcrd_finish_async(McFuncWriteFile, McErrFileNotExist);
        break;
    case CARD_WRITE_FAILED:
        mcrd_finish_async(McFuncWriteFile, McErrCardInvalid);
        break;
    }
    return 1;
}

long MemCardFormat(long chan) {
    char prefix[6];
    char query[8];
    char names[MCRD_MAX_BLOCKS][MCRD_MAX_NAME_LEN];
    char path[MCRD_PATH_CAP];
    struct DIRENTRY entry;
    struct DIRENTRY *it;
    int count = 0;
    int i;

    if (!mcrd_chan_prefix(chan, prefix)) {
        return McErrCardNotExist;
    }
    snprintf(query, sizeof(query), "%s*", prefix);
    for (it = firstfile(query, &entry); it != NULL && count < MCRD_MAX_BLOCKS;
         it = nextfile(&entry)) {
        snprintf(names[count], MCRD_MAX_NAME_LEN, "%s", entry.name);
        count++;
    }
    for (i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "%s%s", prefix, names[i]);
        erase(path);
    }
    return McErrNone;
}

long MemCardUnformat(long chan) {
    char prefix[6];
    return mcrd_chan_prefix(chan, prefix) ? 1 : 0;
}

long psyz_card_rename(const char *old_name, const char *new_name) {
    char old_path[256];
    char new_path[256];

    if (old_name == NULL || new_name == NULL) {
        return 0;
    }
    Psyz_AdjustPath(old_path, old_name, (int)sizeof(old_path));
    Psyz_AdjustPath(new_path, new_name, (int)sizeof(new_path));
    return rename(old_path, new_path) == 0 ? 1 : 0;
}

/* ---- libmcx (PocketStation absent on PC) ---- */

/* shortcut: no PocketStation hardware emulation, add only if PDA mini-apps are targeted */
static int mcx_sync_state = McxSyncNone;
static long mcx_sync_cmd = 0;
static long mcx_sync_result = McxErrInvalid;

static int mcx_register_absent(int port, long cmd) {
    char prefix[6];

    mcx_sync_cmd = cmd;
    mcx_sync_result = mcrd_chan_prefix(port, prefix) ? McxErrInvalid : McxErrNoCard;
    mcx_sync_state = McxSyncFin;
    return 1;
}

void McxStartCom(void) {
    mcx_sync_state = McxSyncNone;
}

void McxStopCom(void) {
    mcx_sync_state = McxSyncNone;
}

int McxSync(int mode, long *cmd, long *result) {
    (void)mode;
    if (mcx_sync_state == McxSyncNone) {
        return McxSyncNone;
    }
    if (cmd != NULL) {
        *cmd = mcx_sync_cmd;
    }
    if (result != NULL) {
        *result = mcx_sync_result;
    }
    mcx_sync_state = McxSyncNone;
    return McxSyncFin;
}

int McxCardType(int port) {
    return mcx_register_absent(port, McxFuncCardType);
}

int McxAllInfo(int port, unsigned char *state) {
    (void)state;
    return mcx_register_absent(port, McxFuncAllInfo);
}

int McxCurrCtrl(int port, int sound, int infred, int led) {
    (void)sound;
    (void)infred;
    (void)led;
    return mcx_register_absent(port, McxFuncCurrCtrl);
}

int McxExecApl(int port, int aplno, long arg) {
    (void)aplno;
    (void)arg;
    return mcx_register_absent(port, McxFuncExecApl);
}

int McxExecFlag(int port, int block, int exec) {
    (void)block;
    (void)exec;
    return mcx_register_absent(port, McxFuncExecFlag);
}

int McxFlashAcs(int port, int mode) {
    (void)mode;
    return mcx_register_absent(port, McxFuncFlashAcs);
}

int McxGetApl(int port, int *aplno) {
    (void)aplno;
    return mcx_register_absent(port, McxFuncGetApl);
}

int McxGetMem(int port, unsigned char *data, unsigned start, unsigned len) {
    (void)data;
    (void)start;
    (void)len;
    return mcx_register_absent(port, McxFuncGetMem);
}

int McxGetSerial(int port, unsigned long *serial) {
    (void)serial;
    return mcx_register_absent(port, McxFuncGetSerial);
}

int McxGetTime(int port, unsigned char *time) {
    (void)time;
    return mcx_register_absent(port, McxFuncGetTime);
}

int McxHideTrans(int port) {
    return mcx_register_absent(port, McxFuncHideTrans);
}

int McxReadDev(int port, int dev, unsigned char *param, unsigned char *data) {
    (void)dev;
    (void)param;
    (void)data;
    return mcx_register_absent(port, McxFuncReadDev);
}

int McxSetLED(int port, int mode) {
    (void)mode;
    return mcx_register_absent(port, McxFuncSetLED);
}

int McxSetMem(int port, unsigned char *data, unsigned start, unsigned len) {
    (void)data;
    (void)start;
    (void)len;
    return mcx_register_absent(port, McxFuncSetMem);
}

int McxShowTrans(int port, int dir, int timeout) {
    (void)dir;
    (void)timeout;
    return mcx_register_absent(port, McxFuncShowTrans);
}

int McxWriteDev(int port, int dev, unsigned char *param, unsigned char *data) {
    (void)dev;
    (void)param;
    (void)data;
    return mcx_register_absent(port, McxFuncWriteDev);
}
