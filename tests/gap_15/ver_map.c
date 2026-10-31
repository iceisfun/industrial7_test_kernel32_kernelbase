#include "gap15.h"

/* ------------------------------------------------------------- VerLanguageName */
TEST(gap15_verlang_args) {
    REQUIRE(U(VerLanguageNameA)); REQUIRE(U(VerLanguageNameW));
    char a[64]; WCHAR w[64];
    CHECK(U(VerLanguageNameA)(0x409, NULL, 64) == 0);
    CHECK(U(VerLanguageNameW)(0x409, NULL, 64) == 0);
    CHECK(U(VerLanguageNameA)(0x409, a, 0) == 0);
    CHECK(U(VerLanguageNameW)(0x409, w, 0) == 0);
    CHECK(U(VerLanguageNameA)(0x409, NULL, 0) == 0);
    CHECK(U(VerLanguageNameW)(0x409, NULL, 0) == 0);
    /* zero-size and NULL calls do not touch the buffer */
    memset(a, 0x55, sizeof a);
    U(VerLanguageNameA)(0x409, a, 0);
    CHECK((unsigned char)a[0] == 0x55);
}
TEST(gap15_verlang_english_us) {
    REQUIRE(U(VerLanguageNameA)); REQUIRE(U(VerLanguageNameW));
    char a[128]; WCHAR w[128];
    DWORD ra = U(VerLanguageNameA)(0x409, a, 128);
    DWORD rw = U(VerLanguageNameW)(0x409, w, 128);
    CHECKF(rw == 23, "W len %lu", (unsigned long)rw);
    CHECKF(ra == 23, "A len %lu", (unsigned long)ra);
    CHECKF(!strcmp(a, "English (United States)"), "A \"%s\"", a);
    CHECK(g_waeq(w, "English (United States)"));
}
TEST(gap15_verlang_truncation_w) {
    REQUIRE(U(VerLanguageNameW));
    const char *full = "English (United States)";
    size_t len = strlen(full);
    for (DWORD cch = 1; cch <= len + 3; cch++) {
        WCHAR w[64];
        for (int i = 0; i < 64; i++) w[i] = 0x7e7e;
        DWORD r = U(VerLanguageNameW)(0x409, w, cch);
        DWORD want = cch - 1 < len ? cch - 1 : (DWORD)len;
        CHECKF(r == want, "W cch=%lu returned %lu want %lu", (unsigned long)cch, (unsigned long)r, (unsigned long)want);
        if (want > 0) {
            CHECKF(w[want] == 0, "W cch=%lu not terminated", (unsigned long)cch);
            int ok = 1; for (DWORD i = 0; i < want; i++) if (w[i] != (WCHAR)full[i]) ok = 0;
            CHECKF(ok, "W cch=%lu content", (unsigned long)cch);
        }
        CHECKF(w[cch < 64 ? cch : 63] == 0x7e7e || cch >= 64, "W cch=%lu wrote past buffer", (unsigned long)cch);
    }
}
TEST(gap15_verlang_truncation_a) {
    REQUIRE(U(VerLanguageNameA));
    const char *full = "English (United States)";
    size_t len = strlen(full);
    for (DWORD cch = 2; cch <= len + 3; cch++) {
        char a[64]; memset(a, 0x7e, sizeof a);
        DWORD r = U(VerLanguageNameA)(0x409, a, cch);
        DWORD want = cch - 1 < len ? cch - 1 : (DWORD)len;
        CHECKF(r == want, "A cch=%lu returned %lu want %lu", (unsigned long)cch, (unsigned long)r, (unsigned long)want);
        CHECKF(!strncmp(a, full, want), "A cch=%lu content \"%s\"", (unsigned long)cch, a);
        CHECKF(a[want] == 0, "A cch=%lu terminator", (unsigned long)cch);
    }
    char a[8]; memset(a, 0x7e, sizeof a);
    DWORD r = U(VerLanguageNameA)(0x409, a, 1);
    t_note("VerLanguageNameA cch=1 -> %lu, buf[0]=0x%02x", (unsigned long)r, (unsigned char)a[0]);
    CHECKF(r == 0, "cch=1 returned %lu", (unsigned long)r);
}
TEST(gap15_verlang_many_ids) {
    REQUIRE(U(VerLanguageNameA)); REQUIRE(U(VerLanguageNameW));
    static const WORD ids[] = { 0x0000, 0x0007, 0x0009, 0x0407, 0x0409, 0x0809, 0x0c09, 0x040c, 0x0410, 0x0411, 0x0412, 0x0416, 0x0419, 0x0804, 0x0404, 0x0c0a, 0x0813, 0x1000, 0x0436, 0x048e, 0x7f, 0xffff, 0x1234, 0x0001, 0x0400, 0x0800 };
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        char a[128]; WCHAR w[128];
        DWORD ra = U(VerLanguageNameA)(ids[i], a, 128);
        DWORD rw = U(VerLanguageNameW)(ids[i], w, 128);
        char wn[128]; g_narrow(w, wn, sizeof wn);
        t_note("VerLanguageName(0x%04x): A(%lu)=\"%s\" W(%lu)=\"%s\"", ids[i], (unsigned long)ra, ra ? a : "", (unsigned long)rw, rw ? wn : "");
        CHECKF(ra == rw, "id 0x%04x: A len %lu W len %lu", ids[i], (unsigned long)ra, (unsigned long)rw);
        if (ids[i] != 0x048e && ids[i] != 0xffff && ids[i] != 0x1234) CHECKF(rw > 0, "id 0x%04x returned empty", ids[i]);
        if (rw && ra == rw) CHECKF(!strcmp(a, wn), "id 0x%04x: A/W differ", ids[i]);
        if (rw) CHECKF(strlen(wn) == rw, "id 0x%04x: length vs strlen", ids[i]);
    }
}
TEST(gap15_verlang_repeat_and_lasterror) {
    REQUIRE(U(VerLanguageNameW));
    WCHAR w[64];
    for (int i = 0; i < 100; i++) {
        CHECK(U(VerLanguageNameW)(0x409, w, 64) == 23);
    }
    /* unknown ids still produce a name */
    SetLastError(0xdeadbeef);
    DWORD r = U(VerLanguageNameW)(0xffff, w, 64);
    t_note("VerLanguageNameW(0xffff)=%lu lasterror=%lu", (unsigned long)r, GetLastError());
    /* the high word of a 32-bit LCID is ignored by the table lookup? observe */
    r = U(VerLanguageNameW)(0x10409, w, 64);
    t_note("VerLanguageNameW(0x10409)=%lu", (unsigned long)r);
}
TEST(gap15_verlang_a_large_cch) {
    REQUIRE(U(VerLanguageNameA));
    char *buf = (char *)HeapAlloc(GetProcessHeap(), 0, 70000);
    if (!buf) return;
    DWORD r = U(VerLanguageNameA)(0x409, buf, 70000);
    t_note("VerLanguageNameA cch=70000 -> %lu", (unsigned long)r);
    HeapFree(GetProcessHeap(), 0, buf);
}

