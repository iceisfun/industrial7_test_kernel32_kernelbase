/* gap7: LoadStringA / LoadStringW / LoadStringByReference.
 * A tiny resource-only PE is generated into the scratch directory and loaded as a data file / image resource. */
#include "t.h"
#include <wchar.h>

struct rblock { WORD id; const WCHAR *s[16]; };

static WCHAR g_long300[301];

static const struct rblock g_blocks[] = {
    { 1,    { L"Zero", L"One", L"", L"Say \"hi\" 100%", L"Uni \x4e2d\x6587 \x00e9", L"A", NULL, L"seven\r\nlines\ttab" } },
    { 2,    { L"Sixteen", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, L"ThirtyOne" } },
    { 65,   { g_long300 } },
    { 4096, { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, L"Top" } },
};
#define NBLK (sizeof g_blocks / sizeof g_blocks[0])

static void put16(BYTE *b, DWORD off, WORD v) { memcpy(b + off, &v, 2); }
static void put32(BYTE *b, DWORD off, DWORD v) { memcpy(b + off, &v, 4); }

/* Build the PE in memory and write to path. Returns 1 on success. */
static int build_res_dll(const char *path) {
    for (int i = 0; i < 300; i++) g_long300[i] = (WCHAR)(L'a' + (i % 26));
    g_long300[300] = 0;
    BYTE *rs = calloc(1, 0x4000);
    DWORD off = 0;
    /* root dir: 1 id entry (RT_STRING) */
    DWORD root = off; off += 16 + 8;
    DWORD typeDir = off; off += 16 + 8 * NBLK;
    DWORD nameDir[NBLK], dataEnt[NBLK];
    for (size_t i = 0; i < NBLK; i++) { nameDir[i] = off; off += 16 + 8; }
    for (size_t i = 0; i < NBLK; i++) { dataEnt[i] = off; off += 16; }
    DWORD dataOff[NBLK], dataLen[NBLK];
    for (size_t i = 0; i < NBLK; i++) {
        off = (off + 3) & ~3u;
        dataOff[i] = off;
        DWORD p = off;
        for (int k = 0; k < 16; k++) {
            const WCHAR *s = g_blocks[i].s[k];
            WORD n = s ? (WORD)lstrlenW(s) : 0;
            put16(rs, p, n); p += 2;
            if (n) { memcpy(rs + p, s, n * 2); p += n * 2; }
        }
        dataLen[i] = p - off; off = p;
    }
    const DWORD RVA = 0x1000;
    put16(rs, root + 14, 1); put32(rs, root + 16, 6); put32(rs, root + 20, 0x80000000u | typeDir);
    put16(rs, typeDir + 14, (WORD)NBLK);
    for (size_t i = 0; i < NBLK; i++) {
        put32(rs, typeDir + 16 + 8 * i, g_blocks[i].id);
        put32(rs, typeDir + 20 + 8 * i, 0x80000000u | nameDir[i]);
        put16(rs, nameDir[i] + 14, 1); put32(rs, nameDir[i] + 16, 0x0409); put32(rs, nameDir[i] + 20, dataEnt[i]);
        put32(rs, dataEnt[i], RVA + dataOff[i]); put32(rs, dataEnt[i] + 4, dataLen[i]); put32(rs, dataEnt[i] + 8, 0);
    }
    DWORD vsize = off, rawsize = (off + 0x1FF) & ~0x1FFu;
    BYTE *img = calloc(1, 0x400 + rawsize);
    IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER *)img; dh->e_magic = IMAGE_DOS_SIGNATURE; dh->e_lfanew = 0x80;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(img + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt->FileHeader.NumberOfSections = 1;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt->FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE | IMAGE_FILE_DLL;
    IMAGE_OPTIONAL_HEADER64 *oh = &nt->OptionalHeader;
    oh->Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    oh->SizeOfInitializedData = rawsize;
    oh->ImageBase = 0x180000000ull;
    oh->SectionAlignment = 0x1000; oh->FileAlignment = 0x200;
    oh->MajorOperatingSystemVersion = 6; oh->MajorSubsystemVersion = 6;
    oh->SizeOfImage = 0x1000 + ((vsize + 0xFFF) & ~0xFFFu);
    oh->SizeOfHeaders = 0x400;
    oh->Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
    oh->NumberOfRvaAndSizes = 16;
    oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = RVA;
    oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = vsize;
    IMAGE_SECTION_HEADER *sh = (IMAGE_SECTION_HEADER *)((BYTE *)oh + sizeof *oh);
    memcpy(sh->Name, ".rsrc", 5);
    sh->Misc.VirtualSize = vsize; sh->VirtualAddress = RVA; sh->SizeOfRawData = rawsize; sh->PointerToRawData = 0x400;
    sh->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
    memcpy(img + 0x400, rs, vsize);
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    int ok = 0;
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w; ok = WriteFile(f, img, 0x400 + rawsize, &w, NULL) && w == 0x400 + rawsize;
        CloseHandle(f);
    }
    free(rs); free(img);
    return ok;
}

