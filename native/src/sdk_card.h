#ifndef VANILLABTN_SDK_CARD_H
#define VANILLABTN_SDK_CARD_H

#include <stdbool.h>
#include <stddef.h>
#include "ps1_card.h"

/* Bind or unbind the Ps1CardManager used by libmcrd/libmcx.
 * If NULL, an internal dual-card manager is used by default. */
void sdk_card_bind_manager(Ps1CardManager *mgr);

/* Get the active Ps1Card pointer for port (0 or 1). */
Ps1Card *sdk_card_get(int port);

/* Rename helper called by libapi rename() macro. */
int sdk_card_rename(const char *oldpath, const char *newpath);

#endif /* VANILLABTN_SDK_CARD_H */