/* ---------------------------------------------------------------- VerFindFile */
typedef DWORD (WINAPI *vff_a)(DWORD, LPSTR, LPSTR, LPSTR, LPSTR, PUINT, LPSTR, PUINT);
typedef DWORD (WINAPI *vff_w)(DWORD, LPWSTR, LPWSTR, LPWSTR, LPWSTR, PUINT, LPWSTR, PUINT);

static void trimslash(char *s) { size_t l = strlen(s); if (l && s[l - 1] == '\\') s[l - 1] = 0; }

TEST(gap15_verfind_bare_name_a) {
    REQUIRE(U(VerFindFileA));
    char app[MAX_PATH], win[MAX_PATH], file[] = "gap15_nofile.dll", cur[MAX_PATH], dst[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH);
    UINT cl = MAX_PATH, dl = MAX_PATH;
    memset(cur, 0x7e, sizeof cur); memset(dst, 0x7e, sizeof dst);
    DWORD r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    char expect[MAX_PATH]; strcpy(expect, app); trimslash(expect);
    CHECKF(!(r & VFF_BUFFTOOSMALL), "buffer flag set: 0x%lx", (unsigned long)r);
    CHECKF(!(r & VFF_FILEINUSE), "file in use flag: 0x%lx", (unsigned long)r);
    CHECKF(cur[0] == 0 && cl == 1, "cur dir \"%s\" len %u", cur, cl);
    CHECKF(!strcmp(dst, expect), "dest \"%s\" want \"%s\"", dst, expect);
    /* VerFindFileW: required length is strlen(szAppDir)+1 taken before the trailing '\\' is stripped from the copy */
    CHECKF(dl == strlen(app) + 1, "dest len %u want %u", dl, (unsigned)strlen(app) + 1);
    CHECKF(r & VFF_CURNEDEST, "CURNEDEST expected, r=0x%lx", (unsigned long)r);
}
TEST(gap15_verfind_bare_name_w) {
    REQUIRE(U(VerFindFileW));
    WCHAR app[MAX_PATH], win[MAX_PATH], file[] = L"gap15_nofile.dll", cur[MAX_PATH], dst[MAX_PATH];
    g_widen(t_tmpdir(), app, MAX_PATH); GetWindowsDirectoryW(win, MAX_PATH);
    UINT cl = MAX_PATH, dl = MAX_PATH;
    for (int i = 0; i < MAX_PATH; i++) cur[i] = dst[i] = 0x7e7e;
    DWORD r = U(VerFindFileW)(0, file, win, app, cur, &cl, dst, &dl);
    char expect[MAX_PATH]; strcpy(expect, t_tmpdir()); trimslash(expect);
    CHECKF(!(r & VFF_BUFFTOOSMALL), "buffer flag set: 0x%lx", (unsigned long)r);
    CHECKF(!(r & VFF_FILEINUSE), "file in use flag: 0x%lx", (unsigned long)r);
    CHECKF(cur[0] == 0 && cl == 1, "cur len %u", cl);
    CHECKF(g_waeq(dst, expect), "dest differs");
    CHECKF(dl == strlen(t_tmpdir()) + 1, "dest len %u", dl);
    CHECK(r & VFF_CURNEDEST);
}
TEST(gap15_verfind_same_dir) {
    REQUIRE(U(VerFindFileA));
    char app[MAX_PATH], win[MAX_PATH], file[MAX_PATH], cur[MAX_PATH], dst[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH);
    snprintf(file, sizeof file, "%sgap15_nofile.dll", t_tmpdir());
    UINT cl = MAX_PATH, dl = MAX_PATH;
    DWORD r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    char expect[MAX_PATH]; strcpy(expect, app); trimslash(expect);
    t_note("VerFindFileA same dir: r=0x%lx cur=\"%s\" (%u) dst=\"%s\" (%u)", (unsigned long)r, cur, cl, dst, dl);
    CHECKF(!(r & VFF_BUFFTOOSMALL), "buffer flag, r=0x%lx", (unsigned long)r);
    CHECKF(strlen(cur) + 1 == cl, "cur len %u vs strlen %u", cl, (unsigned)strlen(cur));
    if (_stricmp(cur, expect)) t_note("cur dir \"%s\" differs from \"%s\"", cur, expect);
    else CHECKF(!(r & VFF_CURNEDEST), "CURNEDEST set when dirs equal: 0x%lx", (unsigned long)r);
}
TEST(gap15_verfind_other_dir_curnedest) {
    REQUIRE(U(VerFindFileA));
    char app[MAX_PATH], win[MAX_PATH], file[MAX_PATH], cur[MAX_PATH], dst[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH);
    snprintf(file, sizeof file, "%s\\gap15_nofile.dll", win);
    UINT cl = MAX_PATH, dl = MAX_PATH;
    DWORD r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    char expect[MAX_PATH]; strcpy(expect, win); trimslash(expect);
    if (_stricmp(cur, expect)) t_note("cur dir \"%s\" differs from \"%s\"", cur, expect);
    CHECKF(r & VFF_CURNEDEST, "CURNEDEST expected: 0x%lx", (unsigned long)r);
}
TEST(gap15_verfind_cur_buffer_small) {
    REQUIRE(U(VerFindFileA)); REQUIRE(U(VerFindFileW));
    char app[MAX_PATH], win[MAX_PATH], file[MAX_PATH], cur[MAX_PATH], dst[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH);
    snprintf(file, sizeof file, "%sgap15_nofile.dll", t_tmpdir());
    char expect[MAX_PATH]; strcpy(expect, app); trimslash(expect);
    size_t curlen = strlen(expect);
    /* GetDirOfFile only yields a directory when the file exists (CreateFileW OPEN_EXISTING), so create it */
    HANDLE hf = CreateFileA(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) { t_skip("cannot create file"); return; }
    CloseHandle(hf);
    UINT cl = (UINT)curlen, dl = MAX_PATH;   /* no room for NUL */
    memset(cur, 0x7e, sizeof cur);
    DWORD r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    t_note("A cur small: r=0x%lx cl=%u", (unsigned long)r, cl);
    CHECKF(r & VFF_BUFFTOOSMALL, "BUFFTOOSMALL expected r=0x%lx", (unsigned long)r);
    CHECKF(cl >= curlen + 1, "required cur len %u want >= %u", cl, (unsigned)curlen + 1);
    /* now with the required size */
    r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    CHECKF(!(r & VFF_BUFFTOOSMALL), "retry r=0x%lx", (unsigned long)r);
    if (_stricmp(cur, expect)) t_note("retry cur \"%s\"", cur);
    /* wide */
    WCHAR wapp[MAX_PATH], wwin[MAX_PATH], wfile[MAX_PATH], wcur[MAX_PATH], wdst[MAX_PATH];
    g_widen(app, wapp, MAX_PATH); g_widen(win, wwin, MAX_PATH); g_widen(file, wfile, MAX_PATH);
    cl = (UINT)curlen; dl = MAX_PATH;
    r = U(VerFindFileW)(0, wfile, wwin, wapp, wcur, &cl, wdst, &dl);
    CHECKF(r & VFF_BUFFTOOSMALL, "W BUFFTOOSMALL expected r=0x%lx", (unsigned long)r);
    CHECKF(cl >= curlen + 1, "W required cur len %u", cl);
    DeleteFileA(file);
    /* length 1 for bare name needs 1 -> fits when empty */
    char bare[] = "gap15_nofile.dll";
    cl = 1; dl = MAX_PATH;
    r = U(VerFindFileA)(0, bare, win, app, cur, &cl, dst, &dl);
    CHECKF(!(r & VFF_BUFFTOOSMALL), "empty cur dir with len 1: r=0x%lx", (unsigned long)r);
    CHECK(cl == 1);
    cl = 0; dl = MAX_PATH;
    r = U(VerFindFileA)(0, bare, win, app, cur, &cl, dst, &dl);
    t_note("A cur len 0: r=0x%lx cl=%u", (unsigned long)r, cl);
}
TEST(gap15_verfind_dest_buffer_small) {
    REQUIRE(U(VerFindFileA)); REQUIRE(U(VerFindFileW));
    char app[MAX_PATH], win[MAX_PATH], file[] = "gap15_nofile.dll", cur[MAX_PATH], dst[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH);
    size_t applen = strlen(app);   /* includes trailing backslash */
    UINT cl = MAX_PATH, dl = (UINT)applen;
    DWORD r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    t_note("A dest small: r=0x%lx dl=%u (applen %u)", (unsigned long)r, dl, (unsigned)applen);
    CHECKF(r & VFF_BUFFTOOSMALL, "BUFFTOOSMALL expected r=0x%lx", (unsigned long)r);
    CHECKF(dl == applen + 1, "required dest len %u want %u", dl, (unsigned)applen + 1);
    dl = (UINT)applen + 1;
    r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    CHECKF(!(r & VFF_BUFFTOOSMALL), "retry r=0x%lx", (unsigned long)r);
    char expect[MAX_PATH]; strcpy(expect, app); trimslash(expect);
    CHECKF(!strcmp(dst, expect), "dest \"%s\"", dst);
    WCHAR wapp[MAX_PATH], wwin[MAX_PATH], wfile[] = L"gap15_nofile.dll", wcur[MAX_PATH], wdst[MAX_PATH];
    g_widen(app, wapp, MAX_PATH); g_widen(win, wwin, MAX_PATH);
    cl = MAX_PATH; dl = (UINT)applen;
    r = U(VerFindFileW)(0, wfile, wwin, wapp, wcur, &cl, wdst, &dl);
    CHECKF(r & VFF_BUFFTOOSMALL, "W BUFFTOOSMALL r=0x%lx", (unsigned long)r);
    CHECKF(dl == applen + 1, "W required dest len %u", dl);
}
TEST(gap15_verfind_null_dest) {
    REQUIRE(U(VerFindFileA));
    char app[MAX_PATH], win[MAX_PATH], file[] = "gap15_nofile.dll", cur[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH);
    UINT cl = MAX_PATH, dl = MAX_PATH;
    DWORD r = U(VerFindFileA)(0, file, win, app, cur, &cl, NULL, &dl);
    t_note("VerFindFileA NULL dest: r=0x%lx cl=%u dl=%u", (unsigned long)r, cl, dl);
    CHECK(!(r & VFF_BUFFTOOSMALL));
}
TEST(gap15_verfind_shared) {
    REQUIRE(U(VerFindFileA)); REQUIRE(U(VerFindFileW));
    char app[MAX_PATH], win[MAX_PATH], file[] = "gap15_nofile.dll", cur[MAX_PATH], dst[MAX_PATH], sys[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH); GetSystemDirectoryA(sys, MAX_PATH);
    UINT cl = MAX_PATH, dl = MAX_PATH;
    DWORD r = U(VerFindFileA)(VFFF_ISSHAREDFILE, file, win, app, cur, &cl, dst, &dl);
    t_note("shared: r=0x%lx dst=\"%s\" win=\"%s\" sys=\"%s\"", (unsigned long)r, dst, win, sys);
    CHECKF(!(r & VFF_BUFFTOOSMALL), "buffer flag r=0x%lx", (unsigned long)r);
    CHECKF(dl == strlen(dst) + 1, "len %u vs \"%s\"", dl, dst);
    CHECKF(_stricmp(dst, app) != 0, "shared file must not go to the app dir");
    CHECKF(!_strnicmp(dst, win, strlen(win)), "shared dest \"%s\" not under windows dir", dst);
    CHECK(r & VFF_CURNEDEST);
    WCHAR wapp[MAX_PATH], wwin[MAX_PATH], wfile[] = L"gap15_nofile.dll", wcur[MAX_PATH], wdst[MAX_PATH];
    g_widen(app, wapp, MAX_PATH); g_widen(win, wwin, MAX_PATH);
    cl = MAX_PATH; dl = MAX_PATH;
    DWORD rw = U(VerFindFileW)(VFFF_ISSHAREDFILE, wfile, wwin, wapp, wcur, &cl, wdst, &dl);
    char dn[MAX_PATH]; g_narrow(wdst, dn, sizeof dn);
    CHECKF(rw == r, "A/W result differ 0x%lx/0x%lx", (unsigned long)r, (unsigned long)rw);
    CHECKF(!_stricmp(dn, dst), "A/W dest differ \"%s\" \"%s\"", dst, dn);
    /* repeated call uses cached directory */
    cl = MAX_PATH; dl = MAX_PATH;
    char dst2[MAX_PATH];
    DWORD r2 = U(VerFindFileA)(VFFF_ISSHAREDFILE, file, win, app, cur, &cl, dst2, &dl);
    CHECK(r2 == r && !strcmp(dst, dst2));
}
TEST(gap15_verfind_file_in_use) {
    REQUIRE(U(VerFindFileA));
    char app[MAX_PATH], win[MAX_PATH], file[MAX_PATH], cur[MAX_PATH], dst[MAX_PATH];
    strcpy(app, t_tmpdir()); GetWindowsDirectoryA(win, MAX_PATH);
    snprintf(file, sizeof file, "%sgap15_inuse.dll", t_tmpdir());
    HANDLE h = CreateFileA(file, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create file"); return; }
    UINT cl = MAX_PATH, dl = MAX_PATH;
    DWORD r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    t_note("locked file: r=0x%lx", (unsigned long)r);
    
    CloseHandle(h);
    cl = MAX_PATH; dl = MAX_PATH;
    r = U(VerFindFileA)(0, file, win, app, cur, &cl, dst, &dl);
    t_note("unlocked file: r=0x%lx", (unsigned long)r);
    CHECKF(!(r & VFF_FILEINUSE), "FILEINUSE after close, r=0x%lx", (unsigned long)r);
    DeleteFileA(file);
}
TEST(gap15_verfind_repeat) {
    REQUIRE(U(VerFindFileW));
    WCHAR app[MAX_PATH], win[MAX_PATH], file[] = L"gap15_nofile.dll", cur[MAX_PATH], dst[MAX_PATH];
    g_widen(t_tmpdir(), app, MAX_PATH); GetWindowsDirectoryW(win, MAX_PATH);
    DWORD first = 0;
    for (int i = 0; i < 50; i++) {
        UINT cl = MAX_PATH, dl = MAX_PATH;
        DWORD r = U(VerFindFileW)(0, file, win, app, cur, &cl, dst, &dl);
        if (!i) first = r; else CHECK(r == first);
    }
}

/* -------------------------------------------------------------- VerQueryValueA */
typedef BOOL (WINAPI *vq_a)(LPCVOID, LPCSTR, LPVOID *, PUINT);
typedef DWORD (WINAPI *gfvsa)(LPCSTR, LPDWORD);
typedef BOOL (WINAPI *gfvia)(LPCSTR, DWORD, DWORD, LPVOID);

static void *load_k32_version(DWORD *size)
{
    char path[MAX_PATH]; GetSystemDirectoryA(path, MAX_PATH);
    strcat(path, "\\kernel32.dll");
    HMODULE v = LoadLibraryA("version.dll");
    if (!v) return NULL;
    gfvsa sz = (gfvsa)GetProcAddress(v, "GetFileVersionInfoSizeA");
    gfvia get = (gfvia)GetProcAddress(v, "GetFileVersionInfoA");
    if (!sz || !get) return NULL;
    DWORD h = 0, n = sz(path, &h);
    if (!n) return NULL;
    void *buf = HeapAlloc(GetProcessHeap(), 0, n + 16);
    if (!buf) return NULL;
    if (!get(path, 0, n, buf)) { HeapFree(GetProcessHeap(), 0, buf); return NULL; }
    *size = n;
    return buf;
}

TEST(gap15_verquery_synthetic_root) {
    REQUIRE(U(VerQueryValueA));
    /* hand-built VS_VERSIONINFO with only the fixed file info */
    struct { WORD wLength, wValueLength, wType; WCHAR key[16]; WORD pad; VS_FIXEDFILEINFO ffi; } __attribute__((aligned(4), packed)) blk;
    _Static_assert(sizeof(blk) == 6 + 32 + 2 + 52, "layout");
    static __attribute__((aligned(8))) BYTE raw[128];
    memset(raw, 0, sizeof raw);
    WORD *hdr = (WORD *)raw;
    hdr[0] = 92; hdr[1] = 52; hdr[2] = 0;
    const WCHAR *k = L"VS_VERSION_INFO";
    memcpy(raw + 6, k, 32);
    VS_FIXEDFILEINFO *f = (VS_FIXEDFILEINFO *)(raw + 40);
    f->dwSignature = 0xFEEF04BD; f->dwStrucVersion = 0x10000; f->dwFileVersionMS = 0x00010002; f->dwFileVersionLS = 0x00030004;
    (void)blk;
    LPVOID p = (LPVOID)0x1; UINT len = 0x55;
    BOOL ok = U(VerQueryValueA)(raw, "\\", &p, &len);
    CHECK(ok);
    CHECKF(p == (LPVOID)f, "root value pointer %p want %p", p, (void *)f);
    CHECKF(len == 52, "root value len %u", len);
    /* leading separators are skipped */
    p = NULL; len = 0;
    ok = U(VerQueryValueA)(raw, "\\\\", &p, &len);
    t_note("double backslash root: ok=%d len=%u", ok, len);
    /* NULL length is allowed */
    p = NULL;
    ok = U(VerQueryValueA)(raw, "\\", &p, NULL);
    CHECK(ok && p == (LPVOID)f);
    /* missing child fails and zeroes the length */
    p = (LPVOID)0x1; len = 0x55;
    ok = U(VerQueryValueA)(raw, "\\StringFileInfo", &p, &len);
    CHECK(!ok);
    CHECKF(len == 0, "len after failure %u", len);
    len = 0x55;
    ok = U(VerQueryValueA)(raw, "\\NoSuchThing\\X", &p, &len);
    CHECK(!ok && len == 0);
    /* empty subblock */
    p = NULL; len = 0x55;
    SetLastError(0xdeadbeef);
    ok = U(VerQueryValueA)(raw, "", &p, &len);
    t_note("empty subblock: ok=%d len=%u err=%lu", ok, len, GetLastError());
}
TEST(gap15_verquery_real_file) {
    REQUIRE(U(VerQueryValueA));
    DWORD sz = 0;
    void *buf = load_k32_version(&sz);
    if (!buf) { t_skip("no version info for kernel32.dll"); return; }
    LPVOID p = NULL; UINT len = 0;
    BOOL ok = U(VerQueryValueA)(buf, "\\", &p, &len);
    CHECK(ok);
    CHECKF(len == sizeof(VS_FIXEDFILEINFO), "fixed info len %u", len);
    if (ok && p) {
        VS_FIXEDFILEINFO *f = (VS_FIXEDFILEINFO *)p;
        CHECKF(f->dwSignature == 0xFEEF04BD, "signature 0x%lx", (unsigned long)f->dwSignature);
        CHECK((BYTE *)p > (BYTE *)buf && (BYTE *)p < (BYTE *)buf + sz);
        t_note("kernel32 version %lu.%lu.%lu.%lu", f->dwFileVersionMS >> 16, f->dwFileVersionMS & 0xffff, f->dwFileVersionLS >> 16, f->dwFileVersionLS & 0xffff);
    }
    /* translation table */
    p = NULL; len = 0;
    ok = U(VerQueryValueA)(buf, "\\VarFileInfo\\Translation", &p, &len);
    CHECK(ok);
    CHECKF(len >= 4 && len % 4 == 0, "translation len %u", len);
    if (ok && len >= 4) {
        WORD lang = ((WORD *)p)[0], cp = ((WORD *)p)[1];
        char key[128], key2[128];
        snprintf(key, sizeof key, "\\StringFileInfo\\%04X%04X\\FileDescription", lang, cp);
        p = NULL; len = 0;
        ok = U(VerQueryValueA)(buf, key, &p, &len);
        CHECKF(ok, "query %s", key);
        if (ok) {
            CHECKF(len == strlen((char *)p) + 1, "string length %u vs strlen+1 %u", len, (unsigned)strlen((char *)p) + 1);
            t_note("FileDescription = \"%s\"", (char *)p);
            CHECK(strlen((char *)p) > 0);
        }
        /* key lookup is case-insensitive */
        snprintf(key2, sizeof key2, "\\stringfileinfo\\%04x%04x\\filedescription", lang, cp);
        LPVOID p2 = NULL; UINT l2 = 0;
        BOOL ok2 = U(VerQueryValueA)(buf, key2, &p2, &l2);
        t_note("lowercase key lookup: ok=%d", ok2);
        if (ok2 && ok) CHECK(p2 == p && l2 == len);
        /* known string entries */
        static const char *names[] = { "FileVersion", "InternalName", "OriginalFilename", "ProductName", "CompanyName", "LegalCopyright", "ProductVersion" };
        for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
            snprintf(key, sizeof key, "\\StringFileInfo\\%04X%04X\\%s", lang, cp, names[i]);
            p = NULL; len = 0;
            ok = U(VerQueryValueA)(buf, key, &p, &len);
            t_note("%s: ok=%d \"%s\"", names[i], ok, ok ? (char *)p : "");
            if (ok) CHECKF(len == strlen((char *)p) + 1, "%s length %u", names[i], len);
        }
        /* bad key under a valid language block */
        snprintf(key, sizeof key, "\\StringFileInfo\\%04X%04X\\NoSuchValueName", lang, cp);
        p = (LPVOID)0x1; len = 0x77;
        ok = U(VerQueryValueA)(buf, key, &p, &len);
        CHECK(!ok); CHECKF(len == 0, "len %u", len);
        /* bad language */
        p = NULL; len = 0x77;
        ok = U(VerQueryValueA)(buf, "\\StringFileInfo\\ZZZZZZZZ\\FileDescription", &p, &len);
        CHECK(!ok && len == 0);
    }
    /* string-file-info container itself */
    p = NULL; len = 0;
    ok = U(VerQueryValueA)(buf, "\\StringFileInfo", &p, &len);
    t_note("\\StringFileInfo container ok=%d len=%u", ok, len);
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(gap15_verquery_garbage_block_header) {
    REQUIRE(U(VerQueryValueA));
    /* a block whose declared size is too small is rejected without touching the output */
    static __attribute__((aligned(8))) BYTE raw[64];
    memset(raw, 0, sizeof raw);
    ((WORD *)raw)[0] = 4;   /* wLength below the 8 byte minimum */
    LPVOID p = (LPVOID)0x1; UINT len = 0x55;
    BOOL ok = U(VerQueryValueA)(raw, "\\", &p, &len);
    t_note("tiny wLength: ok=%d len=%u", ok, len);
    CHECKF(len == 0, "len %u", len);
    CHECK(p == (LPVOID)0x1);
    /* zero filled header */
    memset(raw, 0, sizeof raw);
    p = (LPVOID)0x1; len = 0x55;
    ok = U(VerQueryValueA)(raw, "\\", &p, &len);
    t_note("zero header: ok=%d", ok);
    CHECK(len == 0);
    /* 16-bit-style signature word is routed elsewhere; just make sure it fails cleanly */
    memset(raw, 0, sizeof raw);
    ((WORD *)raw)[0] = 40; ((WORD *)raw)[2] = 0x5356;
    len = 0x55;
    ok = U(VerQueryValueA)(raw, "\\", &p, &len);
    t_note("16-bit style block: ok=%d len=%u", ok, len);
    CHECK(len == 0 || ok);
    /* unknown wType */
    memset(raw, 0, sizeof raw);
    ((WORD *)raw)[0] = 40; ((WORD *)raw)[2] = 7;
    len = 0x55;
    ok = U(VerQueryValueA)(raw, "\\", &p, &len);
    CHECK(!ok && len == 0);
}

/* ------------------------------------------------------------- UnmapViewOfFile */
TEST(gap15_unmap_null) {
    REQUIRE(U(UnmapViewOfFile));
    SetLastError(0xdeadbeef);
    CHECK(U(UnmapViewOfFile)(NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
}
TEST(gap15_unmap_basic_roundtrip) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x4000, NULL);
    CHECK(m != NULL);
    if (!m) return;
    BYTE *v = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CHECK(v != NULL);
    if (!v) { CloseHandle(m); return; }
    v[0] = 0x5a; v[0x3fff] = 0xa5;
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery(v, &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.Type == MEM_MAPPED && mbi.State == MEM_COMMIT && mbi.RegionSize == 0x4000);
    SetLastError(0xdeadbeef);
    CHECK(U(UnmapViewOfFile)(v) == TRUE);
    CHECK(VirtualQuery(v, &mbi, sizeof mbi) == sizeof mbi);
    CHECKF(mbi.State == MEM_FREE, "state after unmap 0x%lx", (unsigned long)mbi.State);
    /* second unmap fails */
    SetLastError(0xdeadbeef);
    CHECK(U(UnmapViewOfFile)(v) == FALSE);
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
    /* the section object still holds the data */
    v = (BYTE *)MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    CHECK(v && v[0] == 0x5a && v[0x3fff] == 0xa5);
    if (v) CHECK(U(UnmapViewOfFile)(v));
    CloseHandle(m);
}
TEST(gap15_unmap_after_handle_close) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    if (!m) return;
    BYTE *v = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CloseHandle(m);
    CHECK(v != NULL);
    if (!v) return;
    v[100] = 7;   /* view stays valid after the mapping handle closes */
    CHECK(v[100] == 7);
    CHECK(U(UnmapViewOfFile)(v) == TRUE);
}
TEST(gap15_unmap_shared_views) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    if (!m) return;
    BYTE *v1 = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    BYTE *v2 = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CHECK(v1 && v2 && v1 != v2);
    if (v1 && v2) {
        v1[10] = 0x42;
        CHECK(v2[10] == 0x42);
        CHECK(U(UnmapViewOfFile)(v1));
        CHECK(v2[10] == 0x42);       /* other view unaffected */
        CHECK(U(UnmapViewOfFile)(v2));
    }
    CloseHandle(m);
}
TEST(gap15_unmap_file_backed_flush) {
    REQUIRE(U(UnmapViewOfFile));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "gap15_map.bin");
    HANDLE f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) return;
    HANDLE m = CreateFileMappingA(f, NULL, PAGE_READWRITE, 0, 8192, NULL);
    CHECK(m != NULL);
    BYTE *v = m ? (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0) : NULL;
    CHECK(v != NULL);
    if (v) {
        memcpy(v, "gap15-data", 10);
        CHECK(U(UnmapViewOfFile)(v));
    }
    if (m) CloseHandle(m);
    BYTE rb[16] = { 0 }; DWORD got = 0;
    SetFilePointer(f, 0, NULL, FILE_BEGIN);
    CHECK(ReadFile(f, rb, 10, &got, NULL) && got == 10);
    CHECK(!memcmp(rb, "gap15-data", 10));
    CHECK(GetFileSize(f, NULL) == 8192);
    CloseHandle(f);
    DeleteFileA(path);
}
TEST(gap15_unmap_private_memory) {
    REQUIRE(U(UnmapViewOfFile));
    BYTE *p = (BYTE *)VirtualAlloc(NULL, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    SetLastError(0xdeadbeef);
    BOOL ok = U(UnmapViewOfFile)(p);
    DWORD e = GetLastError();
    t_note("UnmapViewOfFile(private) ok=%d err=%lu", ok, e);
    CHECK(ok == FALSE);
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery(p, &mbi, sizeof mbi) == sizeof mbi && mbi.State == MEM_COMMIT);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap15_unmap_unallocated_address) {
    REQUIRE(U(UnmapViewOfFile));
    /* find a free page via reserve/release and unmap it */
    BYTE *p = (BYTE *)VirtualAlloc(NULL, 0x1000, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) return;
    VirtualFree(p, 0, MEM_RELEASE);
    SetLastError(0xdeadbeef);
    BOOL ok = U(UnmapViewOfFile)(p);
    DWORD e = GetLastError();
    CHECK(ok == FALSE);
    CHECKF(e == ERROR_INVALID_ADDRESS, "err %lu", e);
    SetLastError(0xdeadbeef);
    CHECK(U(UnmapViewOfFile)((LPCVOID)(ULONG_PTR)0x10) == FALSE);
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
    SetLastError(0xdeadbeef);
    CHECK(U(UnmapViewOfFile)((LPCVOID)(ULONG_PTR)0x7fffffff0000ULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
}
TEST(gap15_unmap_interior_address) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x3000, NULL);
    if (!m) return;
    BYTE *v = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!v) { CloseHandle(m); return; }
    SetLastError(0xdeadbeef);
    BOOL ok = U(UnmapViewOfFile)(v + 0x1000);
    DWORD e = GetLastError();
    t_note("UnmapViewOfFile(base+0x1000) ok=%d err=%lu", ok, e);
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(v, &mbi, sizeof mbi);
    if (mbi.State == MEM_COMMIT) U(UnmapViewOfFile)(v);
    CloseHandle(m);
}
static DWORD WINAPI unmap_thread(LPVOID p) { return U(UnmapViewOfFile)(p) ? 1 : 0; }
TEST(gap15_unmap_other_thread) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    if (!m) return;
    BYTE *v = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CHECK(v != NULL);
    if (v) {
        HANDLE t = CreateThread(NULL, 0, unmap_thread, v, 0, NULL);
        CHECK(t != NULL);
        if (t) {
            CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
            DWORD rc = 0; GetExitCodeThread(t, &rc);
            CHECK(rc == 1);
            CloseHandle(t);
        }
    }
    CloseHandle(m);
}
TEST(gap15_unmap_many_views_loop) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x10000, NULL);
    if (!m) return;
    for (int i = 0; i < 500; i++) {
        BYTE *v = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!v) { CHECKF(0, "map failed at %d", i); break; }
        v[i] = (BYTE)i;
        if (!U(UnmapViewOfFile)(v)) { CHECKF(0, "unmap failed at %d", i); break; }
    }
    CloseHandle(m);
}
TEST(gap15_unmap_partial_view_offset) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x20000, NULL);
    if (!m) return;
    BYTE *v = (BYTE *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0x10000, 0x1000);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery(v, &mbi, sizeof mbi);
        CHECKF(mbi.RegionSize == 0x1000, "region %llu", (unsigned long long)mbi.RegionSize);
        CHECK(U(UnmapViewOfFile)(v));
    }
    CloseHandle(m);
}
TEST(gap15_unmap_read_only_view) {
    REQUIRE(U(UnmapViewOfFile));
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    if (!m) return;
    BYTE *v = (BYTE *)MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    CHECK(v != NULL);
    if (v) CHECK(U(UnmapViewOfFile)(v));
    BYTE *c = (BYTE *)MapViewOfFile(m, FILE_MAP_COPY, 0, 0, 0);
    CHECK(c != NULL);
    if (c) { c[0] = 1; CHECK(U(UnmapViewOfFile)(c)); }
    CloseHandle(m);
}