static HMODULE g_mod[2];
static int g_built;
static HMODULE load_res(int image_resource) {
    char p[MAX_PATH];
    t_tmpfile(p, sizeof p, "g7res.dll");
    if (!g_built) g_built = build_res_dll(p) ? 1 : -1;
    if (g_built < 0) return NULL;
    if (!g_mod[image_resource]) {
        g_mod[image_resource] = LoadLibraryExA(p, NULL, image_resource ? LOAD_LIBRARY_AS_IMAGE_RESOURCE : LOAD_LIBRARY_AS_DATAFILE);
        if (!g_mod[image_resource]) t_note("LoadLibraryEx(res dll) failed %lu", GetLastError());
    }
    return g_mod[image_resource];
}

#define NEED_MOD(var) HMODULE var = load_res(0); if (!var) { t_skip("cannot load generated resource module"); return; }

/* ---------- LoadStringW ---------- */
TEST(gap7_loadstringW_basic) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    WCHAR buf[64];
    SET_ERR(0);
    int n = U(LoadStringW)(m, 0, buf, 64);
    CHECK(n == 4); CHECK(!lstrcmpW(buf, L"Zero"));
    n = U(LoadStringW)(m, 1, buf, 64); CHECK(n == 3); CHECK(!lstrcmpW(buf, L"One"));
    n = U(LoadStringW)(m, 3, buf, 64); CHECK(n == 13); CHECK(!lstrcmpW(buf, L"Say \"hi\" 100%"));
    n = U(LoadStringW)(m, 4, buf, 64); CHECK(n == 8); CHECK(!lstrcmpW(buf, L"Uni \x4e2d\x6587 \x00e9"));
    n = U(LoadStringW)(m, 5, buf, 64); CHECK(n == 1); CHECK(buf[0] == L'A' && buf[1] == 0);
    n = U(LoadStringW)(m, 7, buf, 64); CHECK(n == 16); CHECK(!lstrcmpW(buf, L"seven\r\nlines\ttab"));
    n = U(LoadStringW)(m, 16, buf, 64); CHECK(n == 7); CHECK(!lstrcmpW(buf, L"Sixteen"));
    n = U(LoadStringW)(m, 31, buf, 64); CHECK(n == 9); CHECK(!lstrcmpW(buf, L"ThirtyOne"));
    n = U(LoadStringW)(m, 65535, buf, 64); CHECK(n == 3); CHECK(!lstrcmpW(buf, L"Top"));
}

TEST(gap7_loadstringW_image_resource_module) {
    REQUIRE(U(LoadStringW));
    HMODULE m = load_res(1);
    if (!m) { t_skip("LOAD_LIBRARY_AS_IMAGE_RESOURCE unsupported"); return; }
    WCHAR buf[64];
    CHECK(U(LoadStringW)(m, 0, buf, 64) == 4);
    CHECK(!lstrcmpW(buf, L"Zero"));
    CHECK(U(LoadStringW)(m, 16, buf, 64) == 7);
}

