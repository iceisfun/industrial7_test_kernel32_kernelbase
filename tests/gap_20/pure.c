/* gap20: side-effect-free internal helpers (verified against reference analysis). */
#include "t.h"

typedef BOOL (WINAPI *ChrCmp_pfn)(WORD, WORD);
typedef DWORD (WINAPI *BaseSetLastNTError_fn)(NTSTATUS);
typedef LARGE_INTEGER *(WINAPI *BaseFormatTimeOut_fn)(LARGE_INTEGER *, ULONG);
typedef NTSTATUS (WINAPI *BaseVerifyUnicodeString_fn)(PUNICODE_STRING);
typedef DWORD (WINAPI *BaseIsDosApplication_fn)(PUNICODE_STRING);
typedef ULONG_PTR (WINAPI *BasepMapModuleHandle_fn)(ULONG_PTR, BOOLEAN);
typedef BOOL (WINAPI *Basep8Bit_fn)(PUNICODE_STRING, LPCSTR);
typedef ULONG (WINAPI *RtlNtStatusToDosError_fn)(NTSTATUS);
typedef void (WINAPI *RtlFreeUnicodeString_fn)(PUNICODE_STRING);
typedef ULONG (WINAPI *RtlGetLastWin32Error_fn)(void);

static ChrCmp_pfn chr_a(void) { return (ChrCmp_pfn)t_proc_kb("ChrCmpIA"); }
static ChrCmp_pfn chr_w(void) { return (ChrCmp_pfn)t_proc_kb("ChrCmpIW"); }

