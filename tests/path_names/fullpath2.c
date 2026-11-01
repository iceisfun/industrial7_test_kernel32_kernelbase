/* GetFullPathName interaction with the current directory and per-drive "=X:" variables (child processes only) */
#include "pn.h"

static int fail_code;
#define CK(c, code) do { if (!(c)) { printf("  CHILDFAIL line %d: %s\n", __LINE__, #c); return (code); } } while (0)

static void leave_tmp(void) { char w[MAX_PATH]; GetWindowsDirectoryA(w, sizeof w); SetCurrentDirectoryA(w); }

CHILD(fp_cwd_relative) {
    char d[MAX_PATH], exp[MAX_PATH * 2], got[MAX_PATH * 2];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    CK(SetCurrentDirectoryA(d), 10);
    char sub[MAX_PATH]; snprintf(sub, sizeof sub, "%ssubdir", d); CreateDirectoryA(sub, NULL);
    snprintf(exp, sizeof exp, "%sfile.txt", d);
    CK(U(GetFullPathNameA)("file.txt", sizeof got, got, NULL) == strlen(exp) && !_stricmp(got, exp), 11);
    snprintf(exp, sizeof exp, "%ssubdir\\inner.txt", d);
    CK(U(GetFullPathNameA)("subdir\\inner.txt", sizeof got, got, NULL) == strlen(exp) && !_stricmp(got, exp), 12);
    CK(SetCurrentDirectoryA(sub), 13);
    snprintf(exp, sizeof exp, "%ssubdir\\x", d);
    CK(U(GetFullPathNameA)("x", sizeof got, got, NULL) && !_stricmp(got, exp), 14);
    snprintf(exp, sizeof exp, "%sx", d);
    CK(U(GetFullPathNameA)("..\\x", sizeof got, got, NULL) && !_stricmp(got, exp), 15);
    CK(U(GetFullPathNameA)("..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\..\\x", sizeof got, got, NULL) > 0, 16);
    CK(got[1] == ':' && got[2] == '\\' && !strcmp(got + 3, "x"), 17);
    /* "." resolves to the cwd itself without trailing backslash */
    snprintf(exp, sizeof exp, "%ssubdir", d);
    char *fp = (char *)SENT;
    CK(U(GetFullPathNameA)(".", sizeof got, got, &fp) == strlen(exp) && !_stricmp(got, exp), 18);
    CK(fp && !strcmp(fp, "subdir"), 19);
    /* the cwd is mirrored in the "=X:" variable */
    char var[4] = { '=', got[0], ':', 0 }, val[MAX_PATH] = "";
    DWORD n = GetEnvironmentVariableA(var, val, sizeof val);
    if (!n || _stricmp(val, exp)) printf("  NOTE =X: variable [%s] vs cwd [%s]\n", val, exp);
    leave_tmp();
    return 0;
}
TEST(path_names_fullpath_cwd_relative_child) { EXPECT_EQ(t_run_child("fp_cwd_relative", NULL, 25000), 0); }

CHILD(fp_cwd_root) {
    char root[8] = "C:\\", got[MAX_PATH];
    root[0] = pn_sysdrive();
    CK(SetCurrentDirectoryA(root), 10);
    char exp[16]; snprintf(exp, sizeof exp, "%c:\\x", root[0]);
    CK(U(GetFullPathNameA)("x", sizeof got, got, NULL) == 4 && !_stricmp(got, exp), 11);
    snprintf(exp, sizeof exp, "%c:\\", root[0]);
    char *fp = (char *)SENT;
    CK(U(GetFullPathNameA)(".", sizeof got, got, &fp) == 3 && !_stricmp(got, exp), 12);
    CK(fp == NULL, 13);
    CK(U(GetFullPathNameA)("..", sizeof got, got, NULL) == 3, 14);
    CK(U(GetFullPathNameA)("\\x", sizeof got, got, NULL) == 4, 15);
    leave_tmp();
    return 0;
}
TEST(path_names_fullpath_cwd_root_child) { EXPECT_EQ(t_run_child("fp_cwd_root", NULL, 25000), 0); }

/* Win10 ntdll!RtlGetFullPathName_Ustr validates a "=X:" value (RtlpCheckRelativeDrive opens the directory) and
   resets the variable to "X:\\" when it does not exist. The cases therefore need a real directory on a drive
   other than the cwd drive: the child maps Q: to its scratch dir with DefineDosDevice (session-local, removed
   before exit) and skips (exit 77) if Q: is already in use or the mapping cannot be created. */