TEST(gap7_loadstringW_truncation) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    WCHAR buf[64];
    /* "Zero" = 4 chars */
    for (int cch = 1; cch <= 8; cch++) {
        for (int i = 0; i < 64; i++) buf[i] = 0x7777;
        int n = U(LoadStringW)(m, 0, buf, cch);
        int want = cch - 1 < 4 ? cch - 1 : 4;
        CHECKF(n == want, "cch=%d got %d want %d", cch, n, want);
        CHECKF(buf[want] == 0, "cch=%d NUL at %d", cch, want);
        CHECKF(buf[cch < 64 ? cch : 63] == 0x7777 || cch > want + 1, "cch=%d overwrote beyond buffer", cch);
        if (want) CHECKF(!wmemcmp(buf, L"Zero", want), "cch=%d prefix", cch);
    }
}

TEST(gap7_loadstringW_cch1_and_exact) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    WCHAR buf[8] = { 0x7777, 0x7777, 0x7777 };
    CHECK(U(LoadStringW)(m, 0, buf, 1) == 0);
    CHECK(buf[0] == 0);
    CHECK(buf[1] == 0x7777);
    for (int i = 0; i < 8; i++) buf[i] = 0x7777;
    CHECK(U(LoadStringW)(m, 0, buf, 5) == 4);   /* exact fit incl. NUL */
    CHECK(buf[4] == 0);
    for (int i = 0; i < 8; i++) buf[i] = 0x7777;
    CHECK(U(LoadStringW)(m, 0, buf, 4) == 3);   /* exact-1 */
    CHECK(buf[3] == 0 && buf[4] == 0x7777);
}

TEST(gap7_loadstringW_readonly_pointer_mode) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    /* cchBufferMax == 0: lpBuffer receives a pointer to the string inside the module (not NUL-terminated) */
    LPWSTR p = NULL;
    int n = U(LoadStringW)(m, 0, (LPWSTR)&p, 0);
    CHECK(n == 4);
    CHECK(p != NULL);
    if (p) CHECK(!wmemcmp(p, L"Zero", 4));
    p = NULL;
    n = U(LoadStringW)(m, 3, (LPWSTR)&p, 0);
    CHECK(n == 13);
    if (p) CHECK(!wmemcmp(p, L"Say \"hi\" 100%", 13));
    p = (LPWSTR)0x1; 
    n = U(LoadStringW)(m, 2, (LPWSTR)&p, 0);
    CHECK(n == 0);
    t_note("pointer mode, empty string: p=%p", (void *)p);
}

TEST(gap7_loadstringW_missing_ids) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    static const UINT ids[] = { 2, 6, 8, 15, 17, 30, 32, 47, 1040, 4095, 65534 };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        WCHAR buf[16]; for (int k = 0; k < 16; k++) buf[k] = 0x7777;
        SET_ERR(0);
        int n = U(LoadStringW)(m, ids[i], buf, 16);
        DWORD e = GetLastError();
        CHECKF(n == 0, "id %u -> %d", ids[i], n);
        CHECKF(buf[0] == 0, "id %u buffer not cleared", ids[i]);
        t_note("LoadStringW missing id %u: err %lu", ids[i], e);
        CHECKF(e != 0 || ids[i] == 2 || ids[i] == 6, "id %u: no error set", ids[i]);
    }
}

TEST(gap7_loadstringW_huge_ids_note) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    WCHAR buf[16];
    static const UINT ids[] = { 0x10000, 0x10001, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SET_ERR(0);
        int n = U(LoadStringW)(m, ids[i], buf, 16);
        t_note("LoadStringW(id=0x%x) -> %d err %lu", ids[i], n, GetLastError());
    }
}

TEST(gap7_loadstringW_null_buffer) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    SET_ERR(0);
    CHECK(U(LoadStringW)(m, 0, NULL, 0) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(LoadStringW)(m, 0, NULL, 16) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(LoadStringW)(m, 12345, NULL, 16) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap7_loadstringW_long_string) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    WCHAR buf[512];
    int n = U(LoadStringW)(m, 1024, buf, 512);
    CHECK(n == 300);
    CHECK(!wmemcmp(buf, g_long300, 300) && buf[300] == 0);
    n = U(LoadStringW)(m, 1024, buf, 301); CHECK(n == 300);
    n = U(LoadStringW)(m, 1024, buf, 300); CHECK(n == 299); CHECK(buf[299] == 0);
    n = U(LoadStringW)(m, 1024, buf, 100); CHECK(n == 99); CHECK(buf[99] == 0 && !wmemcmp(buf, g_long300, 99));
    LPWSTR p = NULL;
    n = U(LoadStringW)(m, 1024, (LPWSTR)&p, 0); CHECK(n == 300);
}

