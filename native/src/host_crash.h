#ifndef HOST_CRASH_H
#define HOST_CRASH_H

#include <stdio.h>

/* Developer-only Windows diagnostics. The release target has no references to
 * this interface or its implementation. */
void host_crash_install(void (*state_writer)(FILE *));
void host_crash_note_system_error(char type, long code);
/* Open the Debug audio report in the temporary folder, falling back to cwd.
 * Keep Windows SDK declarations out of main.c, which also uses PS1 RECT. */
FILE *host_crash_open_audio_report(void);

#endif