/* ---- ChrCmpIA / ChrCmpIW ---- */
TEST(gap20_chrcmpia_ascii_case_insensitive) {
    ChrCmp_pfn f = chr_a(); REQUIRE(f);
    for (int c = 'a'; c <= 'z'; c++) {
        CHECKF(f((WORD)c, (WORD)(c - 32)) == 0, "%c vs upper", c);
        CHECKF(f((WORD)(c - 32), (WORD)c) == 0, "upper vs %c", c);
        CHECKF(f((WORD)c, (WORD)c) == 0, "%c same", c);
    }
}
TEST(gap20_chrcmpia_ascii_different) {
    ChrCmp_pfn f = chr_a(); REQUIRE(f);
    for (int c = 'a'; c < 'z'; c++) {
        CHECKF(f((WORD)c, (WORD)(c + 1)) != 0, "%c vs next", c);
        CHECKF(f((WORD)(c - 32), (WORD)(c + 1)) != 0, "upper %c vs next lower", c);
    }
}
TEST(gap20_chrcmpia_digits_punct) {
    ChrCmp_pfn f = chr_a(); REQUIRE(f);
    for (int c = '0'; c <= '9'; c++) { CHECK(f((WORD)c, (WORD)c) == 0); CHECK(f((WORD)c, (WORD)(c == '9' ? '0' : c + 1)) != 0); }
    CHECK(f(' ', ' ') == 0);
    CHECK(f(' ', '\t') != 0);
    CHECK(f('-', '_') != 0);
    CHECK(f('@', '`') != 0);
    CHECK(f('[', '{') != 0);
}
TEST(gap20_chrcmpia_sign) {
    ChrCmp_pfn f = chr_a(); REQUIRE(f);
    CHECK((int)f('a', 'b') < 0);
    CHECK((int)f('b', 'a') > 0);
    CHECK((int)f('A', 'b') < 0);
    CHECK((int)f('B', 'a') > 0);
}
TEST(gap20_chrcmpia_nul) {
    ChrCmp_pfn f = chr_a(); REQUIRE(f);
    CHECK(f(0, 0) == 0);
    CHECK(f('a', 0) != 0);
    CHECK(f(0, 'a') != 0);
}
TEST(gap20_chrcmpia_high_ansi_note) {
    ChrCmp_pfn f = chr_a(); REQUIRE(f);
    t_note("ChrCmpIA(0xE9,0xC9)=%d (code page dependent)", (int)f(0xE9, 0xC9));
    t_note("ChrCmpIA(0xE9,0xE9)=%d", (int)f(0xE9, 0xE9));
}
TEST(gap20_chrcmpiw_ascii_case_insensitive) {
    ChrCmp_pfn f = chr_w(); REQUIRE(f);
    for (int c = 'a'; c <= 'z'; c++) {
        CHECKF(f((WORD)c, (WORD)(c - 32)) == 0, "%c vs upper", c);
        CHECKF(f((WORD)(c - 32), (WORD)c) == 0, "upper vs %c", c);
        CHECKF(f((WORD)c, (WORD)c) == 0, "%c same", c);
    }
}
TEST(gap20_chrcmpiw_ascii_different) {
    ChrCmp_pfn f = chr_w(); REQUIRE(f);
    for (int c = 'a'; c < 'z'; c++) {
        CHECKF(f((WORD)c, (WORD)(c + 1)) != 0, "%c vs next", c);
        CHECKF(f((WORD)(c - 32), (WORD)(c + 1)) != 0, "upper %c vs next lower", c);
    }
}
TEST(gap20_chrcmpiw_sign) {
    ChrCmp_pfn f = chr_w(); REQUIRE(f);
    CHECK((int)f(L'a', L'b') < 0);
    CHECK((int)f(L'b', L'a') > 0);
    CHECK((int)f(L'A', L'b') < 0);
    CHECK((int)f(L'B', L'a') > 0);
}
TEST(gap20_chrcmpiw_nul) {
    ChrCmp_pfn f = chr_w(); REQUIRE(f);
    CHECK(f(0, 0) == 0);
    CHECK(f(L'a', 0) != 0);
    CHECK(f(0, L'a') != 0);
}
TEST(gap20_chrcmpiw_unicode_note) {
    ChrCmp_pfn f = chr_w(); REQUIRE(f);
    t_note("ChrCmpIW(U+00E9,U+00C9)=%d", (int)f(0x00E9, 0x00C9));
    t_note("ChrCmpIW(U+03B1,U+0391)=%d", (int)f(0x03B1, 0x0391));
    t_note("ChrCmpIW(U+0430,U+0410)=%d", (int)f(0x0430, 0x0410));
    CHECK(f(0x00E9, 0x00E9) == 0);
    CHECK(f(0x4E2D, 0x4E2D) == 0);
    CHECK(f(0x4E2D, 0x6587) != 0);
}
TEST(gap20_chrcmpiw_matches_ansi_for_ascii) {
    ChrCmp_pfn a = chr_a(), w = chr_w(); REQUIRE(a); REQUIRE(w);
    for (int x = 0x21; x < 0x7f; x++)
        for (int y = 0x21; y < 0x7f; y++) {
            if (x == '-' || x == '\'' || y == '-' || y == '\'') continue; /* word-sort ignorable */
            int ra = a((WORD)x, (WORD)y) != 0, rw = w((WORD)x, (WORD)y) != 0;
            if (ra != rw) { CHECKF(0, "mismatch %c %c a=%d w=%d", x, y, ra, rw); return; }
        }
    CHECK(1);
}
TEST(gap20_chrcmpiw_equivalence_exhaustive_ascii) {
    ChrCmp_pfn w = chr_w(); REQUIRE(w);
    int bad = 0;
    for (int x = 0x21; x < 0x7f; x++)
        for (int y = 0x21; y < 0x7f; y++) {
            if (x == '-' || x == '\'' || y == '-' || y == '\'') continue; /* word-sort ignorable */
            int eq = (w((WORD)x, (WORD)y) == 0);
            int lx = (x >= 'A' && x <= 'Z') ? x + 32 : x, ly = (y >= 'A' && y <= 'Z') ? y + 32 : y;
            if (eq != (lx == ly)) bad++;
        }
    CHECKF(bad == 0, "%d mismatches", bad);
}

