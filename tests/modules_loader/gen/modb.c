/* imports a function that k32m_dep.dll does not export */
extern int __stdcall ml_nosuch(void);
int __stdcall ml_via_bad(void) { return ml_nosuch(); }
int __stdcall __attribute__((used)) DllMain(void *h, unsigned long reason, void *r) { return 1; }
