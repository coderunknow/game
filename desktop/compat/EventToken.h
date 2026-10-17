/* Minimal Windows SDK EventToken.h substitute for mingw/zig cross-builds.
 * WebView2.h only needs the EventRegistrationToken type from that header. */
#ifndef __EVENTTOKEN_H__
#define __EVENTTOKEN_H__
#include <windows.h>
typedef struct _EventRegistrationToken {
    INT64 value;
} EventRegistrationToken;
#endif