/* ---- BaseSetLastNTError ---- */
static const struct { NTSTATUS st; DWORD err; } nt_tab[] = {
    { 0x00000000, 0 }, { 0xC0000005, 998 }, { 0xC000000D, 87 }, { 0xC0000022, 5 }, { 0xC0000034, 2 },
    { 0xC0000035, 183 }, { 0xC000003A, 3 }, { 0xC0000008, 6 }, { 0x80000005, 234 }, { 0xC0000023, 122 },
    { 0xC0000017, 8 }, { 0xC0000001, 31 }, { 0xC00000BB, 50 }, { 0xC0000101, 145 }, { 0xC0000033, 123 },
    { 0xC0000043, 32 }, { 0xC000007F, 112 }, { 0x80000006, 18 }, { 0xC0000011, 38 }, { 0x00000102, 1460 },
    { 0xC000000F, 2 },
};
TEST(gap20_basesetlastnterror_table) {
    BaseSetLastNTError_fn f = (BaseSetLastNTError_fn)t_proc_k32("BaseSetLastNTError"); REQUIRE(f);
    RtlGetLastWin32Error_fn gle = (RtlGetLastWin32Error_fn)t_proc_nt("RtlGetLastWin32Error");
    for (size_t i = 0; i < sizeof nt_tab / sizeof nt_tab[0]; i++) {
        SetLastError(0xDEAD);
        DWORD r = f(nt_tab[i].st);
        CHECKF(r == nt_tab[i].err, "status %08lx -> %lu want %lu", (unsigned long)nt_tab[i].st, r, nt_tab[i].err);
        CHECKF(GetLastError() == nt_tab[i].err, "last error %lu want %lu", GetLastError(), nt_tab[i].err);
        if (gle) CHECK(gle() == nt_tab[i].err);
    }
}
TEST(gap20_basesetlastnterror_matches_rtl) {
    BaseSetLastNTError_fn f = (BaseSetLastNTError_fn)t_proc_k32("BaseSetLastNTError"); REQUIRE(f);
    RtlNtStatusToDosError_fn r = (RtlNtStatusToDosError_fn)t_proc_nt("RtlNtStatusToDosError"); REQUIRE(r);
    static const NTSTATUS st[] = { 0xC0000001, 0xC0000002, 0xC0000010, 0xC0000018, 0xC0000040, 0xC0000056, 0xC000009A,
                                   0xC00000A3, 0xC00000BA, 0xC00000C0, 0xC00000D4, 0xC0000120, 0xC0000194, 0xC000020C,
                                   0x40000000, 0x80000001, 0x80000004, 0x8000000B, 0xC0150001, 0xC0000FFF, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof st / sizeof st[0]; i++) {
        DWORD want = r(st[i]);
        SetLastError(0);
        DWORD got = f(st[i]);
        CHECKF(got == want, "status %08lx -> %lu want %lu", (unsigned long)st[i], got, want);
        CHECK(GetLastError() == want);
    }
}
TEST(gap20_basesetlastnterror_overwrites_previous) {
    BaseSetLastNTError_fn f = (BaseSetLastNTError_fn)t_proc_k32("BaseSetLastNTError"); REQUIRE(f);
    SetLastError(1234);
    f(0xC0000022);
    EXPECT_ERR(5);
    f(0);
    EXPECT_ERR(0);
}
TEST(gap20_basesetlastnterror_unmapped_status) {
    BaseSetLastNTError_fn f = (BaseSetLastNTError_fn)t_proc_k32("BaseSetLastNTError"); REQUIRE(f);
    RtlNtStatusToDosError_fn r = (RtlNtStatusToDosError_fn)t_proc_nt("RtlNtStatusToDosError"); REQUIRE(r);
    DWORD got = f(0xC0FFEE00);
    CHECK(got == r(0xC0FFEE00));
    t_note("unmapped status 0xC0FFEE00 -> %lu", got);
}

