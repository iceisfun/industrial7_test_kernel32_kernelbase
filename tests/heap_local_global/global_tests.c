#define K_IS_LOCAL 0
#define K_P global
#define K_ALLOC GlobalAlloc
#define K_REALLOC GlobalReAlloc
#define K_FREE GlobalFree
#define K_LOCK GlobalLock
#define K_UNLOCK GlobalUnlock
#define K_FLAGS GlobalFlags
#define K_SIZE GlobalSize
#define K_HANDLE GlobalHandle
#define K_DISCARDABLE 0x100
#define K_DISCMASK 0x100
#include "lg_tmpl.h"
