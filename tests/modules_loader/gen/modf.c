/* DllMain refuses process attach */
int __stdcall __attribute__((used)) DllMain(void *h, unsigned long reason, void *r) { return reason == 1 ? 0 : 1; }
int __stdcall ml_f(void) { return 1; }
