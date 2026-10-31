/* Tiny CRT-free test DLL (no imports). Built by gen.sh, embedded as modblob.h. */
#include <stdint.h>
#define EXP __declspec(dllexport)
EXP volatile int ml_proc_attach = 0;
EXP volatile int ml_thread_attach = 0;
EXP volatile int ml_thread_detach = 0;
EXP int ml_value = 0x1234;
int __stdcall ml_add(int a, int b) { return a + b; }
int __stdcall ml_ord7(void) { return 77; }
int __stdcall ml_case(void) { return 5; }
int __stdcall ml_noname(void) { return 99; }
int __stdcall __attribute__((used)) DllMain(void *h, unsigned long reason, void *r) {
    if (reason == 1) ml_proc_attach++;
    else if (reason == 2) ml_thread_attach++;
    else if (reason == 3) ml_thread_detach++;
    return 1;
}
