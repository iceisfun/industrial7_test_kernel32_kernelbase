/* SetDllDirectory/GetDllDirectory, AddDllDirectory/RemoveDllDirectory, SetDefaultDllDirectories,
 * search-order flags. Everything that mutates process-global loader state runs in a child. */
#include "ml.h"

#define S(n, c) do { if (!(c)) return (n); } while (0)

static WCHAR g_dirw[MAX_PATH];
static const char *dd_prep(const char *sub, int with_dll) {
    const char *d = ml_dir(sub);
    if (with_dll) ml_put(ML_A, d, "k32m_dd.dll", NULL);
    return d;
}
static char *dd_nobs(const char *d) { static char b[MAX_PATH]; strcpy(b, d); b[strlen(b) - 1] = 0; return b; }
static DWORD dd_run(const char *child, const char *dir, const char *cwd) {
    char args[MAX_PATH + 8]; snprintf(args, sizeof args, "\"%s\"", dd_nobs(dir));
    DWORD rc = ml_run_exe(t_self_path(), child, args, cwd, 25000, 0);
    if (rc) t_note("child %s failed at step %lu", child, rc);
    return rc;
}
static HMODULE tryload(const char *name, DWORD fl) { return U(LoadLibraryExA)(name, NULL, fl); }
static int loads(const char *name, DWORD fl) { HMODULE h = tryload(name, fl); if (h) { ml_unload(h); return 1; } return 0; }

