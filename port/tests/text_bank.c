#include "text_bank.h"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
_Static_assert(sizeof(TextBankStarts) == 12, "text-bank relocation metadata width");
int main(void) {
    CHECK(LOCALIZED(text_bank.messages.name_label) == text_bank.messages.name_label);
    CHECK(LOCALIZED_NAME(text_bank.names.chicken_names.popuri) ==
          text_bank.names.chicken_names.popuri);
    CHECK(text_bank_starts.names + NAME_OFFSET(text_bank.names.honey) ==
          (uintptr_t)text_bank.names.honey);
    CHECK(LOCALIZED(text_bank.messages.name_label)[0] == text_bank.messages.name_label[0]);
    text_bank_starts.messages = 2;
    CHECK(LOCALIZED(text_bank.messages.name_label) == text_bank.messages.name_label + 1);
    text_bank_starts.names += 2;
    CHECK(LOCALIZED_NAME(text_bank.names.honey) == text_bank.names.honey + 1);
    return 0;
}