TEST(gap7_loadstringW_null_instance_is_exe) {
    REQUIRE(U(LoadStringW));
    WCHAR buf[16] = { 0x7777 };
    SET_ERR(0);
    /* the test exe carries no string table */
    int n = U(LoadStringW)(NULL, 1, buf, 16);
    CHECK(n == 0);
    CHECK(buf[0] == 0);
    t_note("LoadStringW(NULL,1) error %lu", GetLastError());
    n = U(LoadStringW)(GetModuleHandleA(NULL), 1, buf, 16);
    CHECK(n == 0);
}

TEST(gap7_loadstringW_repeat) {
    REQUIRE(U(LoadStringW)); NEED_MOD(m);
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        WCHAR buf[32];
        if (U(LoadStringW)(m, (UINT)(i & 1) ? 16 : 0, buf, 32) != ((i & 1) ? 7 : 4)) bad++;
    }
    CHECK(bad == 0);
}

TEST(gap7_loadstringW_system_dll_strings) {
    REQUIRE(U(LoadStringW));
    /* a loaded system module without the requested id fails cleanly */
    HMODULE k = GetModuleHandleA("kernel32.dll");
    WCHAR buf[8] = { 0x7777 };
    SET_ERR(0);
    CHECK(U(LoadStringW)(k, 0xFFF0, buf, 8) == 0);
    CHECK(buf[0] == 0);
    CHECK(GetLastError() != 0);
}

/* ---------- LoadStringA ---------- */
TEST(gap7_loadstringA_basic) {
    REQUIRE(U(LoadStringA)); NEED_MOD(m);
    char buf[64];
    int n = U(LoadStringA)(m, 0, buf, 64); CHECK(n == 4); EXPECT_STR(buf, "Zero");
    n = U(LoadStringA)(m, 1, buf, 64); CHECK(n == 3); EXPECT_STR(buf, "One");
    n = U(LoadStringA)(m, 3, buf, 64); CHECK(n == 13); EXPECT_STR(buf, "Say \"hi\" 100%");
    n = U(LoadStringA)(m, 5, buf, 64); CHECK(n == 1); EXPECT_STR(buf, "A");
    n = U(LoadStringA)(m, 7, buf, 64); CHECK(n == 16); EXPECT_STR(buf, "seven\r\nlines\ttab");
    n = U(LoadStringA)(m, 16, buf, 64); CHECK(n == 7); EXPECT_STR(buf, "Sixteen");
    n = U(LoadStringA)(m, 65535, buf, 64); CHECK(n == 3); EXPECT_STR(buf, "Top");
}

TEST(gap7_loadstringA_exact_fit_and_big) {
    REQUIRE(U(LoadStringA)); NEED_MOD(m);
    char buf[512];
    memset(buf, 0x77, sizeof buf);
    CHECK(U(LoadStringA)(m, 0, buf, 5) == 4);
    CHECK(buf[4] == 0 && !memcmp(buf, "Zero", 4));
    CHECK(U(LoadStringA)(m, 1024, buf, 512) == 300);
    CHECK(buf[300] == 0 && buf[0] == 'a' && buf[299] == 'a' + (299 % 26));
}

TEST(gap7_loadstringA_truncation_note) {
    REQUIRE(U(LoadStringA)); NEED_MOD(m);
    char buf[16];
    for (int cch = 1; cch <= 5; cch++) {
        memset(buf, 0x77, sizeof buf);
        int n = U(LoadStringA)(m, 0, buf, cch);
        int len = (int)strlen(buf);
        t_note("LoadStringA(cch=%d) -> %d, strlen %d", cch, n, len);
        CHECKF(len < cch, "cch=%d result not terminated within buffer", cch);
        CHECKF(n <= cch, "cch=%d returned %d", cch, n);
    }
}

