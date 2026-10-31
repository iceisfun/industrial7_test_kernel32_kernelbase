/* Minimal test harness. kernel32/kernelbase entry points under test are never
 * called directly: every call goes through GetProcAddress. Signatures come from
 * the SDK headers via typeof. (The harness itself uses only a few trivial APIs.) */
#ifndef T_H
#define T_H
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>

HMODULE t_kernel32(void);
HMODULE t_kernelbase(void);
HMODULE t_ntdll(void);
HMODULE t_user32(void);
/* Lookup order: kernel32 -> kernelbase -> ntdll -> user32. */
FARPROC t_proc(const char *name);
FARPROC t_proc_k32(const char *name);   /* kernel32.dll export only (forwarders resolve) */
FARPROC t_proc_kb(const char *name);    /* kernelbase.dll export only */
FARPROC t_proc_nt(const char *name);    /* ntdll.dll export only */

/* U(CreateFileW)(...): pointer typed from the SDK declaration, resolved kernel32 -> kernelbase -> ntdll. */
#define U(name)  ((__typeof__(&name))t_proc(#name))
#define UK(name) ((__typeof__(&name))t_proc_k32(#name))
#define UB(name) ((__typeof__(&name))t_proc_kb(#name))
#define UNT(name) ((__typeof__(&name))t_proc_nt(#name))
/* Declare a pointer type for functions missing from the mingw headers. */
#define UDECL(ret, name, args) typedef ret (WINAPI *name##_pfn) args
#define UN(name) ((name##_pfn)t_proc(#name))

typedef void (*t_fn)(void);
void t_register(const char *name, t_fn fn);
#define TEST(n) \
    static void test_##n(void); \
    static void __attribute__((constructor)) reg_##n(void) { t_register(#n, test_##n); } \
    static void test_##n(void)

/* Child-process entry points: the test exe re-launches itself as
 *   exe --child <name> [args...]
 * CHILD(name) { ... return exitcode; } receives argc/argv (argv[0]=name). */
typedef int (*t_child_fn)(int argc, char **argv);
void t_register_child(const char *name, t_child_fn fn);
#define CHILD(n) \
    static int child_##n(int argc, char **argv); \
    static void __attribute__((constructor)) regc_##n(void) { t_register_child(#n, child_##n); } \
    static int child_##n(int argc, char **argv)
/* Launch self as a child: returns process handle (caller closes), NULL on failure. */
HANDLE t_spawn_child(const char *name, const char *extra_args, DWORD creation_flags, HANDLE hin, HANDLE hout, HANDLE herr, BOOL inherit, const char *env_block_or_null, PROCESS_INFORMATION *pi_out);
/* Run child to completion (<= timeout_ms), return its exit code, or 0xDEAD0000 on spawn failure / 0xDEAD0001 on timeout (child killed). */
DWORD t_run_child(const char *name, const char *extra_args, DWORD timeout_ms);
const char *t_self_path(void);

/* Private scratch directory, created on first use under %TEMP%, unique per process:
 *   ...\k32t_<pid>\   (trailing backslash). Removed (recursively) at exit. */
const char *t_tmpdir(void);
void t_tmpfile(char *out, size_t cap, const char *leaf);   /* tmpdir + leaf */

void t_check(int ok, const char *file, int line, const char *expr, const char *fmt, ...);
#define CHECK(c)            t_check(!!(c), __FILE__, __LINE__, #c, NULL)
#define CHECKF(c, ...)      t_check(!!(c), __FILE__, __LINE__, #c, __VA_ARGS__)
#define EXPECT_EQ(a, b)     t_check((ULONG_PTR)(a) == (ULONG_PTR)(b), __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)(ULONG_PTR)(a), (unsigned long long)(ULONG_PTR)(b))
#define EXPECT_NE(a, b)     t_check((ULONG_PTR)(a) != (ULONG_PTR)(b), __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)(ULONG_PTR)(a))
#define EXPECT_STR(a, b)    t_check(!strcmp((a), (b)), __FILE__, __LINE__, #a " == " #b, "got \"%s\" want \"%s\"", (a), (b))
#define EXPECT_WSTR(a, b)   t_check(!lstrcmpW((a), (b)), __FILE__, __LINE__, #a " == " #b, "wide string mismatch")
/* GetLastError expectation: call right after the failing call. */
#define EXPECT_ERR(err) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #err, "GetLastError got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define SET_ERR(e)          SetLastError(e)
void t_note(const char *fmt, ...);   /* observed behaviour, never fails */
void t_skip(const char *fmt, ...);   /* skip rest of current test */
#define REQUIRE(ptr) do { if (!(ptr)) { t_skip("missing export: " #ptr); return; } } while (0)
int t_is_wine(void);                 /* true under an emulator exposing wine_get_version */

#endif