/* ---- BaseFormatTimeOut ---- */
TEST(gap20_baseformattimeout_basic) {
    BaseFormatTimeOut_fn f = (BaseFormatTimeOut_fn)t_proc_k32("BaseFormatTimeOut"); REQUIRE(f);
    static const struct { ULONG ms; LONGLONG want; } tab[] = {
        { 0, 0 }, { 1, -10000 }, { 2, -20000 }, { 10, -100000 }, { 100, -1000000 }, { 1000, -10000000 },
        { 60000, -600000000LL }, { 3600000, -36000000000LL }, { 0x7FFFFFFF, -21474836470000LL },
        { 0x80000000u, -21474836480000LL }, { 0xFFFFFFFEu, -42949672940000LL }, { 12345, -123450000 },
    };
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        LARGE_INTEGER li; li.QuadPart = 0x5555555555555555LL;
        LARGE_INTEGER *r = f(&li, tab[i].ms);
        CHECKF(r == &li, "ms=%lu returned %p", tab[i].ms, (void *)r);
        CHECKF(li.QuadPart == tab[i].want, "ms=%lu got %lld want %lld", tab[i].ms, li.QuadPart, tab[i].want);
    }
}
TEST(gap20_baseformattimeout_infinite) {
    BaseFormatTimeOut_fn f = (BaseFormatTimeOut_fn)t_proc_k32("BaseFormatTimeOut"); REQUIRE(f);
    LARGE_INTEGER li; li.QuadPart = 0x1122334455667788LL;
    CHECK(f(&li, INFINITE) == NULL);
    CHECK(li.QuadPart == 0x1122334455667788LL);
}
TEST(gap20_baseformattimeout_zero_is_not_null) {
    BaseFormatTimeOut_fn f = (BaseFormatTimeOut_fn)t_proc_k32("BaseFormatTimeOut"); REQUIRE(f);
    LARGE_INTEGER li; li.QuadPart = 99;
    CHECK(f(&li, 0) == &li);
    CHECK(li.QuadPart == 0);
}
TEST(gap20_baseformattimeout_repeat_same_buffer) {
    BaseFormatTimeOut_fn f = (BaseFormatTimeOut_fn)t_proc_k32("BaseFormatTimeOut"); REQUIRE(f);
    LARGE_INTEGER li;
    for (ULONG ms = 1; ms < 2000; ms += 37) { CHECK(f(&li, ms) == &li); CHECK(li.QuadPart == -(LONGLONG)ms * 10000); }
}
TEST(gap20_baseformattimeout_preserves_lasterror) {
    BaseFormatTimeOut_fn f = (BaseFormatTimeOut_fn)t_proc_k32("BaseFormatTimeOut"); REQUIRE(f);
    LARGE_INTEGER li; SetLastError(0x4242);
    f(&li, 5); EXPECT_ERR(0x4242);
    f(&li, INFINITE); EXPECT_ERR(0x4242);
}

