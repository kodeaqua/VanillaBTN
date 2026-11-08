#include "sdk_cd.h"
#include <libcd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#define CD_SECTOR 2048u
#define CD_FILES 64
typedef struct DiscFile {
    FILE *handle;
    char name[256];
    u_long size, first, sectors;
} DiscFile;
static char root[1024];
static DiscFile files[CD_FILES];
static unsigned file_count;
static u_long next_sector, seek_sector;
static u_char last_command, drive_mode, filter_file, filter_channel;
static bool filter_set, xa_streaming;
static struct {
    DiscFile *file;
    unsigned char *destination;
    u_long offset, sectors;
    int status;
} read_job;

SdkCdXaStatus sdk_cd_xa_status(void) {
    return (SdkCdXaStatus){xa_streaming, drive_mode, filter_file, filter_channel, seek_sector};
}

void sdk_cd_unmount(void) {
    for (unsigned i = 0; i < file_count; ++i) fclose(files[i].handle);
    memset(files, 0, sizeof(files));
    memset(&read_job, 0, sizeof(read_job));
    root[0] = 0;
    file_count = 0;
    next_sector = seek_sector = 0;
    last_command = drive_mode = filter_file = filter_channel = 0;
    filter_set = xa_streaming = false;
}

bool sdk_cd_mount(const char *directory) {
    struct _stat64 info;
    sdk_cd_unmount();
    if (!directory || strlen(directory) >= sizeof(root)
        || _stat64(directory, &info) != 0 || !(info.st_mode & _S_IFDIR)) return false;
    strcpy(root, directory);
    return true;
}

static int decimal(u_char bcd) {
    return (bcd & 15) < 10 && (bcd >> 4) < 10 ? (bcd >> 4) * 10 + (bcd & 15) : -1;
}

static u_char bcd(int value) { return (u_char)((value / 10 << 4) | value % 10); }

CdlLOC *CdIntToPos(int sector, CdlLOC *position) {
    if (!position || sector < 0 || sector > 449849) return NULL;
    int absolute = sector + 150;
    *position = (CdlLOC){bcd(absolute / 4500), bcd(absolute / 75 % 60), bcd(absolute % 75), 0};
    return position;
}

int CdPosToInt(CdlLOC *position) {
    if (!position) return -1;
    int minute = decimal(position->minute), second = decimal(position->second);
    int sector = decimal(position->sector);
    if (minute < 0 || second < 0 || second >= 60 || sector < 0 || sector >= 75) return -1;
    int value = (minute * 60 + second) * 75 + sector - 150;
    return value >= 0 ? value : -1;
}

static bool disc_name(char *output, const char *input) {
    if (!input) return false;
    if (*input == '\\' || *input == '/') ++input;
    size_t length = strcspn(input, ";");
    if (!length || length >= 256 || (input[length] && strcmp(input + length, ";1"))) return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (c < 32 || c == ':' || c == '"' || c == '*'
            || c == '?' || c == '<' || c == '>' || c == '|') return false;
        output[i] = c == '\\' ? '/' : (char)c;
    }
    output[length] = 0;
    for (char *part = output; *part;) {
        size_t count = strcspn(part, "/");
        if (!count || part[count - 1] == '.' || part[count - 1] == ' ') return false;
        part += count;
        if (*part) { ++part; if (!*part) return false; }
    }
    return true;
}

CdlFILE *CdSearchFile(CdlFILE *result, char *name) {
    char relative[256], path[1280];
    if (!result || !root[0] || !disc_name(relative, name)) return NULL;
    DiscFile *file = NULL;
    for (unsigned i = 0; i < file_count; ++i)
        if (_stricmp(files[i].name, relative) == 0) file = files + i;
    if (!file) {
        if (file_count == CD_FILES) return NULL;
        int length = snprintf(path, sizeof(path), "%s/%s", root, relative);
        if (length < 0 || (size_t)length >= sizeof(path)) return NULL;
        FILE *handle = fopen(path, "rb");
        if (!handle) return NULL;
        long bytes;
        if (fseek(handle, 0, SEEK_END) || (bytes = ftell(handle)) < 0) { fclose(handle); return NULL; }
        u_long sectors = ((u_long)bytes + CD_SECTOR - 1) / CD_SECTOR;
        if (sectors == 0 || sectors > 449850 - next_sector) { fclose(handle); return NULL; }
        file = files + file_count++;
        *file = (DiscFile){handle, "", (u_long)bytes, next_sector, sectors};
        strcpy(file->name, relative);
        next_sector += sectors;
    }
    memset(result, 0, sizeof(*result));
    if (!CdIntToPos((int)file->first, &result->pos)) return NULL;
    result->size = file->size;
    strncpy(result->name, relative, sizeof(result->name) - 1);
    return result;
}

