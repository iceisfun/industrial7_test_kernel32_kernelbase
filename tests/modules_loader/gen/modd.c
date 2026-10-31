/* dependency DLL: exports ml_dep_fn */
int __stdcall ml_dep_fn(void) { return 4242; }
int __stdcall __attribute__((used)) DllMain(void *h, unsigned long reason, void *r) { return 1; }
