#ifndef SDK_ERROR_H
#define SDK_ERROR_H

#ifdef _DEBUG
/* The original error path aborts. A native Debug host can record why before
 * the crash handler collects the call stack. */
void sdk_error_set_hook(void (*hook)(char type, long code));
#endif

#endif