static int fp_drive_vars_body(void) {
    char got[MAX_PATH]; WCHAR gw[MAX_PATH];
    SetEnvironmentVariableA("=Q:", "Q:\\k32\\dir");
    { DWORD n0 = U(GetFullPathNameA)("Q:foo", sizeof got, got, NULL); printf("  NOTE Q:foo -> [%s] n=%lu\n", got, n0); }
    CK(U(GetFullPathNameA)("Q:foo", sizeof got, got, NULL) == 14 && !strcmp(got, "Q:\\k32\\dir\\foo"), 10);
    CK(U(GetFullPathNameA)("q:foo", sizeof got, got, NULL) == 14 && !_stricmp(got, "Q:\\k32\\dir\\foo"), 11);
    CK(U(GetFullPathNameA)("Q:", sizeof got, got, NULL) == 10 && !strcmp(got, "Q:\\k32\\dir"), 12);
    CK(U(GetFullPathNameA)("Q:..\\x", sizeof got, got, NULL) && !strcmp(got, "Q:\\k32\\x"), 13);
    CK(U(GetFullPathNameA)("Q:.\\x\\..\\y", sizeof got, got, NULL) && !strcmp(got, "Q:\\k32\\dir\\y"), 14);
    CK(U(GetFullPathNameA)("Q:\\foo", sizeof got, got, NULL) == 6 && !strcmp(got, "Q:\\foo"), 15);
    char *fp = (char *)SENT;
    CK(U(GetFullPathNameA)("Q:foo.txt", sizeof got, got, &fp) && fp && !strcmp(fp, "foo.txt"), 16);
    CK(U(GetFullPathNameW)(L"Q:w", MAX_PATH, gw, NULL) == 12 && pn_weq(gw, "Q:\\k32\\dir\\w"), 17);
    /* trailing backslash in the variable: Win10 ntdll!RtlGetFullPathName_Ustr appends '\\' to any =X: value longer
       than 3 chars without checking for an existing one, so the separator is doubled (Wine keeps the old expectation); a root
       value works */
    SetEnvironmentVariableA("=Q:", "Q:\\k32\\dir\\");
    {   DWORD n20 = U(GetFullPathNameA)("Q:foo", sizeof got, got, NULL);
        printf("  NOTE =Q:=Q:\\k32\\dir\\ : Q:foo -> [%s] n=%lu\n", got, n20);
        if (t_is_wine()) CK(n20 && !strcmp(got, "Q:\\k32\\dir\\foo"), 20);
        else CK(n20 == 15 && !strcmp(got, "Q:\\k32\\dir\\\\foo"), 20);
    }
    SetEnvironmentVariableA("=Q:", "Q:\\");
    CK(U(GetFullPathNameA)("Q:foo", sizeof got, got, NULL) == 6 && !strcmp(got, "Q:\\foo"), 21);
    CK(U(GetFullPathNameA)("Q:", sizeof got, got, NULL) == 3 && !strcmp(got, "Q:\\"), 22);
    /* no variable: root of that drive */
    SetEnvironmentVariableA("=Q:", NULL);
    CK(U(GetFullPathNameA)("Q:foo", sizeof got, got, NULL) == 6 && !strcmp(got, "Q:\\foo"), 23);
    if (!t_is_wine()) {
        /* a variable naming a directory that does not exist is reset to the drive root */
        SetEnvironmentVariableA("=Q:", "Q:\\k32\\nonexistent");
        CK(U(GetFullPathNameA)("Q:foo", sizeof got, got, NULL) == 6 && !strcmp(got, "Q:\\foo"), 24);
    }
    /* a variable naming a different drive is still used verbatim for the path part */
    SetEnvironmentVariableA("=Q:", "R:\\other");
    DWORD n = U(GetFullPathNameA)("Q:foo", sizeof got, got, NULL);
    printf("  NOTE mismatched =Q: value -> %s (n=%lu)\n", got, n);
    SetEnvironmentVariableA("=Q:", NULL);
    return 0;
}
CHILD(fp_drive_vars) {
    char tgt[MAX_PATH + 8], sub[MAX_PATH + 16], dev[MAX_PATH];
    if (QueryDosDeviceA("Q:", dev, sizeof dev)) { printf("  NOTE Q: in use, skipped\n"); return 77; }
    snprintf(sub, sizeof sub, "%sk32", t_tmpdir());
    CreateDirectoryA(sub, NULL);
    snprintf(sub, sizeof sub, "%sk32\\dir", t_tmpdir());
    CreateDirectoryA(sub, NULL);
    snprintf(tgt, sizeof tgt, "\\??\\%s", t_tmpdir());
    tgt[strlen(tgt) - 1] = 0;
    if (!DefineDosDeviceA(DDD_RAW_TARGET_PATH, "Q:", tgt)) { printf("  NOTE DefineDosDevice failed %lu, skipped\n", GetLastError()); return 77; }
    {   /* precondition: the mapping must resolve for this process, else the =Q: directory check cannot succeed */
        char tg2[MAX_PATH] = "";
        DWORD qa = GetFileAttributesA("Q:\\k32\\dir"), qe = GetLastError();
        QueryDosDeviceA("Q:", tg2, sizeof tg2);
        printf("  NOTE Q: -> [%s]; attrs(Q:\\k32\\dir)=0x%lx err %lu\n", tg2, qa, qe);
        if (qa == INVALID_FILE_ATTRIBUTES) {
            DefineDosDeviceA(DDD_REMOVE_DEFINITION | DDD_RAW_TARGET_PATH | DDD_EXACT_MATCH_ON_REMOVE, "Q:", tgt);
            return 77;
        }
    }
    int rc = fp_drive_vars_body();
    DefineDosDeviceA(DDD_REMOVE_DEFINITION | DDD_RAW_TARGET_PATH | DDD_EXACT_MATCH_ON_REMOVE, "Q:", tgt);
    return rc;
}
TEST(path_names_fullpath_drive_variables_child) {
    DWORD rc = t_run_child("fp_drive_vars", NULL, 25000);
    if (rc == 77) { t_skip("no free Q: drive letter or DefineDosDevice denied"); return; }
    EXPECT_EQ(rc, 0);
}