/* ---- BaseVerifyUnicodeString ---- */
#define STATUS_ACCESS_DENIED_ ((NTSTATUS)0xC0000022)
static NTSTATUS verify(USHORT len, USHORT max, PWSTR buf) {
    BaseVerifyUnicodeString_fn f = (BaseVerifyUnicodeString_fn)t_proc_k32("BaseVerifyUnicodeString");
    UNICODE_STRING us; us.Length = len; us.MaximumLength = max; us.Buffer = buf;
    return f(&us);
}
TEST(gap20_baseverifyunicodestring_null_buffer_empty_ok) {
    REQUIRE(t_proc_k32("BaseVerifyUnicodeString"));
    CHECK(verify(0, 0, NULL) == 0);
}
TEST(gap20_baseverifyunicodestring_null_buffer_with_length_fails) {
    REQUIRE(t_proc_k32("BaseVerifyUnicodeString"));
    CHECK(verify(2, 0, NULL) == STATUS_ACCESS_DENIED_);
    CHECK(verify(0, 2, NULL) == STATUS_ACCESS_DENIED_);
    CHECK(verify(4, 4, NULL) == STATUS_ACCESS_DENIED_);
    CHECK(verify(0xFFFE, 0xFFFE, NULL) == STATUS_ACCESS_DENIED_);
}
TEST(gap20_baseverifyunicodestring_bad_lengths_with_buffer) {
    REQUIRE(t_proc_k32("BaseVerifyUnicodeString"));
    WCHAR buf[16] = { 0 };
    CHECK(verify(0, 0, buf) == STATUS_ACCESS_DENIED_);        /* MaximumLength 0 rejected */
    CHECK(verify(4, 2, buf) == STATUS_ACCESS_DENIED_);        /* Length > MaximumLength */
    CHECK(verify(1, 2, buf) == STATUS_ACCESS_DENIED_);        /* odd Length */
    CHECK(verify(2, 3, buf) == STATUS_ACCESS_DENIED_);        /* odd MaximumLength */
    CHECK(verify(3, 5, buf) == STATUS_ACCESS_DENIED_);
}
TEST(gap20_baseverifyunicodestring_private_buffer_rejected) {
    REQUIRE(t_proc_k32("BaseVerifyUnicodeString"));
    /* Only buffers inside the CSR read-only shared section verify; ordinary memory does not. */
    WCHAR stackbuf[8] = { 0 };
    CHECK(verify(4, 16, stackbuf) == STATUS_ACCESS_DENIED_);
    WCHAR *heap = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, 64);
    CHECK(verify(4, 64, heap) == STATUS_ACCESS_DENIED_);
    HeapFree(GetProcessHeap(), 0, heap);
    static WCHAR global[8];
    CHECK(verify(2, 16, global) == STATUS_ACCESS_DENIED_);
}
TEST(gap20_baseverifyunicodestring_shared_section_accepted) {
    REQUIRE(t_proc_k32("BaseVerifyUnicodeString"));
    PVOID ro = *(PVOID *)((BYTE *)NtCurrentTeb()->ProcessEnvironmentBlock + 0x88);
    if (!ro) { t_skip("no read-only shared memory"); return; }
    NTSTATUS st = verify(2, 2, (PWSTR)ro);
    if (t_is_wine()) { t_note("shared section verify -> %08lx", (unsigned long)st); return; }
    CHECK(st == 0);
    CHECK(verify(2, 16, (PWSTR)ro) == 0);
    CHECK(verify(4, 4, (PWSTR)((BYTE *)ro + 0x10)) == 0);
}
TEST(gap20_baseverifyunicodestring_before_shared_section_rejected) {
    REQUIRE(t_proc_k32("BaseVerifyUnicodeString"));
    PVOID ro = *(PVOID *)((BYTE *)NtCurrentTeb()->ProcessEnvironmentBlock + 0x88);
    if (!ro) { t_skip("no read-only shared memory"); return; }
    CHECK(verify(2, 2, (PWSTR)((BYTE *)ro - 2)) == STATUS_ACCESS_DENIED_ || t_is_wine());
}

/* ---- BaseIsDosApplication ---- */
static DWORD dosapp(const WCHAR *s, int trim_chars) {
    BaseIsDosApplication_fn f = (BaseIsDosApplication_fn)t_proc_k32("BaseIsDosApplication");
    UNICODE_STRING us; size_t n = lstrlenW(s) - trim_chars;
    us.Buffer = (PWSTR)s; us.Length = (USHORT)(n * 2); us.MaximumLength = (USHORT)((n + 1) * 2);
    return (DWORD)f(&us);
}
#define DOS(nm, str, want) TEST(gap20_baseisdosapplication_##nm) { REQUIRE(t_proc_k32("BaseIsDosApplication")); \
    DWORD r = dosapp(str, 0); CHECKF(r == (want), "%ls -> %lu want %d", str, r, (int)(want)); }