TEST(gap7_loadstringA_missing_and_null) {
    REQUIRE(U(LoadStringA)); NEED_MOD(m);
    char buf[16]; memset(buf, 0x77, sizeof buf);
    SET_ERR(0);
    CHECK(U(LoadStringA)(m, 9999, buf, 16) == 0);
    t_note("LoadStringA missing id: err %lu buf[0]=%d", GetLastError(), buf[0]);
    CHECK(U(LoadStringA)(m, 2, buf, 16) == 0);
    CHECK(buf[0] == 0);
    if (!t_is_wine()) {
        SET_ERR(0);
        CHECK(U(LoadStringA)(m, 0, NULL, 16) == 0);
        t_note("LoadStringA NULL buf: err %lu", GetLastError());
    }
}

TEST(gap7_loadstringA_W_length_parity) {
    REQUIRE(U(LoadStringA)); REQUIRE(U(LoadStringW)); NEED_MOD(m);
    static const UINT ids[] = { 0, 1, 3, 5, 7, 16, 31, 1024, 65535 };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        char a[512]; WCHAR w[512];
        int na = U(LoadStringA)(m, ids[i], a, 512), nw = U(LoadStringW)(m, ids[i], w, 512);
        CHECKF(na == nw, "id %u A=%d W=%d", ids[i], na, nw);
        if (na == nw && na > 0) {
            WCHAR conv[512]; int c = MultiByteToWideChar(CP_ACP, 0, a, na, conv, 512);
            CHECKF(c == nw && !wmemcmp(conv, w, nw), "id %u content parity", ids[i]);
        }
    }
}

TEST(gap7_loadstringA_unicode_conversion_note) {
    REQUIRE(U(LoadStringA)); REQUIRE(U(LoadStringW)); NEED_MOD(m);
    char a[64]; WCHAR w[64];
    int na = U(LoadStringA)(m, 4, a, 64), nw = U(LoadStringW)(m, 4, w, 64);
    CHECK(nw == 8);
    t_note("LoadStringA of non-ANSI string: n=%d (W=%d), acp=%u", na, nw, GetACP());
    CHECK(na >= 5 && na <= 12);
    CHECK(!strncmp(a, "Uni ", 4));
}

/* ---------- LoadStringByReference ---------- */
UDECL(BOOL, LoadStringByReference, (DWORD, PCWSTR, PCWSTR, PWSTR, ULONG, PCWSTR, PULONG));

TEST(gap7_loadstringbyref_arg_validation) {
    if (!UN(LoadStringByReference) || t_is_wine()) { t_skip("LoadStringByReference unavailable"); return; }
    WCHAR buf[64]; ULONG out = 0;
    const WCHAR *src = L"@%SystemRoot%\\system32\\nonexistent_k32t.dll,-100";
    /* NULL source */
    SET_ERR(0); CHECK(!UN(LoadStringByReference)(0, NULL, NULL, buf, 64, NULL, &out)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* NULL buffer with a non-zero size, or without an out-size */
    SET_ERR(0); CHECK(!UN(LoadStringByReference)(0, NULL, src, NULL, 64, NULL, &out)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!UN(LoadStringByReference)(0, NULL, src, NULL, 0, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* reserved flag bits and the invalid flag combination (low two bits both set) */
    static const DWORD badflags[] = { 0x20, 0x40, 0x80, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, 0x3, 0x7, 0xB, 0x13 };
    for (size_t i = 0; i < sizeof badflags / sizeof badflags[0]; i++) {
        SET_ERR(0);
        CHECKF(!UN(LoadStringByReference)(badflags[i], NULL, src, buf, 64, NULL, &out), "flags %08lx", badflags[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    /* invalid locale name */
    SET_ERR(0);
    CHECK(!UN(LoadStringByReference)(0, L"not a locale!!", src, buf, 64, NULL, &out));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap7_loadstringbyref_bad_source_string) {
    if (!UN(LoadStringByReference) || t_is_wine()) { t_skip("LoadStringByReference unavailable"); return; }
    WCHAR buf[64]; ULONG out = 0;
    static const WCHAR *bad[] = { L"", L"plain text", L"@", L"@nocomma", L"nodll,-1" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        BOOL r = UN(LoadStringByReference)(0, NULL, bad[i], buf, 64, NULL, &out);
        CHECK(!r);
        DWORD e = GetLastError();
        t_note("LoadStringByReference(\"%ls\") -> err %lu", bad[i], e);
        CHECK(e != 0);
    }
}