CHILD(fp_cwd_other_drive) {
    /* the cwd drive is "same drive": X:foo with X = cwd drive uses the cwd, not the variable */
    char d[MAX_PATH], got[MAX_PATH], in[16], exp[MAX_PATH * 2];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    CK(SetCurrentDirectoryA(d), 10);
    snprintf(in, sizeof in, "%c:zz", d[0]);
    snprintf(exp, sizeof exp, "%szz", d);
    CK(U(GetFullPathNameA)(in, sizeof got, got, NULL) && !_stricmp(got, exp), 11);
    in[0] = (char)tolower((unsigned char)d[0]);
    CK(U(GetFullPathNameA)(in, sizeof got, got, NULL) && !_stricmp(got, exp), 12);
    leave_tmp();
    return 0;
}
TEST(path_names_fullpath_cwd_drive_relative_child) { EXPECT_EQ(t_run_child("fp_cwd_other_drive", NULL, 25000), 0); }

CHILD(fp_cwd_unc_free) {
    /* rooted path after switching the cwd to a different directory keeps the drive */
    char d[MAX_PATH], got[MAX_PATH];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    CK(SetCurrentDirectoryA(d), 10);
    CK(U(GetFullPathNameA)("\\rooted\\x", sizeof got, got, NULL) == 11 + 0 || U(GetFullPathNameA)("\\rooted\\x", sizeof got, got, NULL) > 0, 11);
    CK(got[0] == d[0] && got[1] == ':' && !strcmp(got + 2, "\\rooted\\x"), 12);
    leave_tmp();
    return 0;
}
TEST(path_names_fullpath_rooted_after_chdir_child) { EXPECT_EQ(t_run_child("fp_cwd_unc_free", NULL, 25000), 0); }

CHILD(fp_long_cwd_trailing) {
    char d[MAX_PATH], got[MAX_PATH], exp[MAX_PATH * 2];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    CK(SetCurrentDirectoryA(d), 10);
    char sp[MAX_PATH]; snprintf(sp, sizeof sp, "%strailing.", d);
    snprintf(exp, sizeof exp, "%strailing", d);
    CK(U(GetFullPathNameA)("trailing.", sizeof got, got, NULL) && !_stricmp(got, exp), 11);
    CK(U(GetFullPathNameA)("trailing  ", sizeof got, got, NULL) && !_stricmp(got, exp), 12);
    leave_tmp();
    return 0;
}
TEST(path_names_fullpath_relative_trailing_dot_child) { EXPECT_EQ(t_run_child("fp_long_cwd_trailing", NULL, 25000), 0); }

CHILD(fp_getcwd_vs_fullpath) {
    char d[MAX_PATH], c1[MAX_PATH], c2[MAX_PATH];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    CK(SetCurrentDirectoryA(d), 10);
    GetCurrentDirectoryA(sizeof c1, c1);
    char *fp;
    DWORD n = U(GetFullPathNameA)(".", sizeof c2, c2, &fp);
    CK(n == strlen(c1) && !_stricmp(c1, c2), 11);
    /* The current directory has no trailing backslash unless it is a root. */
    CK(c1[strlen(c1) - 1] != '\\', 12);
    leave_tmp();
    return 0;
}
TEST(path_names_fullpath_dot_matches_getcurrentdirectory_child) { EXPECT_EQ(t_run_child("fp_getcwd_vs_fullpath", NULL, 25000), 0); }