static DiscFile *find_sector_file(u_long sector) {
    for (unsigned i = 0; i < file_count; ++i) {
        DiscFile *file = files + i;
        if (sector >= file->first && sector - file->first < file->sectors) return file;
    }
    return NULL;
}

int CdControl(u_char command, u_char *parameter, u_char *result) {
    if (command == CdlStandby || command == CdlPause) {
        xa_streaming = false;
        last_command = command;
        if (result) result[0] = 2;
        return 1;
    }
    if (!root[0]) return 0;
    if (command == CdlSetloc) {
        int sector = CdPosToInt((CdlLOC *)parameter);
        if (sector < 0) return 0;
        seek_sector = (u_long)sector;
    } else if (command == CdlSetfilter) {
        if (!parameter || parameter[0] == 0 || parameter[1] > 31) return 0;
        filter_file = parameter[0];
        filter_channel = parameter[1];
        filter_set = true;
    } else if (command == CdlSetmode) {
        /* Support 2048-byte data modes (0x00/0x80) and the 0xC8 real-time
         * filtered XA-ADPCM mode configured by cd_audio.c. */
        if (!parameter || (*parameter != 0 && *parameter != 0x80 && *parameter != 0xC8)) return 0;
        drive_mode = *parameter;
        if (drive_mode != 0xC8) xa_streaming = false;
    } else if (command == CdlReadS) {
        if (drive_mode != 0xC8 || !filter_set) return 0;
        if (parameter) {
            int sector = CdPosToInt((CdlLOC *)parameter);
            if (sector < 0) return 0;
            seek_sector = (u_long)sector;
        }
        if (!find_sector_file(seek_sector)) return 0;
        xa_streaming = true;
    } else if (command == CdlGetlocP) {
        if (result) {
            CdlLOC pos;
            memset(result, 0, 8);
            if (CdIntToPos((int)seek_sector, &pos)) {
                result[5] = pos.minute;
                result[6] = pos.second;
                result[7] = pos.sector;
            }
        }
    } else return 0;
    last_command = command;
    if (result && command != CdlGetlocP) result[0] = 2;
    return 1;
}

int CdControlB(u_char command, u_char *parameter, u_char *result) {
    return CdControl(command, parameter, result);
}

int CdControlF(u_char command, u_char *parameter) {
    return CdControl(command, parameter, NULL);
}

u_char CdLastCom(void) {
    return last_command;
}

int CdSync(int mode, u_char *result) {
    if (!root[0] || (mode != 0 && mode != 1)) return -1;
    if (result) {
        memset(result, 0, 8);
        result[0] = 2;
        if (last_command == CdlGetlocP) {
            CdlLOC pos;
            if (CdIntToPos((int)seek_sector, &pos)) {
                result[5] = pos.minute;
                result[6] = pos.second;
                result[7] = pos.sector;
            }
        }
    }
    return CdlComplete; /* Supported control commands complete immediately. */
}

int CdRead(int sectors, u_long *buffer, int mode) {
    if (!buffer || sectors <= 0 || read_job.status > 0 || (mode & ~0x80)) return 0;
    drive_mode = (u_char)mode;
    xa_streaming = false;
    for (unsigned i = 0; i < file_count; ++i) {
        DiscFile *file = files + i;
        if (seek_sector >= file->first && seek_sector - file->first < file->sectors
            && (u_long)sectors <= file->sectors - (seek_sector - file->first)) {
            read_job.file = file;
            read_job.destination = (unsigned char *)buffer;
            read_job.offset = (seek_sector - file->first) * CD_SECTOR;
            read_job.sectors = (u_long)sectors;
            read_job.status = sectors;
            return 1;
        }
    }
    return 0;
}

int CdReadSync(int mode, u_char *result) {
    if (!root[0] || (mode != 0 && mode != 1)) return -1;
    if (read_job.status > 0) {
        DiscFile *file = read_job.file;
        size_t size = (size_t)read_job.sectors * CD_SECTOR;
        size_t bytes = file->size - read_job.offset;
        if (bytes > size) bytes = size;
        /* Completion happens here for both wait and poll. Host file I/O is
         * synchronous; no artificial busy ticks or emulated seek delays. */
        if (fseek(file->handle, (long)read_job.offset, SEEK_SET)
            || fread(read_job.destination, 1, bytes, file->handle) != bytes) read_job.status = -1;
        else {
            memset(read_job.destination + bytes, 0, size - bytes);
            read_job.status = 0;
            seek_sector = file->first + read_job.offset / CD_SECTOR + read_job.sectors;
        }
    }
    if (result) result[0] = read_job.status < 0 ? 1 : 2;
    return read_job.status;
}

void CdReadBreak(void) { memset(&read_job, 0, sizeof(read_job)); }
void CdFlush(void) { /* No unread controller response FIFO exists on the host. */ }
