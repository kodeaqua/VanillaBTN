#ifndef HOST_CRASH_H
#define HOST_CRASH_H

#include <stdio.h>

/* Developer-only Windows diagnostics. The release target has no references to
 * this interface or its implementation. */
void host_crash_install(void (*state_writer)(FILE *));
void host_crash_note_system_error(char type, long code);

#endif
