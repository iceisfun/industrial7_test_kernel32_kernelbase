#define K_IS_LOCAL 1
#define K_P local
#define K_ALLOC LocalAlloc
#define K_REALLOC LocalReAlloc
#define K_FREE LocalFree
#define K_LOCK LocalLock
#define K_UNLOCK LocalUnlock
#define K_FLAGS LocalFlags
#define K_SIZE LocalSize
#define K_HANDLE LocalHandle
#define K_DISCARDABLE 0xF00
#define K_DISCMASK 0xF00
#include "lg_tmpl.h"