DOS(exe, L"C:\\dir\\prog.exe", 1)
DOS(exe_upper, L"C:\\dir\\PROG.EXE", 1)
DOS(exe_mixed, L"prog.ExE", 1)
DOS(exe_bare, L".exe", 1)
DOS(com, L"C:\\dir\\prog.com", 2)
DOS(com_upper, L"PROG.COM", 2)
DOS(com_bare, L".com", 2)
DOS(pif, L"prog.pif", 3)
DOS(pif_upper, L"C:\\x\\PROG.PIF", 3)
DOS(pif_bare, L".PiF", 3)
DOS(bat, L"prog.bat", 0)
DOS(cmd, L"prog.cmd", 0)
DOS(dll, L"prog.dll", 0)
DOS(txt, L"prog.txt", 0)
DOS(no_ext, L"program", 0)
DOS(exe_trailing_space, L"prog.exe ", 0)
DOS(exe_trailing_dot, L"prog.exe.", 0)
DOS(exe_double, L"prog.exe.txt", 0)
DOS(exe_in_name, L"exe", 0)
DOS(exe_no_dot, L"progexe", 0)
DOS(too_short, L"a.ex", 0)
DOS(empty, L"", 0)
DOS(short3, L"com", 0)
DOS(exe_suffix_longer, L"prog.exex", 0)
DOS(exe_dir_name, L"C:\\prog.exe\\file", 0)
TEST(gap20_baseisdosapplication_respects_length) {
    REQUIRE(t_proc_k32("BaseIsDosApplication"));
    CHECK(dosapp(L"prog.exe.txt", 4) == 1);   /* Length covers only "prog.exe" */
    CHECK(dosapp(L"prog.com.txt", 4) == 2);
    CHECK(dosapp(L"prog.pif.txt", 4) == 3);
    CHECK(dosapp(L"prog.exe", 4) == 0);        /* "prog" */
}
TEST(gap20_baseisdosapplication_unicode_names) {
    REQUIRE(t_proc_k32("BaseIsDosApplication"));
    CHECK(dosapp(L"\x00e9\x00e9\x00e9.exe", 0) == 1);
    CHECK(dosapp(L"\x4e2d\x6587.com", 0) == 2);
    CHECK(dosapp(L"\x4e2d\x6587.pif", 0) == 3);
}

/* ---- BasepMapModuleHandle ---- */
TEST(gap20_basepmapmodulehandle_null_is_exe) {
    BasepMapModuleHandle_fn f = (BasepMapModuleHandle_fn)t_proc_k32("BasepMapModuleHandle"); REQUIRE(f);
    HMODULE exe = GetModuleHandleW(NULL);
    CHECK(f(0, 0) == (ULONG_PTR)exe);
    CHECK(f(0, 1) == (ULONG_PTR)exe);
    CHECK(f(0, 0) == *(ULONG_PTR *)((BYTE *)NtCurrentTeb()->ProcessEnvironmentBlock + 0x10));
}
TEST(gap20_basepmapmodulehandle_aligned_passthrough) {
    BasepMapModuleHandle_fn f = (BasepMapModuleHandle_fn)t_proc_k32("BasepMapModuleHandle"); REQUIRE(f);
    ULONG_PTR k32 = (ULONG_PTR)t_kernel32();
    CHECK(f(k32, 0) == k32);
    CHECK(f(k32, 1) == k32);
    CHECK(f(0x10000, 0) == 0x10000);
    CHECK(f(4, 0) == 4);
    CHECK(f(8, 0) == 8);
    CHECK(f(0x7FFE0000, 0) == 0x7FFE0000);
}
TEST(gap20_basepmapmodulehandle_datafile_bits) {
    BasepMapModuleHandle_fn f = (BasepMapModuleHandle_fn)t_proc_k32("BasepMapModuleHandle"); REQUIRE(f);
    for (ULONG_PTR low = 1; low <= 3; low++) {
        ULONG_PTR h = 0x180000000ULL | low;
        CHECKF(f(h, 0) == 0, "low=%lu flag0", (unsigned long)low);
        CHECKF(f(h, 1) == h, "low=%lu flag1", (unsigned long)low);
    }
    CHECK(f(1, 0) == 0);
    CHECK(f(2, 0) == 0);
    CHECK(f(3, 0) == 0);
    CHECK(f(1, 1) == 1);
    CHECK(f(2, 1) == 2);
    CHECK(f(3, 1) == 3);
}
TEST(gap20_basepmapmodulehandle_nonzero_flag_values) {
    BasepMapModuleHandle_fn f = (BasepMapModuleHandle_fn)t_proc_k32("BasepMapModuleHandle"); REQUIRE(f);
    CHECK(f(0x180000001ULL, 0xFF) == 0x180000001ULL);
    CHECK(f(0x180000002ULL, 2) == 0x180000002ULL);
}