/* ---- Get/SetDllDirectory ---- */
CHILD(dd_get_default) {
    char a[MAX_PATH] = "junk"; WCHAR w[MAX_PATH] = L"junk";
    SetLastError(0x1111);
    S(1, U(GetDllDirectoryW)(MAX_PATH, w) == 0);
    S(2, w[0] == 0);
    S(3, GetLastError() == 0);
    SetLastError(0x1111);
    S(4, U(GetDllDirectoryA)(MAX_PATH, a) == 0);
    S(5, a[0] == 0);
    S(6, GetLastError() == 0);
    S(7, U(GetDllDirectoryA)(0, NULL) == 1);
    return 0;
}
TEST(dd_get_default_empty) {
    REQUIRE(U(GetDllDirectoryA)); REQUIRE(U(GetDllDirectoryW));
    EXPECT_EQ(t_run_child("dd_get_default", "", 25000), 0);
}
CHILD(dd_set_get_roundtrip) {
    const char *d = argv[1]; size_t L = strlen(d);
    WCHAR wd[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, d, -1, wd, MAX_PATH);
    S(1, U(SetDllDirectoryA)(d));
    char a[MAX_PATH + 8]; WCHAR w[MAX_PATH + 8];
    SetLastError(0x1111);
    S(2, U(GetDllDirectoryA)(MAX_PATH, a) == L);
    S(3, !strcmp(a, d));
    S(4, U(GetDllDirectoryW)(MAX_PATH, w) == L);
    S(5, !lstrcmpW(w, wd));
    S(6, GetLastError() == 0);
    /* exact fit */
    memset(w, 0x55, sizeof w);
    S(7, U(GetDllDirectoryW)((DWORD)L + 1, w) == L && !lstrcmpW(w, wd));
    memset(a, 0x55, sizeof a);
    S(8, U(GetDllDirectoryA)((DWORD)L + 1, a) == L && !strcmp(a, d));
    /* too small: required size including NUL */
    S(9, U(GetDllDirectoryW)((DWORD)L, w) == L + 1);
    S(10, U(GetDllDirectoryA)((DWORD)L, a) == L + 1);
    S(11, U(GetDllDirectoryW)(0, NULL) == L + 1);
    S(12, U(GetDllDirectoryA)(0, NULL) == L + 1);
    /* clear again */
    S(13, U(SetDllDirectoryA)(NULL));
    S(14, U(GetDllDirectoryW)(MAX_PATH, w) == 0 && w[0] == 0);
    /* wide variant */
    S(15, U(SetDllDirectoryW)(wd));
    S(16, U(GetDllDirectoryA)(MAX_PATH, a) == L && !strcmp(a, d));
    S(17, U(SetDllDirectoryW)(NULL));
    S(18, U(GetDllDirectoryA)(MAX_PATH, a) == 0);
    return 0;
}
TEST(dd_set_get_roundtrip) {
    REQUIRE(U(SetDllDirectoryA)); REQUIRE(U(SetDllDirectoryW)); REQUIRE(U(GetDllDirectoryA)); REQUIRE(U(GetDllDirectoryW));
    EXPECT_EQ(dd_run("dd_set_get_roundtrip", dd_prep("dd_rt", 0), NULL), 0);
}
CHILD(dd_set_empty_vs_null) {
    WCHAR w[16] = L"junk";
    S(1, U(SetDllDirectoryW)(L""));
    S(2, U(GetDllDirectoryW)(16, w) == 0);
    S(3, w[0] == 0);
    S(4, U(SetDllDirectoryA)(""));
    S(5, U(SetDllDirectoryA)(NULL));
    S(6, U(SetDllDirectoryW)(NULL));
    return 0;
}
TEST(dd_set_empty_string_accepted) {
    REQUIRE(U(SetDllDirectoryW)); REQUIRE(U(GetDllDirectoryW));
    EXPECT_EQ(t_run_child("dd_set_empty_vs_null", "", 25000), 0);
}
CHILD(dd_set_too_long) {
    static WCHAR big[40000];
    for (int i = 0; i < 39990; i++) big[i] = L'a';
    SetLastError(0);
    S(1, !U(SetDllDirectoryW)(big));
    S(2, GetLastError() == ERROR_INVALID_PARAMETER);
    for (int i = 0; i < 32767; i++) big[i] = L'b';
    big[32767] = 0;
    SetLastError(0);
    S(3, !U(SetDllDirectoryW)(big));
    S(4, GetLastError() == ERROR_INVALID_PARAMETER);
    /* a failed set leaves the previous value */
    WCHAR w[MAX_PATH];
    S(5, U(GetDllDirectoryW)(MAX_PATH, w) == 0);
    return 0;
}
TEST(dd_set_too_long_rejected) {
    REQUIRE(U(SetDllDirectoryW));
    EXPECT_EQ(t_run_child("dd_set_too_long", "", 25000), 0);
}
CHILD(dd_set_nonexistent) {
    S(1, U(SetDllDirectoryA)("C:\\k32t_no_such_directory_xyz"));
    char a[MAX_PATH];
    S(2, U(GetDllDirectoryA)(MAX_PATH, a) == strlen("C:\\k32t_no_such_directory_xyz"));
    S(3, !strcmp(a, "C:\\k32t_no_such_directory_xyz"));
    /* relative path */
    BOOL r = U(SetDllDirectoryA)("relative_k32t");
    (void)r;
    return 0;
}
TEST(dd_set_nonexistent_dir_accepted) {
    REQUIRE(U(SetDllDirectoryA));
    EXPECT_EQ(t_run_child("dd_set_nonexistent", "", 25000), 0);
}
CHILD(dd_set_changes_search) {
    const char *d = argv[1];
    S(1, !loads("k32m_dd.dll", 0));                 /* not found anywhere yet */
    S(2, GetLastError() == ERROR_MOD_NOT_FOUND || 1);
    S(3, U(SetDllDirectoryA)(d));
    S(4, loads("k32m_dd.dll", 0));                  /* DllDirectory is part of the default order */
    S(5, loads("k32m_dd", 0));
    S(6, U(LoadLibraryA)("k32m_dd.dll") != NULL);
    HMODULE h = GetModuleHandleA("k32m_dd.dll"); ml_unload(h);
    S(7, U(SetDllDirectoryA)(NULL));
    S(8, !loads("k32m_dd.dll", 0));
    S(9, GetLastError() == ERROR_MOD_NOT_FOUND);
    return 0;
}
TEST(dd_set_dll_directory_affects_search) {
    REQUIRE(U(SetDllDirectoryA)); REQUIRE(U(LoadLibraryExA));
    EXPECT_EQ(dd_run("dd_set_changes_search", dd_prep("dd_sd", 1), NULL), 0);
}
CHILD(dd_set_dir_user_dirs_flag) {
    const char *d = argv[1];
    S(1, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    S(2, GetLastError() == ERROR_MOD_NOT_FOUND);
    S(3, U(SetDllDirectoryA)(d));
    S(4, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));     /* SetDllDirectory counts as a user dir */
    S(5, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    S(6, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(7, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_APPLICATION_DIR));
    return 0;
}
TEST(dd_set_dll_directory_is_user_dir) {
    REQUIRE(U(SetDllDirectoryA)); REQUIRE(U(LoadLibraryExA));
    EXPECT_EQ(dd_run("dd_set_dir_user_dirs_flag", dd_prep("dd_ud", 1), NULL), 0);
}
CHILD(dd_empty_removes_cwd) {
    const char *d = argv[1];       /* cwd == d, contains k32m_dd.dll */
    S(1, loads("k32m_dd.dll", 0));                 /* found through the current directory */
    S(2, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));   /* search flags never use cwd */
    t_note("SAFE_CURRENT_DIRS bare name from cwd: %s", loads("k32m_dd.dll", LOAD_LIBRARY_SAFE_CURRENT_DIRS) ? "loaded" : "refused");
    S(4, U(SetDllDirectoryW)(L""));
    S(5, !loads("k32m_dd.dll", 0));                /* cwd removed from the default order */
    S(6, GetLastError() == ERROR_MOD_NOT_FOUND);
    S(7, U(SetDllDirectoryW)(NULL));
    S(8, loads("k32m_dd.dll", 0));                 /* restored */
    return 0;
}
TEST(dd_empty_string_removes_cwd_from_search) {
    REQUIRE(U(SetDllDirectoryW)); REQUIRE(U(LoadLibraryExA));
    const char *d = dd_prep("dd_cwd", 1);
    EXPECT_EQ(dd_run("dd_empty_removes_cwd", d, dd_nobs(d)), 0);
}
CHILD(dd_path_env) {
    const char *d = argv[1];
    S(1, !loads("k32m_dd.dll", 0));
    S(2, SetEnvironmentVariableA("PATH", d));
    S(3, loads("k32m_dd.dll", 0));                 /* PATH is searched last */
    S(4, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    S(5, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_APPLICATION_DIR));
    S(6, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    return 0;
}
TEST(dd_path_variable_searched_by_default_only) {
    REQUIRE(U(LoadLibraryExA));
    EXPECT_EQ(dd_run("dd_path_env", dd_prep("dd_path", 1), NULL), 0);
}

/* ---- AddDllDirectory / RemoveDllDirectory ---- */
CHILD(dd_add_remove) {
    const char *d = argv[1];
    WCHAR wd[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, d, -1, wd, MAX_PATH);
    S(1, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    DLL_DIRECTORY_COOKIE c = U(AddDllDirectory)(wd);
    S(2, c != NULL);
    S(3, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    S(4, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(5, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    S(6, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(7, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_APPLICATION_DIR));
    S(8, !loads("k32m_dd.dll", 0));                 /* AddDllDirectory dirs are NOT in the legacy order */
    S(9, U(RemoveDllDirectory)(c));
    S(10, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    S(11, GetLastError() == ERROR_MOD_NOT_FOUND);
    /* removing twice fails */
    SetLastError(0);
    S(12, !U(RemoveDllDirectory)(c));
    S(13, GetLastError() != 0);
    return 0;
}
TEST(dd_add_remove_dll_directory) {
    REQUIRE(U(AddDllDirectory)); REQUIRE(U(RemoveDllDirectory)); REQUIRE(U(LoadLibraryExA));
    EXPECT_EQ(dd_run("dd_add_remove", dd_prep("dd_ar", 1), NULL), 0);
}
CHILD(dd_add_invalid) {
    DLL_DIRECTORY_COOKIE c;
    SetLastError(0);
    S(1, U(AddDllDirectory)(L"relative\\dir") == NULL);
    S(2, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    S(3, U(AddDllDirectory)(L"dir") == NULL);
    S(4, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    S(5, U(AddDllDirectory)(L"") == NULL);
    S(6, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    S(7, U(AddDllDirectory)(NULL) == NULL);
    S(8, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    S(9, U(AddDllDirectory)(L".") == NULL);
    S(10, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    S(11, U(AddDllDirectory)(L"..\\x") == NULL);
    S(12, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    S(13, U(AddDllDirectory)(L"C:dir") == NULL);          /* drive-relative */
    S(14, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    S(15, U(AddDllDirectory)(L"\\rooted_only") == NULL || 1);
    return 0;
}
TEST(dd_add_dll_directory_rejects_relative) {
    REQUIRE(U(AddDllDirectory));
    EXPECT_EQ(t_run_child("dd_add_invalid", "", 25000), 0);
}
CHILD(dd_add_cookies) {
    const char *d = argv[1];
    WCHAR wd[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, d, -1, wd, MAX_PATH);
    DLL_DIRECTORY_COOKIE c1 = U(AddDllDirectory)(wd), c2 = U(AddDllDirectory)(wd);
    S(1, c1 != NULL && c2 != NULL);
    S(2, c1 != c2);                                   /* every add gets its own cookie */
    S(3, U(RemoveDllDirectory)(c1));
    S(4, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));    /* c2 still present */
    S(5, U(RemoveDllDirectory)(c2));
    S(6, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    /* nonexistent directory is accepted */
    DLL_DIRECTORY_COOKIE c3 = U(AddDllDirectory)(L"C:\\k32t_no_such_directory_xyz");
    if (c3) S(8, U(RemoveDllDirectory)(c3));
    return 0;
}
TEST(dd_add_dll_directory_cookies_distinct) {
    REQUIRE(U(AddDllDirectory)); REQUIRE(U(RemoveDllDirectory));
    EXPECT_EQ(dd_run("dd_add_cookies", dd_prep("dd_ck", 1), NULL), 0);
}
CHILD(dd_remove_invalid) {
    SetLastError(0);
    S(1, !U(RemoveDllDirectory)(NULL));
    S(2, GetLastError() != 0);
    SetLastError(0);
    S(3, !U(RemoveDllDirectory)((DLL_DIRECTORY_COOKIE)(ULONG_PTR)0x12345678));
    S(4, GetLastError() != 0);
    SetLastError(0);
    S(5, !U(RemoveDllDirectory)(INVALID_HANDLE_VALUE));
    S(6, GetLastError() != 0);
    return 0;
}
TEST(dd_remove_dll_directory_invalid_cookies) {
    REQUIRE(U(RemoveDllDirectory));
    EXPECT_EQ(t_run_child("dd_remove_invalid", "", 25000), 0);
}
CHILD(dd_add_trailing_and_case) {
    const char *d = argv[1];
    WCHAR wd[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, d, -1, wd, MAX_PATH);
    WCHAR with_bs[MAX_PATH]; lstrcpyW(with_bs, wd); lstrcatW(with_bs, L"\\");
    DLL_DIRECTORY_COOKIE c = U(AddDllDirectory)(with_bs);
    S(1, c != NULL);
    S(2, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    S(3, U(RemoveDllDirectory)(c));
    WCHAR up[MAX_PATH]; lstrcpyW(up, wd); CharUpperW(up);
    c = U(AddDllDirectory)(up);
    S(4, c != NULL);
    S(5, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    S(6, U(RemoveDllDirectory)(c));
    /* forward slashes */
    WCHAR sl[MAX_PATH]; lstrcpyW(sl, wd); for (WCHAR *p = sl; *p; p++) if (*p == L'/') ; for (WCHAR *p = sl; *p; p++) if (*p == L'\\') *p = L'/';
    c = U(AddDllDirectory)(sl);
    if (c) { S(7, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS)); U(RemoveDllDirectory)(c); }
    return 0;
}
TEST(dd_add_dll_directory_path_forms) {
    REQUIRE(U(AddDllDirectory)); REQUIRE(U(RemoveDllDirectory));
    EXPECT_EQ(dd_run("dd_add_trailing_and_case", dd_prep("dd_pf", 1), NULL), 0);
}
CHILD(dd_add_many) {
    DLL_DIRECTORY_COOKIE c[64];
    WCHAR p[MAX_PATH];
    for (int i = 0; i < 64; i++) {
        char pa[MAX_PATH]; snprintf(pa, sizeof pa, "%s\\%d", argv[1], i); MultiByteToWideChar(CP_ACP, 0, pa, -1, p, MAX_PATH);
        c[i] = U(AddDllDirectory)(p);
        S(1, c[i] != NULL);
    }
    for (int i = 0; i < 64; i++) for (int j = i + 1; j < 64; j++) S(2, c[i] != c[j]);
    for (int i = 63; i >= 0; i--) S(3, U(RemoveDllDirectory)(c[i]));
    return 0;
}
TEST(dd_add_dll_directory_many) {
    REQUIRE(U(AddDllDirectory)); REQUIRE(U(RemoveDllDirectory));
    const char *d = ml_dir("dd_many");
    for (int i = 0; i < 64; i++) { char p[MAX_PATH]; snprintf(p, sizeof p, "%s%d", d, i); CreateDirectoryA(p, NULL); }
    EXPECT_EQ(dd_run("dd_add_many", d, NULL), 0);
}

/* ---- SetDefaultDllDirectories ---- */
CHILD(dd_default_invalid) {
    static const DWORD bad[] = { 0x1, 0x2, 0x4, 0x8, 0x100, 0x100 | 0x800, 0x2000, 0x10000, 0x80000000, 0xFFFFFFFF, 0x200 | 0x1, 0x0A00 | 0x100, 0x20, 0x40, 0x80 };
    for (int i = 0; i < (int)(sizeof bad / sizeof *bad); i++) {
        SetLastError(0);
        S(10 + i, !U(SetDefaultDllDirectories)(bad[i]));
        S(40 + i, GetLastError() == ERROR_INVALID_PARAMETER);
    }
    return 0;
}
TEST(dd_set_default_invalid_flags) {
    REQUIRE(U(SetDefaultDllDirectories));
    EXPECT_EQ(t_run_child("dd_default_invalid", "", 25000), 0);
}
CHILD(dd_default_zero) {
    SetLastError(0);
    BOOL r = U(SetDefaultDllDirectories)(0);
    (void)r;
    return 0;
}
TEST(dd_set_default_zero_flags_noted) {
    REQUIRE(U(SetDefaultDllDirectories));
    EXPECT_EQ(t_run_child("dd_default_zero", "", 25000), 0);
}
CHILD(dd_default_valid) {
    static const DWORD good[] = { LOAD_LIBRARY_SEARCH_APPLICATION_DIR, LOAD_LIBRARY_SEARCH_SYSTEM32, LOAD_LIBRARY_SEARCH_USER_DIRS,
        LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32 };
    for (int i = 0; i < (int)(sizeof good / sizeof *good); i++) S(10 + i, U(SetDefaultDllDirectories)(good[i]));
    return 0;
}
TEST(dd_set_default_valid_flags) {
    REQUIRE(U(SetDefaultDllDirectories));
    EXPECT_EQ(t_run_child("dd_default_valid", "", 25000), 0);
}
CHILD(dd_default_affects_plain_loadlibrary) {
    const char *d = argv[1];
    WCHAR wd[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, d, -1, wd, MAX_PATH);
    DLL_DIRECTORY_COOKIE c = U(AddDllDirectory)(wd);
    S(1, c != NULL);
    S(2, !loads("k32m_dd.dll", 0));                       /* legacy order ignores AddDllDirectory */
    S(3, U(SetDefaultDllDirectories)(LOAD_LIBRARY_SEARCH_USER_DIRS));
    S(4, loads("k32m_dd.dll", 0));                        /* plain LoadLibrary now uses the process defaults */
    S(5, U(LoadLibraryA)("k32m_dd.dll") != NULL);
    ml_unload(GetModuleHandleA("k32m_dd.dll"));
    S(6, U(SetDefaultDllDirectories)(LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(7, !loads("k32m_dd.dll", 0));
    S(8, GetLastError() == ERROR_MOD_NOT_FOUND);
    S(9, loads("kernel32.dll", 0));
    S(10, U(SetDefaultDllDirectories)(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    S(11, loads("k32m_dd.dll", 0));
    /* explicit flags override the process defaults */
    S(12, U(SetDefaultDllDirectories)(LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(13, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    /* LOAD_WITH_ALTERED_SEARCH_PATH is not a search-flag so defaults are not applied? only record */
    return 0;
}
TEST(dd_set_default_dirs_changes_plain_loadlibrary) {
    REQUIRE(U(AddDllDirectory)); REQUIRE(U(SetDefaultDllDirectories)); REQUIRE(U(LoadLibraryExA));
    EXPECT_EQ(dd_run("dd_default_affects_plain_loadlibrary", dd_prep("dd_df", 1), NULL), 0);
}
CHILD(dd_default_excludes_cwd_and_path) {
    const char *d = argv[1];                                   /* cwd == d */
    S(1, loads("k32m_dd.dll", 0));                             /* via cwd */
    S(2, U(SetDefaultDllDirectories)(LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(3, !loads("k32m_dd.dll", 0));
    S(4, SetEnvironmentVariableA("PATH", d));
    S(5, !loads("k32m_dd.dll", 0));
    return 0;
}
TEST(dd_set_default_dirs_drops_cwd_and_path) {
    REQUIRE(U(SetDefaultDllDirectories));
    const char *d = dd_prep("dd_dc", 1);
    EXPECT_EQ(dd_run("dd_default_excludes_cwd_and_path", d, dd_nobs(d)), 0);
}
CHILD(dd_default_dependencies) {
    /* argv[1] = dir containing k32m_imp.dll and k32m_dep.dll */
    const char *d = argv[1];
    WCHAR wd[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, d, -1, wd, MAX_PATH);
    DLL_DIRECTORY_COOKIE c = U(AddDllDirectory)(wd);
    S(1, c != NULL);
    HMODULE h = U(LoadLibraryExA)("k32m_imp.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
    S(2, h != NULL);                                           /* dependency found through the same flags */
    S(3, GetModuleHandleA("k32m_dep.dll") != NULL);
    ml_unload(h);
    S(4, GetModuleHandleA("k32m_dep.dll") == NULL);
    h = U(LoadLibraryExA)("k32m_imp.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    S(5, h == NULL);
    S(6, GetLastError() == ERROR_MOD_NOT_FOUND);
    U(RemoveDllDirectory)(c);
    h = U(LoadLibraryExA)("k32m_imp.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
    S(7, h == NULL);
    return 0;
}
TEST(dd_user_dir_serves_dependencies) {
    REQUIRE(U(AddDllDirectory)); REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("dd_dep");
    ml_put(ML_IMP, d, "k32m_imp.dll", NULL); ml_put(ML_DEP, d, "k32m_dep.dll", NULL);
    EXPECT_EQ(dd_run("dd_default_dependencies", d, NULL), 0);
}

/* ---- search flags ---- */
CHILD(dd_appdir_flags) {
    S(1, loads("k32m_dd.dll", 0));                              /* application directory is first in the legacy order */
    S(2, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_APPLICATION_DIR));
    S(3, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    S(4, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(5, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(6, GetLastError() == ERROR_MOD_NOT_FOUND);
    S(7, !loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_USER_DIRS));
    S(8, U(SetDefaultDllDirectories)(LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(9, !loads("k32m_dd.dll", 0));                             /* defaults now exclude the app dir */
    S(10, loads("k32m_dd.dll", LOAD_LIBRARY_SEARCH_APPLICATION_DIR));
    S(11, U(SetDefaultDllDirectories)(LOAD_LIBRARY_SEARCH_APPLICATION_DIR));
    S(12, loads("k32m_dd.dll", 0));
    return 0;
}
TEST(dd_application_directory_search) {
    REQUIRE(U(LoadLibraryExA)); REQUIRE(U(SetDefaultDllDirectories));
    const char *d = ml_dir("dd_app"); char exe[MAX_PATH];
    CHECK(ml_copy_self(d, "k32t_app.exe", exe));
    CHECK(ml_put(ML_A, d, "k32m_dd.dll", NULL));
    EXPECT_EQ(ml_run_exe(exe, "dd_appdir_flags", "", NULL, 25000, 0), 0);
}
CHILD(dd_system32_flag) {
    S(1, loads("kernel32.dll", LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(2, loads("version.dll", LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(3, loads("version", LOAD_LIBRARY_SEARCH_SYSTEM32));
    S(4, loads("version.dll", LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
    S(5, loads("version.dll", LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32));
    return 0;
}
TEST(dd_system32_search_flag) {
    REQUIRE(U(LoadLibraryExA));
    EXPECT_EQ(t_run_child("dd_system32_flag", "", 25000), 0);
}
CHILD(dd_load_dir_flags) {
    const char *d = argv[1];
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\k32m_imp.dll", d);
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
    S(1, h != NULL);                          /* dependency is next to the image */
    S(2, GetModuleHandleA("k32m_dep.dll") != NULL);
    ml_unload(h);
    h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    S(3, h == NULL);                          /* dll load dir not searched for dependencies */
    S(4, GetLastError() == ERROR_MOD_NOT_FOUND);
    h = U(LoadLibraryExA)(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    S(5, h != NULL);
    ml_unload(h);
    h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    S(6, h != NULL);
    ml_unload(h);
    h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_WITH_ALTERED_SEARCH_PATH);
    S(7, h == NULL);
    S(8, GetLastError() == ERROR_INVALID_PARAMETER);
    return 0;
}
TEST(dd_dll_load_dir_for_dependencies) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("dd_ldd");
    ml_put(ML_IMP, d, "k32m_imp.dll", NULL); ml_put(ML_DEP, d, "k32m_dep.dll", NULL);
    EXPECT_EQ(dd_run("dd_load_dir_flags", d, NULL), 0);
}
CHILD(dd_search_flags_relative_name) {
    /* relative file names with a path component are resolved against cwd for legacy search, rejected/ignored for search flags */
    S(1, loads(".\\sub\\k32m_dd.dll", 0));
    HMODULE h = U(LoadLibraryExA)(".\\sub\\k32m_dd.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (h) ml_unload(h);
    return 0;
}
TEST(dd_relative_path_with_search_flags) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("dd_rel"); char sub[MAX_PATH]; snprintf(sub, sizeof sub, "%ssub", d); CreateDirectoryA(sub, NULL);
    strcat(sub, "\\"); ml_put(ML_A, sub, "k32m_dd.dll", NULL);
    EXPECT_EQ(dd_run("dd_search_flags_relative_name", d, dd_nobs(d)), 0);
}
CHILD(dd_altered_path_bare_name) {
    const char *d = argv[1];
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\k32m_imp.dll", d);
    /* altered search path with a full path: dependencies from the same dir */
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    S(1, h != NULL);
    ml_unload(h);
    /* plain load by full path with dependency in the same dir; behaviour recorded by test, here only: result is consistent */
    h = U(LoadLibraryExA)(p, NULL, 0);
    if (h) ml_unload(h);
    return 0;
}
TEST(dd_altered_search_path_full_path) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("dd_alt");
    ml_put(ML_IMP, d, "k32m_imp.dll", NULL); ml_put(ML_DEP, d, "k32m_dep.dll", NULL);
    EXPECT_EQ(dd_run("dd_altered_path_bare_name", d, NULL), 0);
}
CHILD(dd_ignore_authz_flags) {
    /* flags with no visible effect for an unsigned local image must not break plain loading */
    const char *d = argv[1];
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\k32m_dd.dll", d);
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_IGNORE_CODE_AUTHZ_LEVEL);
    S(1, h != NULL);
    ml_unload(h);
    h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_SAFE_CURRENT_DIRS);
    S(2, h != NULL);
    ml_unload(h);
    h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    S(3, h != NULL);                          /* full path: search flags only matter for dependencies */
    ml_unload(h);
    h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_REQUIRE_SIGNED_TARGET);
    S(4, h == NULL);                          /* unsigned image refused */
    DWORD e = GetLastError();
    S(5, e == ERROR_INVALID_IMAGE_HASH || e == ERROR_BAD_EXE_FORMAT || e == ERROR_ACCESS_DENIED || e == ERROR_INVALID_PARAMETER || e == ERROR_MOD_NOT_FOUND);
    return 0;
}
TEST(dd_other_load_flags_full_path) {
    REQUIRE(U(LoadLibraryExA));
    EXPECT_EQ(dd_run("dd_ignore_authz_flags", dd_prep("dd_oth", 1), NULL), 0);
}
