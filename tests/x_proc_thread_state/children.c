/* Child-process entry points of the area. */
#include "xpt.h"

CHILD(xpt_sleeper) { return xpt_sleeper_main(argc, argv); }
CHILD(xpt_quick) { return (int)XPT_CHILD_EXIT; }
/* Child that exits with a code after creating a second thread that is still parked. */
static DWORD WINAPI xpt_child_park(LPVOID p) { Sleep(20000); return 0; }
CHILD(xpt_quick_mt) {
    HANDLE h = CreateThread(NULL, 0, xpt_child_park, NULL, 0, NULL);
    if (h) CloseHandle(h);
    Sleep(50);
    ExitProcess(0x2B);
    return 0x2B;
}