/* ----------------------------------------------- UnregisterBadMemoryNotification */
typedef VOID (WINAPI *bad_cb)(void);
static VOID WINAPI bad_memory_cb(void) {}

CHILD(gap15_badmem_roundtrip) {
    typedef PVOID (WINAPI *reg_t)(PBAD_MEMORY_CALLBACK_ROUTINE);
    typedef BOOL (WINAPI *unreg_t)(PVOID);
    reg_t reg = (reg_t)t_proc("RegisterBadMemoryNotification");
    unreg_t unreg = (unreg_t)t_proc("UnregisterBadMemoryNotification");
    if (!unreg) return 20;
    if (!reg) return 21;
    SetLastError(0);
    PVOID h = reg((PBAD_MEMORY_CALLBACK_ROUTINE)bad_memory_cb);
    if (!h) return 10;
    if (!unreg(h)) return 11;
    return 0;
}
CHILD(gap15_badmem_null) {
    typedef BOOL (WINAPI *unreg_t)(PVOID);
    unreg_t unreg = (unreg_t)t_proc("UnregisterBadMemoryNotification");
    if (!unreg) return 20;
    SetLastError(0xdeadbeef);
    BOOL r = unreg(NULL);
    DWORD e = GetLastError();
    /* encode: bit 8 = result, error in the upper bits */
    return 0x100 | (r ? 1 : 0) | (int)((e & 0xffff) << 12);
}
TEST(gap15_badmem_register_unregister) {
    REQUIRE(U(UnregisterBadMemoryNotification));
    DWORD rc = t_run_child("gap15_badmem_roundtrip", "", 20000);
    t_note("badmem roundtrip child exit 0x%lx", (unsigned long)rc);
    if (rc == 21) { t_skip("RegisterBadMemoryNotification missing"); return; }
    CHECKF(rc == 0 || rc == 10, "register->unregister: child exit 0x%lx", (unsigned long)rc);
}
TEST(gap15_badmem_unregister_null_child) {
    REQUIRE(U(UnregisterBadMemoryNotification));
    DWORD rc = t_run_child("gap15_badmem_null", "", 20000);
    t_note("Unregister(NULL) child exit 0x%lx (bit8=result, err<<12)", (unsigned long)rc);
    CHECKF(rc != 0xDEAD0000 && rc != 0xDEAD0001, "child spawn/timeout 0x%lx", (unsigned long)rc);
}

/* ------------------------------------------------------------ UnregisterTraceGuids */
TEST(gap15_unregtraceguids_stub) {
    REQUIRE(U(UnregisterTraceGuids));
    /* decomp: UnregisterTraceGuids body is RtlSetLastWin32Error(0x7f); return 0x7f */
    /* kernelbase exports this as a stub that fails with ERROR_PROC_NOT_FOUND (127) */
    static const ULONG64 handles[] = { 0, 1, 0xdeadbeef, (ULONG64)-1 };
    for (size_t i = 0; i < 4; i++) {
        SetLastError(0xdeadbeef);
        ULONG r = U(UnregisterTraceGuids)(handles[i]);
        DWORD e = GetLastError();
        CHECKF(r == 127, "handle %llx returned %lu", (unsigned long long)handles[i], (unsigned long)r);
        CHECKF(e == 127, "handle %llx lasterror %lu", (unsigned long long)handles[i], e);
    }
}
