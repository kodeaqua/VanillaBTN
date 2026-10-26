/* Full native text addresses; bank start fields retain their original 32-bit
 * relocation deltas. Never truncate a member pointer into an asset offset. */
#define LOCALIZED(message) ((u16 *)((u8 *)(message) + text_bank_starts.messages))
#define NAME_OFFSET(name) ((uintptr_t)(name) - __builtin_offsetof(TextBank, names))
#define LOCALIZED_NAME(name) \
    ((u16 *)((u8 *)(name) + ((intptr_t)text_bank_starts.names - \
                             (intptr_t)__builtin_offsetof(TextBank, names))))
