#include <libmcrd.h>
#include <libmcx.h>
#include <stdio.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    MemCardInit(0);
    MemCardStart();
    McxStartCom();
    s32 command = -1, result = -1;
    CHECK(MemCardSync(0, &command, &result) == -1 && result == McErrCardNotExist);
    CHECK(MemCardAccept(0) == 1 && MemCardExist(0x10) == 0);
    CHECK(MemCardSync(1, &command, &result) == 1 && result == McErrCardNotExist);
    CHECK(MemCardSync(1, &command, &result) == -1);
    CHECK(MemCardExist(0x10) == 1);
    CHECK(MemCardSync(0, &command, &result) == 1 && result == McErrCardNotExist);
    CHECK(MemCardAccept(1) == 0);
    CHECK(McxCardType(0) == 1 && McxCardType(0) == 0);
    CHECK(McxSync(1, &command, &result) == 1 && result == McxErrNoCard);
    CHECK(McxSync(0, &command, &result) == -1);
    return 0;
}