/* ---- Basep8BitStringToDynamicUnicodeString / BasepAnsiStringToDynamicUnicodeString ---- */
static void dyn_basic(const char *name) {
    Basep8Bit_fn f = (Basep8Bit_fn)t_proc_k32(name);
    RtlFreeUnicodeString_fn fr = (RtlFreeUnicodeString_fn)t_proc_nt("RtlFreeUnicodeString");
    if (!f || !fr) { t_skip("missing export"); return; }
    UNICODE_STRING us; memset(&us, 0xCC, sizeof us);
    SetLastError(0);
    BOOL r = (BOOL)(f(&us, "hello") & 1);
    CHECK(r == TRUE);
    if (r) {
        CHECK(us.Length == 10);
        CHECK(us.MaximumLength >= 10);
        CHECK(us.Buffer != NULL);
        if (us.Buffer) CHECK(memcmp(us.Buffer, L"hello", 10) == 0);
        fr(&us);
    }
}
TEST(gap20_basep8bit_basic) { dyn_basic("Basep8BitStringToDynamicUnicodeString"); }
TEST(gap20_basepansi_basic) { dyn_basic("BasepAnsiStringToDynamicUnicodeString"); }

static void dyn_empty(const char *name) {
    Basep8Bit_fn f = (Basep8Bit_fn)t_proc_k32(name);
    RtlFreeUnicodeString_fn fr = (RtlFreeUnicodeString_fn)t_proc_nt("RtlFreeUnicodeString");
    if (!f || !fr) { t_skip("missing export"); return; }
    UNICODE_STRING us; memset(&us, 0xCC, sizeof us);
    BOOL r = (BOOL)(f(&us, "") & 1);
    CHECK(r == TRUE);
    if (r) { CHECK(us.Length == 0); t_note("%s empty: Max=%u Buffer=%p", name, us.MaximumLength, (void *)us.Buffer); fr(&us); }
}
TEST(gap20_basep8bit_empty) { dyn_empty("Basep8BitStringToDynamicUnicodeString"); }
TEST(gap20_basepansi_empty) { dyn_empty("BasepAnsiStringToDynamicUnicodeString"); }

static void dyn_null(const char *name) {
    Basep8Bit_fn f = (Basep8Bit_fn)t_proc_k32(name);
    RtlFreeUnicodeString_fn fr = (RtlFreeUnicodeString_fn)t_proc_nt("RtlFreeUnicodeString");
    if (!f || !fr) { t_skip("missing export"); return; }
    UNICODE_STRING us; memset(&us, 0xCC, sizeof us);
    BOOL r = (BOOL)(f(&us, NULL) & 1);
    CHECK(r == TRUE);
    if (r) { CHECK(us.Length == 0); fr(&us); }
}
TEST(gap20_basep8bit_null_source) { dyn_null("Basep8BitStringToDynamicUnicodeString"); }
TEST(gap20_basepansi_null_source) { dyn_null("BasepAnsiStringToDynamicUnicodeString"); }

static void dyn_lens(const char *name) {
    Basep8Bit_fn f = (Basep8Bit_fn)t_proc_k32(name);
    RtlFreeUnicodeString_fn fr = (RtlFreeUnicodeString_fn)t_proc_nt("RtlFreeUnicodeString");
    if (!f || !fr) { t_skip("missing export"); return; }
    static const int lens[] = { 1, 2, 3, 15, 16, 17, 255, 256, 260, 1000, 4096, 30000 };
    for (size_t i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        int n = lens[i];
        char *s = (char *)HeapAlloc(GetProcessHeap(), 0, n + 1);
        for (int k = 0; k < n; k++) s[k] = (char)('a' + k % 26);
        s[n] = 0;
        UNICODE_STRING us; memset(&us, 0, sizeof us);
        BOOL r = (BOOL)(f(&us, s) & 1);
        CHECKF(r, "len %d", n);
        if (r) {
            CHECKF(us.Length == n * 2, "len %d got %u", n, us.Length);
            int ok = 1;
            for (int k = 0; k < n && ok; k++) ok = us.Buffer[k] == (WCHAR)('a' + k % 26);
            CHECKF(ok, "content len %d", n);
            CHECK(us.Buffer[n] == 0 || us.MaximumLength == us.Length);
            fr(&us);
        }
        HeapFree(GetProcessHeap(), 0, s);
    }
}
TEST(gap20_basep8bit_lengths) { dyn_lens("Basep8BitStringToDynamicUnicodeString"); }
TEST(gap20_basepansi_lengths) { dyn_lens("BasepAnsiStringToDynamicUnicodeString"); }

