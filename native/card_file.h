#ifndef VANILLABTN_CARD_FILE_H
#define VANILLABTN_CARD_FILE_H

enum CardWriteResult {
    CARD_WRITE_OK,
    CARD_WRITE_MISSING,
    CARD_WRITE_FAILED,
};

enum CardWriteResult card_write_file(const char *path, const void *data, long offset,
                                     unsigned long size);

#endif
