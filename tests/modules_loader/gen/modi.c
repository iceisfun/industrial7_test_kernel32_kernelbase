/* importer DLL: imports ml_dep_fn from k32m_dep.dll */
extern int __stdcall ml_dep_fn(void);
int __stdcall ml_via_dep(void) { return ml_dep_fn() + 1; }
int __stdcall __attribute__((used)) DllMain(void *h, unsigned long reason, void *r) { return 1; }