static void dyn_toolong(const char *name) {
    Basep8Bit_fn f = (Basep8Bit_fn)t_proc_k32(name);
    if (!f) { t_skip("missing export"); return; }
    /* > 65535 bytes: RtlInitAnsiStringEx fails -> ERROR_FILENAME_EXCED_RANGE */
    int n = 70000;
    char *s = (char *)HeapAlloc(GetProcessHeap(), 0, n + 1);
    memset(s, 'x', n); s[n] = 0;
    UNICODE_STRING us; memset(&us, 0, sizeof us);
    SetLastError(0);
    BOOL r = (BOOL)(f(&us, s) & 1);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
    CHECK(us.Buffer == NULL);
    /* Fits as ANSI but the UTF-16 form would exceed 64 KiB */
    n = 40000;
    memset(s, 'y', n); s[n] = 0;
    SetLastError(0);
    r = (BOOL)(f(&us, s) & 1);
    CHECK(r == FALSE);
    t_note("%s 40000 chars: err=%lu", name, GetLastError());
    CHECK(us.Buffer == NULL);
    HeapFree(GetProcessHeap(), 0, s);
}
TEST(gap20_basep8bit_too_long) { dyn_toolong("Basep8BitStringToDynamicUnicodeString"); }
TEST(gap20_basepansi_too_long) { dyn_toolong("BasepAnsiStringToDynamicUnicodeString"); }

TEST(gap20_basepansi_matches_ntdll_conversion) {
    Basep8Bit_fn f = (Basep8Bit_fn)t_proc_k32("BasepAnsiStringToDynamicUnicodeString"); REQUIRE(f);
    RtlFreeUnicodeString_fn fr = (RtlFreeUnicodeString_fn)t_proc_nt("RtlFreeUnicodeString"); REQUIRE(fr);
    const char *tests[] = { "C:\\Windows\\System32", "A", "mixed Case 123 !@#", "tab\there" };
    for (int i = 0; i < 4; i++) {
        UNICODE_STRING us; memset(&us, 0, sizeof us);
        CHECK(f(&us, tests[i]) & 1);
        WCHAR ref[64]; int n = MultiByteToWideChar(CP_ACP, 0, tests[i], -1, ref, 64) - 1;
        CHECKF(us.Length == n * 2, "len %u want %d", us.Length, n * 2);
        if (us.Buffer) CHECK(memcmp(us.Buffer, ref, n * 2) == 0);
        fr(&us);
    }
}
TEST(gap20_basep8bit_ascii_matches_ansi) {
    Basep8Bit_fn a = (Basep8Bit_fn)t_proc_k32("BasepAnsiStringToDynamicUnicodeString");
    Basep8Bit_fn b = (Basep8Bit_fn)t_proc_k32("Basep8BitStringToDynamicUnicodeString");
    RtlFreeUnicodeString_fn fr = (RtlFreeUnicodeString_fn)t_proc_nt("RtlFreeUnicodeString");
    REQUIRE(a); REQUIRE(b); REQUIRE(fr);
    UNICODE_STRING u1, u2; memset(&u1, 0, sizeof u1); memset(&u2, 0, sizeof u2);
    CHECK(a(&u1, "Same ASCII text") & 1);
    CHECK(b(&u2, "Same ASCII text") & 1);
    CHECK(u1.Length == u2.Length);
    if (u1.Buffer && u2.Buffer && u1.Length == u2.Length) CHECK(memcmp(u1.Buffer, u2.Buffer, u1.Length) == 0);
    fr(&u1); fr(&u2);
}
