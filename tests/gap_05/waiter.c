#include "g5.h"
/* Child: signal ready, then wait (bounded) for quit. */
CHILD(gap5_waiter) {
    if (argc < 3) return 2;
    HANDLE r = OpenEventA(EVENT_MODIFY_STATE, FALSE, argv[1]);
    HANDLE q = OpenEventA(SYNCHRONIZE, FALSE, argv[2]);
    if (!r || !q) return 3;
    SetEvent(r);
    WaitForSingleObject(q, 20000);
    return 0;
}
