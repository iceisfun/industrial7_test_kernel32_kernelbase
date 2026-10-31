#include "mvc.h"
/* CreateFileMapping / OpenFileMapping / MapViewOfFile / UnmapViewOfFile / FlushViewOfFile */

#define CFMA  U(CreateFileMappingA)
#define CFMW  U(CreateFileMappingW)
#define OFMA  U(OpenFileMappingA)
#define OFMW  U(OpenFileMappingW)
#define MVF   U(MapViewOfFile)
#define MVFE  U(MapViewOfFileEx)
#define UMVF  U(UnmapViewOfFile)
#define PAGEFILE INVALID_HANDLE_VALUE

static void mapname(char *out, size_t cap, const char *tag) {
    snprintf(out, cap, "Local\\k32t_mv_%lu_%s", GetCurrentProcessId(), tag);
}
/* Create a file of the given size filled with pattern byte (i*7+3)&0xFF. */
static HANDLE mk_file(const char *leaf, DWORD size, DWORD access) {
    char path[MAX_PATH];
    t_tmpfile(path, sizeof path, leaf);
    HANDLE h = CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return h;
    unsigned char buf[4096];
    DWORD done = 0, w;
    while (done < size) {
        DWORD n = size - done > sizeof buf ? sizeof buf : size - done;
        for (DWORD i = 0; i < n; i++) buf[i] = (unsigned char)(((done + i) * 7 + 3) & 0xFF);
        if (!WriteFile(h, buf, n, &w, NULL)) break;
        done += n;
    }
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    return h;
}
static unsigned char *mapall(HANDLE h) { return h ? (unsigned char *)MVF(h, FILE_MAP_ALL_ACCESS, 0, 0, 0) : NULL; }
static unsigned char patt(DWORD off) { return (unsigned char)((off * 7 + 3) & 0xFF); }

TEST(mv_fm_anon_basic) {
    REQUIRE(CFMW); REQUIRE(MVF); REQUIRE(UMVF); REQUIRE(VQ);
    SET_ERR(0xdead);
    HANDLE h = CFMW(PAGEFILE, NULL, PAGE_READWRITE, 0, 0x10000, NULL);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(GetLastError(), 0);
    unsigned char *v = MVF(h, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CHECK(v != NULL);
    if (v) {
        CHECK(((ULONG_PTR)v & (GR - 1)) == 0);
        MEMORY_BASIC_INFORMATION m = mv_q(v);
        EXPECT_EQ(m.AllocationBase, v); EXPECT_EQ(m.BaseAddress, v);
        EXPECT_EQ(m.Type, MEM_MAPPED); EXPECT_EQ(m.State, MEM_COMMIT);
        EXPECT_EQ(m.Protect, PAGE_READWRITE); EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE);
        EXPECT_EQ(m.RegionSize, 0x10000);
        int nz = 0;
        for (int i = 0; i < 0x10000; i += 17) nz |= v[i];
        CHECK(nz == 0);
        v[0] = 1; v[0xFFFF] = 2;
        SET_ERR(0);
        CHECK(UMVF(v));
        m = mv_q(v);
        EXPECT_EQ(m.State, MEM_FREE);
    }
    CloseHandle(h);
}
TEST(mv_fm_anon_zero_size_invalid) {
    REQUIRE(CFMW);
    SET_ERR(0);
    HANDLE h = CFMW(PAGEFILE, NULL, PAGE_READWRITE, 0, 0, NULL);
    CHECK(h == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    h = CFMW(PAGEFILE, NULL, PAGE_READONLY, 0, 0, L"Local\\k32t_mv_zero_never");
    CHECK(h == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
}
TEST(mv_fm_protect_validation) {
    REQUIRE(CFMW);
    struct { DWORD prot; int ok; } t[] = {
        { PAGE_READONLY, 1 }, { PAGE_READWRITE, 1 }, { PAGE_WRITECOPY, 1 }, { PAGE_EXECUTE_READ, 1 },
        { PAGE_EXECUTE_READWRITE, 1 }, { PAGE_EXECUTE_WRITECOPY, 1 },
        { 0, 0 }, { PAGE_NOACCESS, 0 }, { PAGE_EXECUTE, 0 }, { PAGE_GUARD, 0 }, { PAGE_READONLY | PAGE_GUARD, 0 },
        { PAGE_READWRITE | PAGE_NOCACHE, 0 }, { 6, 0 }, { 3, 0 }, { 0x1000, 0 }, { PAGE_READWRITE | SEC_PROTECTED_IMAGE, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SET_ERR(0);
        HANDLE h = CFMW(PAGEFILE, NULL, t[i].prot, 0, PG, NULL);
        CHECKF((h != NULL) == t[i].ok, "protect 0x%lx -> %p err %lu", t[i].prot, h, GetLastError());
        if (h) CloseHandle(h); else if (!t[i].ok) ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_fm_sec_flags_anonymous) {
    REQUIRE(CFMW);
    struct { DWORD flags; DWORD err; } t[] = {
        { SEC_RESERVE, 0 }, { SEC_RESERVE | SEC_NOCACHE, 0 }, { SEC_RESERVE | SEC_LARGE_PAGES, ERROR_INVALID_PARAMETER },
        { SEC_RESERVE | SEC_WRITECOMBINE, 0 }, { SEC_COMMIT, 0 }, { SEC_COMMIT | SEC_NOCACHE, 0 }, { SEC_COMMIT | SEC_WRITECOMBINE, 0 },
        { SEC_RESERVE | SEC_COMMIT, ERROR_INVALID_PARAMETER }, { SEC_IMAGE | SEC_RESERVE, ERROR_INVALID_PARAMETER },
        { SEC_IMAGE | SEC_COMMIT, ERROR_INVALID_PARAMETER }, { SEC_NOCACHE, ERROR_INVALID_PARAMETER },
        { SEC_LARGE_PAGES, ERROR_INVALID_PARAMETER }, { SEC_WRITECOMBINE, ERROR_INVALID_PARAMETER },
        { SEC_FILE, ERROR_INVALID_PARAMETER }, { SEC_FILE | SEC_RESERVE, ERROR_INVALID_PARAMETER }, { SEC_FILE | SEC_COMMIT, ERROR_INVALID_PARAMETER },
        { 0, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SET_ERR(0);
        HANDLE h = CFMW(PAGEFILE, NULL, PAGE_READWRITE | t[i].flags, 0, GR, NULL);
        if (t[i].err) { CHECKF(h == NULL, "flags 0x%lx", t[i].flags); if (!h) ERR1(t[i].err); }
        else CHECKF(h != NULL, "flags 0x%lx err %lu", t[i].flags, GetLastError());
        if (h) CloseHandle(h);
    }
}
TEST(mv_fm_sec_image_anonymous) {
    REQUIRE(CFMW);
    SET_ERR(0);
    HANDLE h = CFMW(PAGEFILE, NULL, PAGE_READONLY | SEC_IMAGE, 0, GR, NULL);
    CHECK(h == NULL);
    ERR2(ERROR_BAD_EXE_FORMAT, ERROR_INVALID_PARAMETER);
    if (h) CloseHandle(h);
}
TEST(mv_fm_numa_node_validation) {
    REQUIRE(U(CreateFileMappingNumaW));
    int haveA = U(CreateFileMappingNumaA) != NULL;
    if (!haveA) t_note("CreateFileMappingNumaA not exported");
    DWORD nodes[] = { NUMA_NO_PREFERRED_NODE, 0 };
    for (int i = 0; i < 2; i++) {
        HANDLE h = U(CreateFileMappingNumaW)(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, NULL, nodes[i]);
        CHECKF(h != NULL, "node %lu err %lu", nodes[i], GetLastError());
        if (h) CloseHandle(h);
        if (haveA) {
            h = U(CreateFileMappingNumaA)(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, NULL, nodes[i]);
            CHECKF(h != NULL, "A node %lu err %lu", nodes[i], GetLastError());
            if (h) CloseHandle(h);
        }
    }
    DWORD bad[] = { 0x40, 0x41, 100, 0x1000, 0xFFFFFFFE };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        HANDLE h = U(CreateFileMappingNumaW)(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, NULL, bad[i]);
        CHECKF(h == NULL, "node 0x%lx", bad[i]);
        if (h) CloseHandle(h); else ERR1(ERROR_INVALID_PARAMETER);
        if (!haveA) continue;
        SET_ERR(0);
        h = U(CreateFileMappingNumaA)(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, NULL, bad[i]);
        CHECKF(h == NULL, "A node 0x%lx", bad[i]);
        if (h) CloseHandle(h); else ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_fm_mapviewexnuma_node_validation) {
    REQUIRE(U(MapViewOfFileExNuma)); REQUIRE(CFMW);
    HANDLE h = CFMW(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, NULL);
    CHECK(h != NULL);
    if (!h) return;
    void *v = U(MapViewOfFileExNuma)(h, FILE_MAP_WRITE, 0, 0, 0, NULL, NUMA_NO_PREFERRED_NODE);
    CHECK(v != NULL);
    if (v) UMVF(v);
    v = U(MapViewOfFileExNuma)(h, FILE_MAP_WRITE, 0, 0, 0, NULL, 0);
    CHECK(v != NULL);
    if (v) UMVF(v);
    SET_ERR(0);
    v = U(MapViewOfFileExNuma)(h, FILE_MAP_WRITE, 0, 0, 0, NULL, 0x40);
    CHECK(v == NULL);
    if (v) UMVF(v); else ERR1(ERROR_INVALID_PARAMETER);
    CloseHandle(h);
}
TEST(mv_fm_named_already_exists) {
    REQUIRE(CFMA); REQUIRE(CFMW);
    char n[100]; mapname(n, sizeof n, "named1");
    SET_ERR(0xdead);
    HANDLE a = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, n);
    CHECK(a != NULL);
    EXPECT_EQ(GetLastError(), 0);
    SET_ERR(0);
    HANDLE b = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, n);
    CHECK(b != NULL);
    CHECK(b != a);
    ERR1(ERROR_ALREADY_EXISTS);
    /* wide spelling of the same name opens the same object */
    WCHAR wn[100]; MultiByteToWideChar(CP_ACP, 0, n, -1, wn, 100);
    SET_ERR(0);
    HANDLE c = CFMW(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, wn);
    CHECK(c != NULL);
    ERR1(ERROR_ALREADY_EXISTS);
    unsigned char *va = mapall(a);
    unsigned char *vc = mapall(c);
    if (va && vc) { va[5] = 0x42; EXPECT_EQ(vc[5], 0x42); }
    if (va) UMVF(va);
    if (vc) UMVF(vc);
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
    if (c) CloseHandle(c);
}
TEST(mv_fm_named_case_sensitive) {
    REQUIRE(CFMA);
    char n[100], u[100]; mapname(n, sizeof n, "CaseTest"); mapname(u, sizeof u, "CASETEST");
    HANDLE a = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, n);
    SET_ERR(0);
    HANDLE b = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, u);
    CHECK(a && b);
    EXPECT_EQ(GetLastError(), 0);            /* different name -> new object */
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
}
TEST(mv_fm_named_different_size_keeps_original) {
    REQUIRE(CFMA); REQUIRE(VQ);
    char n[100]; mapname(n, sizeof n, "size1");
    HANDLE a = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, n);
    SET_ERR(0);
    HANDLE b = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, 4 * GR, n);
    CHECK(a && b);
    ERR1(ERROR_ALREADY_EXISTS);
    unsigned char *v = b ? (unsigned char *)MVF(b, FILE_MAP_ALL_ACCESS, 0, 0, 0) : NULL;
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(mv_q(v).RegionSize, GR); UMVF(v); }
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
}
TEST(mv_fm_open_basic) {
    REQUIRE(CFMA); REQUIRE(OFMA); REQUIRE(OFMW);
    char n[100]; mapname(n, sizeof n, "open1");
    HANDLE a = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, n);
    CHECK(a != NULL);
    SET_ERR(0);
    HANDLE b = OFMA(FILE_MAP_ALL_ACCESS, FALSE, n);
    CHECK(b != NULL);
    EXPECT_EQ(GetLastError(), 0);
    WCHAR wn[100]; MultiByteToWideChar(CP_ACP, 0, n, -1, wn, 100);
    HANDLE c = OFMW(FILE_MAP_READ, FALSE, wn);
    CHECK(c != NULL);
    unsigned char *va = mapall(a);
    unsigned char *vc = c ? (unsigned char *)MVF(c, FILE_MAP_READ, 0, 0, 0) : NULL;
    CHECK(va && vc);
    if (va && vc) { va[100] = 0x99; EXPECT_EQ(vc[100], 0x99); EXPECT_EQ(mv_wr(vc, 1), EXCEPTION_ACCESS_VIOLATION); }
    if (va) UMVF(va);
    if (vc) UMVF(vc);
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
    if (c) CloseHandle(c);
}
TEST(mv_fm_open_errors) {
    REQUIRE(OFMA); REQUIRE(OFMW); REQUIRE(CFMA);
    char n[100]; mapname(n, sizeof n, "nonexistent");
    SET_ERR(0);
    CHECK(OFMA(FILE_MAP_READ, FALSE, n) == NULL);
    ERR1(ERROR_FILE_NOT_FOUND);
    SET_ERR(0);
    CHECK(OFMW(FILE_MAP_READ, FALSE, L"Local\\k32t_mv_nonexistent_w") == NULL);
    ERR1(ERROR_FILE_NOT_FOUND);
    SET_ERR(0);
    CHECK(OFMA(FILE_MAP_READ, FALSE, NULL) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(OFMW(FILE_MAP_READ, FALSE, NULL) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    HANDLE h = OFMA(FILE_MAP_READ, FALSE, "");
    CHECK(h == NULL);
    t_note("OpenFileMappingA(\"\") err=%lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(mv_fm_name_type_mismatch) {
    REQUIRE(OFMA); REQUIRE(CFMA);
    char n[100]; mapname(n, sizeof n, "evt1");
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, n);
    CHECK(ev != NULL);
    SET_ERR(0);
    CHECK(OFMA(FILE_MAP_READ, FALSE, n) == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, n) == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(mv_fm_name_released_after_last_close) {
    REQUIRE(OFMA); REQUIRE(CFMA);
    char n[100]; mapname(n, sizeof n, "life1");
    HANDLE a = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, n);
    unsigned char *v = mapall(a);
    CloseHandle(a);
    /* the name goes away with the last handle even though a view keeps the object alive */
    if (v) { v[0] = 7; CHECK(v[0] == 7); }
    SET_ERR(0);
    HANDLE b = OFMA(FILE_MAP_ALL_ACCESS, FALSE, n);
    if (v) { CHECK(v[0] == 7); UMVF(v); }
    CHECK(b == NULL);
    ERR1(ERROR_FILE_NOT_FOUND);
    if (b) CloseHandle(b);
}
TEST(mv_fm_view_survives_handle_close) {
    REQUIRE(CFMA);
    HANDLE a = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, 2 * PG, NULL);
    unsigned char *v = mapall(a);
    CHECK(v != NULL);
    CloseHandle(a);
    if (v) {
        EXPECT_EQ(mv_wr(v + PG, 0x55), 0);
        EXPECT_EQ(v[PG], 0x55);
        CHECK(UMVF(v));
    }
}
TEST(mv_fm_invalid_handle_to_mapview) {
    REQUIRE(MVF);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x7770, ev };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        void *v = MVF(bad[i], FILE_MAP_READ, 0, 0, 0);
        CHECKF(v == NULL, "handle %p", bad[i]);
        if (v) UMVF(v); else ERR1(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
    HANDLE a = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, PG, NULL);
    CloseHandle(a);
    SET_ERR(0);
    CHECK(MVF(a, FILE_MAP_READ, 0, 0, 0) == NULL);
    ERR1(ERROR_INVALID_HANDLE);
}
TEST(mv_fm_hfile_invalid_types) {
    REQUIRE(CFMA);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    HANDLE h = CFMA(ev, NULL, PAGE_READWRITE, 0, PG, NULL);
    CHECK(h == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    if (h) CloseHandle(h);
    CloseHandle(ev);
    HANDLE f = mk_file("closed.bin", 100, GENERIC_READ | GENERIC_WRITE);
    CloseHandle(f);
    SET_ERR(0);
    h = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    CHECK(h == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    if (h) CloseHandle(h);
}

/* ---------------- file-backed ---------------- */
TEST(mv_fm_file_size_zero_maps_whole_file) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE f = mk_file("whole.bin", 10000, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    CHECK(m != NULL);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION q = mv_q(v);
        EXPECT_EQ(q.RegionSize, 12288);          /* file size rounded up to a page */
        EXPECT_EQ(q.Type, MEM_MAPPED);
        for (DWORD i = 0; i < 10000; i += 13) if (v[i] != patt(i)) { CHECKF(0, "byte %lu", i); break; }
        EXPECT_EQ(v[10000], 0);                  /* tail of last page is zero */
        UMVF(v);
    }
    CloseHandle(m); CloseHandle(f);
}
TEST(mv_fm_file_zero_length_size_zero) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("empty.bin", 0, GENERIC_READ | GENERIC_WRITE);
    SET_ERR(0);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    CHECK(m == NULL);
    ERR1(ERROR_FILE_INVALID);
    if (m) CloseHandle(m);
    SET_ERR(0);
    m = CFMA(f, NULL, PAGE_READONLY, 0, 0, NULL);
    CHECK(m == NULL);
    ERR1(ERROR_FILE_INVALID);
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_file_extends_with_size) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("grow.bin", 100, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 20000, NULL);
    CHECK(m != NULL);
    EXPECT_EQ(GetFileSize(f, NULL), 20000);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(v[0], patt(0)); EXPECT_EQ(v[99], patt(99)); EXPECT_EQ(v[100], 0); EXPECT_EQ(v[19999], 0); v[19999] = 5; UMVF(v); }
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_file_empty_extends_with_size) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("empty2.bin", 0, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 5000, NULL);
    CHECK(m != NULL);
    EXPECT_EQ(GetFileSize(f, NULL), 5000);
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_file_readonly_larger_than_file) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("small.bin", 100, GENERIC_READ | GENERIC_WRITE);
    CloseHandle(f);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "small.bin");
    f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    HANDLE m = CFMA(f, NULL, PAGE_READONLY, 0, 0x4000, NULL);
    CHECK(m == NULL);
    ERR1(ERROR_NOT_ENOUGH_MEMORY);
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_file_access_rights) {
    REQUIRE(CFMA);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "rights.bin");
    HANDLE f = mk_file("rights.bin", 4096, GENERIC_READ | GENERIC_WRITE);
    CloseHandle(f);
    f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    CHECK(m == NULL);
    ERR1(ERROR_ACCESS_DENIED);
    if (m) CloseHandle(m);
    SET_ERR(0);
    m = CFMA(f, NULL, PAGE_WRITECOPY, 0, 0, NULL);
    CHECKF(m != NULL, "WRITECOPY on read-only handle err %lu", GetLastError());   /* copy-on-write needs only read access */
    if (m) CloseHandle(m);
    m = CFMA(f, NULL, PAGE_READONLY, 0, 0, NULL);
    CHECK(m != NULL);
    if (m) CloseHandle(m);
    CloseHandle(f);
    f = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    CHECK(m == NULL);
    ERR1(ERROR_ACCESS_DENIED);
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_file_write_through_view_visible_in_file) {
    REQUIRE(CFMA); REQUIRE(U(FlushViewOfFile));
    HANDLE f = mk_file("wt.bin", 8192, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) {
        memset(v + 100, 0xEE, 50);
        SET_ERR(0);
        CHECK(U(FlushViewOfFile)(v, 0));
        CHECK(U(FlushViewOfFile)(v + 100, 50));
        CHECK(U(FlushViewOfFile)(v, 8192));
        unsigned char buf[50]; DWORD got = 0;
        SetFilePointer(f, 100, NULL, FILE_BEGIN);
        CHECK(ReadFile(f, buf, 50, &got, NULL) && got == 50);
        int ok = 1; for (int i = 0; i < 50; i++) ok &= buf[i] == 0xEE;
        CHECK(ok);
        UMVF(v);
    }
    CloseHandle(m); CloseHandle(f);
}
TEST(mv_fm_file_writefile_visible_in_view) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("coh.bin", 4096, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) {
        DWORD w; unsigned char b = 0x6B;
        SetFilePointer(f, 77, NULL, FILE_BEGIN);
        CHECK(WriteFile(f, &b, 1, &w, NULL));
        EXPECT_EQ(v[77], 0x6B);                   /* cache is coherent with mapped views */
        UMVF(v);
    }
    CloseHandle(m); CloseHandle(f);
}
TEST(mv_fm_file_handle_closed_before_map) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("closedf.bin", 4096, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    CloseHandle(f);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(v[5], patt(5)); UMVF(v); }
    CloseHandle(m);
}
TEST(mv_fm_file_truncate_while_mapped) {
    REQUIRE(CFMA);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "trunc.bin");
    HANDLE f = mk_file("trunc.bin", 16384, GENERIC_READ | GENERIC_WRITE | DELETE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) {
        SetFilePointer(f, 100, NULL, FILE_BEGIN);
        SET_ERR(0);
        CHECK(!SetEndOfFile(f));
        ERR1(ERROR_USER_MAPPED_FILE);
        EXPECT_EQ(GetFileSize(f, NULL), 16384);
        /* extending is fine */
        SetFilePointer(f, 20000, NULL, FILE_BEGIN);
        CHECK(SetEndOfFile(f));
        UMVF(v);
    }
    CloseHandle(m);
    SetFilePointer(f, 100, NULL, FILE_BEGIN);
    CHECK(SetEndOfFile(f));                       /* ok once the section is gone */
    CloseHandle(f);
}
TEST(mv_fm_file_delete_while_mapped) {
    REQUIRE(CFMA);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "delmap.bin");
    HANDLE f = mk_file("delmap.bin", 4096, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    unsigned char *v = mapall(m);
    CloseHandle(f);
    SET_ERR(0);
    BOOL r = DeleteFileA(p);
    CHECK(!r);
    ERR2(ERROR_ACCESS_DENIED, ERROR_SHARING_VIOLATION);
    if (v) UMVF(v);
    CloseHandle(m);
    CHECK(DeleteFileA(p));
}
TEST(mv_fm_file_open_exclusive_while_mapped) {
    REQUIRE(CFMA);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "excl.bin");
    HANDLE f = mk_file("excl.bin", 4096, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    unsigned char *v = mapall(m);
    CloseHandle(f);
    /* opening for write with FILE_SHARE_WRITE is allowed, but truncating with CREATE_ALWAYS is not */
    SET_ERR(0);
    HANDLE g = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(g == INVALID_HANDLE_VALUE);
    ERR1(ERROR_USER_MAPPED_FILE);
    if (g != INVALID_HANDLE_VALUE) CloseHandle(g);
    if (v) UMVF(v);
    CloseHandle(m);
}
TEST(mv_fm_file_pagewritecopy_does_not_modify_file) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("cow.bin", 8192, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_WRITECOPY, 0, 0, NULL);
    CHECK(m != NULL);
    unsigned char *v = m ? (unsigned char *)MVF(m, FILE_MAP_COPY, 0, 0, 0) : NULL;
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION q = mv_q(v);
        EXPECT_EQ(q.Protect, PAGE_WRITECOPY);
        EXPECT_EQ(q.AllocationProtect, PAGE_WRITECOPY);
        EXPECT_EQ(q.Type, MEM_MAPPED);
        EXPECT_EQ(v[10], patt(10));
        v[10] = 0xA5;
        EXPECT_EQ(v[10], 0xA5);
        q = mv_q(v);
        EXPECT_EQ(q.Protect, PAGE_READWRITE);     /* touched page became private copy */
        EXPECT_EQ(q.RegionSize, PG);
        q = mv_q(v + PG);
        EXPECT_EQ(q.Protect, PAGE_WRITECOPY);
        unsigned char b = 0; DWORD got;
        SetFilePointer(f, 10, NULL, FILE_BEGIN);
        CHECK(ReadFile(f, &b, 1, &got, NULL));
        EXPECT_EQ(b, patt(10));
        UMVF(v);
    }
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_copy_view_of_rw_mapping_isolated) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    unsigned char *w = mapall(m);
    unsigned char *c = (unsigned char *)MVF(m, FILE_MAP_COPY, 0, 0, 0);
    CHECK(w && c);
    if (w && c) {
        w[0] = 1;
        EXPECT_EQ(c[0], 1);                       /* not yet written: still shares */
        c[0] = 2;
        EXPECT_EQ(w[0], 1);
        w[0] = 3;
        EXPECT_EQ(c[0], 2);                       /* private copy persists */
        w[PG] = 9;
        EXPECT_EQ(c[PG], 9);                      /* other pages still shared */
    }
    if (w) UMVF(w);
    if (c) UMVF(c);
    CloseHandle(m);
}
TEST(mv_fm_two_views_coherent) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    unsigned char *a = mapall(m), *b = mapall(m);
    CHECK(a && b && a != b);
    if (a && b) {
        for (int i = 0; i < 1000; i++) { a[i * 8] = (unsigned char)i; }
        int ok = 1; for (int i = 0; i < 1000; i++) ok &= b[i * 8] == (unsigned char)i;
        CHECK(ok);
        b[0xFFFF] = 0x77; EXPECT_EQ(a[0xFFFF], 0x77);
    }
    if (a) UMVF(a);
    if (b) UMVF(b);
    CloseHandle(m);
}
TEST(mv_fm_two_mappings_same_file_coherent) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("two.bin", 4096, GENERIC_READ | GENERIC_WRITE);
    HANDLE m1 = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL), m2 = CFMA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    unsigned char *a = mapall(m1), *b = mapall(m2);
    CHECK(a && b);
    if (a && b) { a[1] = 0x31; EXPECT_EQ(b[1], 0x31); }
    if (a) UMVF(a);
    if (b) UMVF(b);
    CloseHandle(m1); CloseHandle(m2); CloseHandle(f);
}

/* ---------------- views: access matrix, offsets, addresses ---------------- */
static const char *accname(DWORD a) {
    switch (a) { case FILE_MAP_READ: return "READ"; case FILE_MAP_WRITE: return "WRITE"; case FILE_MAP_COPY: return "COPY"; case FILE_MAP_ALL_ACCESS: return "ALL"; default: return "?"; }
}
/* expected view protection for (mapping protect, view access); 0 = access denied */
TEST(mv_fm_access_matrix_rw_mapping) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    struct { DWORD acc; DWORD prot; } t[] = {
        { FILE_MAP_READ, PAGE_READONLY }, { FILE_MAP_WRITE, PAGE_READWRITE }, { FILE_MAP_COPY, PAGE_WRITECOPY },
        { FILE_MAP_ALL_ACCESS, PAGE_READWRITE }, { FILE_MAP_READ | FILE_MAP_WRITE, PAGE_READWRITE },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SET_ERR(0);
        void *v = MVF(m, t[i].acc, 0, 0, 0);
        CHECKF(v != NULL, "access 0x%lx err %lu", t[i].acc, GetLastError());
        if (!v) continue;
        MEMORY_BASIC_INFORMATION q = mv_q(v);
        CHECKF(q.Protect == t[i].prot, "access 0x%lx prot 0x%lx want 0x%lx", t[i].acc, q.Protect, t[i].prot);
        UMVF(v);
    }
    /* execute needs an executable section */
    SET_ERR(0);
    void *v = MVF(m, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, 0);
    CHECK(v == NULL);
    ERR1(ERROR_ACCESS_DENIED);
    if (v) UMVF(v);
    CloseHandle(m);
}
TEST(mv_fm_access_matrix_readonly_mapping) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READONLY, 0, GR, NULL);
    CHECK(m != NULL);
    if (!m) return;
    void *v = MVF(m, FILE_MAP_READ, 0, 0, 0);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(mv_q(v).Protect, PAGE_READONLY); UMVF(v); }
    DWORD denied[] = { FILE_MAP_WRITE, FILE_MAP_COPY, FILE_MAP_ALL_ACCESS, FILE_MAP_READ | FILE_MAP_EXECUTE };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        v = MVF(m, denied[i], 0, 0, 0);
        CHECKF(v == NULL, "access 0x%lx", denied[i]);
        if (v) UMVF(v); else ERR1(ERROR_ACCESS_DENIED);
    }
    CloseHandle(m);
}
TEST(mv_fm_access_matrix_writecopy_mapping) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_WRITECOPY, 0, GR, NULL);
    CHECK(m != NULL);
    if (!m) return;
    void *v = MVF(m, FILE_MAP_COPY, 0, 0, 0);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(mv_q(v).Protect, PAGE_WRITECOPY); UMVF(v); }
    v = MVF(m, FILE_MAP_READ, 0, 0, 0);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(mv_q(v).Protect, PAGE_READONLY); UMVF(v); }
    SET_ERR(0);
    v = MVF(m, FILE_MAP_WRITE, 0, 0, 0);
    t_note("FILE_MAP_WRITE on PAGE_WRITECOPY mapping: %p err=%lu prot=0x%lx", v, GetLastError(), v ? mv_q(v).Protect : 0);
    if (v) UMVF(v);
    CloseHandle(m);
}
TEST(mv_fm_access_matrix_execute_mapping) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_EXECUTE_READWRITE, 0, GR, NULL);
    CHECK(m != NULL);
    if (!m) return;
    struct { DWORD acc; DWORD prot; } t[] = {
        { FILE_MAP_READ | FILE_MAP_EXECUTE, PAGE_EXECUTE_READ }, { FILE_MAP_WRITE | FILE_MAP_EXECUTE, PAGE_EXECUTE_READWRITE },
        { FILE_MAP_COPY | FILE_MAP_EXECUTE, PAGE_EXECUTE_WRITECOPY }, { FILE_MAP_READ, PAGE_READONLY }, { FILE_MAP_WRITE, PAGE_READWRITE },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SET_ERR(0);
        void *v = MVF(m, t[i].acc, 0, 0, 0);
        CHECKF(v != NULL, "access 0x%lx err %lu", t[i].acc, GetLastError());
        if (!v) continue;
        CHECKF(mv_q(v).Protect == t[i].prot, "access 0x%lx prot 0x%lx want 0x%lx", t[i].acc, mv_q(v).Protect, t[i].prot);
        UMVF(v);
    }
    /* run code out of an execute view */
    unsigned char *w = (unsigned char *)MVF(m, FILE_MAP_WRITE | FILE_MAP_EXECUTE, 0, 0, 0);
    CHECK(w != NULL);
    if (w) {
        memcpy(w, mv_stub, sizeof mv_stub);
        int r = 0;
        EXPECT_EQ(mv_exec(w, &r), 0);
        EXPECT_EQ(r, 0x1234);
        UMVF(w);
    }
    CloseHandle(m);
}
TEST(mv_fm_view_access_zero_is_noaccess) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    SET_ERR(0);
    void *v = MVF(m, 0, 0, 0, 0);
    t_note("MapViewOfFile(access=0): %p err=%lu prot=0x%lx", v, GetLastError(), v ? mv_q(v).Protect : 0);
    if (v) { EXPECT_EQ(mv_q(v).Protect, PAGE_NOACCESS); UMVF(v); }
    CloseHandle(m);
}
TEST(mv_fm_view_size_rounding) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    struct { SIZE_T req, region; } t[] = { {1, PG}, {100, PG}, {PG, PG}, {PG + 1, 2 * PG}, {GR - 1, GR}, {GR, GR} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        void *v = MVF(m, FILE_MAP_WRITE, 0, 0, t[i].req);
        CHECKF(v != NULL, "bytes %Iu", t[i].req);
        if (!v) continue;
        CHECKF(mv_q(v).RegionSize == t[i].region, "bytes %Iu region 0x%Ix", t[i].req, mv_q(v).RegionSize);
        UMVF(v);
    }
    CloseHandle(m);
}
TEST(mv_fm_view_bytes_beyond_mapping) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    SIZE_T sizes[] = { GR + 1, 2 * GR, (SIZE_T)1 << 40, (SIZE_T)-1 };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        void *v = MVF(m, FILE_MAP_WRITE, 0, 0, sizes[i]);
        CHECKF(v == NULL, "bytes 0x%Ix", sizes[i]);
        if (v) UMVF(v); else ERR3(ERROR_ACCESS_DENIED, ERROR_INVALID_PARAMETER, ERROR_NOT_ENOUGH_MEMORY);
    }
    CloseHandle(m);
}
TEST(mv_fm_view_offset_alignment) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, 4 * GR, NULL);
    DWORD bad[] = { 1, 0x1000, 0x8000, 0xFFFF, 0x10001, 0x18000 };
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        void *v = MVF(m, FILE_MAP_WRITE, 0, bad[i], PG);
        CHECKF(v == NULL, "offset 0x%lx", bad[i]);
        if (v) UMVF(v); else ERR2(ERROR_MAPPED_ALIGNMENT, ERROR_INVALID_PARAMETER);
    }
    DWORD good[] = { 0, GR, 2 * GR, 3 * GR };
    for (int i = 0; i < 4; i++) {
        void *v = MVF(m, FILE_MAP_WRITE, 0, good[i], PG);
        CHECKF(v != NULL, "offset 0x%lx err %lu", good[i], GetLastError());
        if (v) UMVF(v);
    }
    CloseHandle(m);
}
TEST(mv_fm_view_offset_out_of_range) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, 2 * GR, NULL);
    struct { DWORD hi, lo; } offs[] = { {0, 2 * GR}, {0, 4 * GR}, {1, 0}, {0x7FFFFFFF, 0}, {0xFFFFFFFF, 0xFFFF0000} };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        void *v = MVF(m, FILE_MAP_WRITE, offs[i].hi, offs[i].lo, 0);
        CHECKF(v == NULL, "offset %lx:%lx", offs[i].hi, offs[i].lo);
        if (v) UMVF(v); else ERR3(ERROR_ACCESS_DENIED, ERROR_INVALID_PARAMETER, ERROR_MAPPED_ALIGNMENT);
        if (!v) t_note("offset %lx:%lx -> err %lu", offs[i].hi, offs[i].lo, GetLastError());
    }
    CloseHandle(m);
}
TEST(mv_fm_view_offset_data) {
    REQUIRE(CFMA);
    DWORD sz = 3 * GR + 1000;
    HANDLE f = mk_file("offs.bin", sz, GENERIC_READ | GENERIC_WRITE);
    HANDLE m = CFMA(f, NULL, PAGE_READONLY, 0, 0, NULL);
    CHECK(m != NULL);
    if (m) {
        for (int k = 0; k < 4; k++) {
            DWORD off = k * GR;
            unsigned char *v = (unsigned char *)MVF(m, FILE_MAP_READ, 0, off, 0);
            CHECKF(v != NULL, "off %lu err %lu", off, GetLastError());
            if (!v) continue;
            CHECK(v[0] == patt(off));
            CHECK(v[500] == patt(off + 500));
            MEMORY_BASIC_INFORMATION q = mv_q(v);
            DWORD remain = sz - off;
            EXPECT_EQ(q.RegionSize, (remain + PG - 1) & ~(PG - 1));
            UMVF(v);
        }
    }
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_view_fixed_address) {
    REQUIRE(CFMA); REQUIRE(MVFE);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *h = mv_hole(4 * GR);
    CHECK(h != NULL);
    if (h) {
        SET_ERR(0);
        void *v = MVFE(m, FILE_MAP_WRITE, 0, 0, 0, h);
        if (!v) t_note("hole reuse raced err=%lu", GetLastError());
        else { EXPECT_EQ(v, h); UMVF(v); }
        /* unaligned base address */
        SET_ERR(0);
        v = MVFE(m, FILE_MAP_WRITE, 0, 0, 0, (char *)h + PG);
        CHECK(v == NULL);
        if (v) UMVF(v); else ERR2(ERROR_MAPPED_ALIGNMENT, ERROR_INVALID_PARAMETER);
    }
    /* occupied address */
    char *p = mv_alloc(PG);
    SET_ERR(0);
    void *v = MVFE(m, FILE_MAP_WRITE, 0, 0, 0, p);
    CHECK(v == NULL);
    if (v) UMVF(v); else ERR1(ERROR_INVALID_ADDRESS);
    mv_free(p);
    CloseHandle(m);
}
TEST(mv_fm_view_fixed_overlap_existing_view) {
    REQUIRE(CFMA); REQUIRE(MVFE);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    unsigned char *a = mapall(m);
    CHECK(a != NULL);
    SET_ERR(0);
    void *v = MVFE(m, FILE_MAP_WRITE, 0, 0, 0, a);
    CHECK(v == NULL);
    ERR1(ERROR_INVALID_ADDRESS);
    if (v) UMVF(v);
    if (a) UMVF(a);
    CloseHandle(m);
}
TEST(mv_fm_view_fixed_in_reserved_region_fails) {
    REQUIRE(CFMA); REQUIRE(MVFE);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    char *r = VALLOC(NULL, 2 * GR, MEM_RESERVE, PAGE_READWRITE);
    SET_ERR(0);
    void *v = MVFE(m, FILE_MAP_WRITE, 0, 0, 0, r);
    CHECK(v == NULL);
    if (v) UMVF(v); else ERR1(ERROR_INVALID_ADDRESS);
    mv_free(r);
    CloseHandle(m);
}

/* ---------------- unmap ---------------- */
TEST(mv_unmap_errors) {
    REQUIRE(UMVF); REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *v = mapall(m);
    CHECK(v != NULL);
    SET_ERR(0);
    CHECK(UMVF(v));
    SET_ERR(0);
    CHECK(!UMVF(v));
    ERR1(ERROR_INVALID_ADDRESS);
    SET_ERR(0);
    CHECK(!UMVF((void *)0xdeadbeef));
    ERR1(ERROR_INVALID_ADDRESS);
    char *p = mv_alloc(PG);
    SET_ERR(0);
    CHECK(!UMVF(p));
    ERR1(ERROR_INVALID_ADDRESS);
    mv_free(p);
    SET_ERR(0);
    CHECK(!UMVF(NULL));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
    CloseHandle(m);
}
TEST(mv_unmap_image_address_fails) {
    REQUIRE(UMVF);
    /* do NOT unmap our own image; only a non-base address inside an image should be refused */
    SET_ERR(0);
    CHECK(!UMVF((void *)0x10));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
}
TEST(mv_unmap_middle_of_view) {
    REQUIRE(UMVF); REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    char *v = (char *)mapall(m);
    CHECK(v != NULL);
    SET_ERR(0);
    BOOL r = UMVF(v + 3 * PG);
    t_note("UnmapViewOfFile(view+0x3000): r=%d err=%lu", r, GetLastError());
    if (r) EXPECT_EQ(mv_q(v).State, MEM_FREE); else CHECK(UMVF(v));
    CloseHandle(m);
}
TEST(mv_unmap_vfree_release_view) {
    REQUIRE(CFMA); REQUIRE(VFREE);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *v = mapall(m);
    SET_ERR(0);
    BOOL r = VFREE(v, 0, MEM_RELEASE);
    t_note("VirtualFree(MEM_RELEASE) on a mapped view: r=%d err=%lu", r, GetLastError());
    if (!r) UMVF(v);
    SET_ERR(0);
    CHECK(!VFREE(v, 0, MEM_RELEASE) || 1);
    CloseHandle(m);
}
TEST(mv_unmap_vfree_decommit_view_fails) {
    REQUIRE(CFMA); REQUIRE(VFREE);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *v = mapall(m);
    SET_ERR(0);
    CHECK(!VFREE(v, GR, MEM_DECOMMIT));
    ERR1(ERROR_INVALID_PARAMETER);
    UMVF(v);
    CloseHandle(m);
}
TEST(mv_unmap_ex_and_2) {
    REQUIRE(U(UnmapViewOfFileEx)); REQUIRE(U(UnmapViewOfFile2)); REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *v = mapall(m);
    SET_ERR(0);
    CHECK(U(UnmapViewOfFileEx)(v, 0));
    SET_ERR(0);
    CHECK(!U(UnmapViewOfFileEx)(v, 0));
    ERR1(ERROR_INVALID_ADDRESS);
    v = mapall(m);
    SET_ERR(0);
    CHECK(U(UnmapViewOfFile2)(GetCurrentProcess(), v, 0));
    SET_ERR(0);
    CHECK(!U(UnmapViewOfFile2)(GetCurrentProcess(), v, 0));
    ERR1(ERROR_INVALID_ADDRESS);
    v = mapall(m);
    DWORD badflags[] = { 4, 8, 0x100, 0x1000, 0x80000000, 0xFFFFFFFC };
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        CHECKF(!U(UnmapViewOfFile2)(GetCurrentProcess(), v, badflags[i]), "flags 0x%lx", badflags[i]);
        ERR1(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        CHECKF(!U(UnmapViewOfFileEx)(v, badflags[i]), "Ex flags 0x%lx", badflags[i]);
        ERR1(ERROR_INVALID_PARAMETER);
    }
    /* flags valid for placeholders on an ordinary view */
    SET_ERR(0);
    BOOL r = U(UnmapViewOfFile2)(GetCurrentProcess(), v, MEM_PRESERVE_PLACEHOLDER);
    t_note("UnmapViewOfFile2(MEM_PRESERVE_PLACEHOLDER) on plain view: r=%d err=%lu", r, GetLastError());
    if (!r) UMVF(v);
    CloseHandle(m);
}
TEST(mv_unmap_2_bad_process) {
    REQUIRE(U(UnmapViewOfFile2)); REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *v = mapall(m);
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7771 };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECKF(!U(UnmapViewOfFile2)(bad[i], v, 0), "h=%p", bad[i]);
        ERR1(ERROR_INVALID_HANDLE);
    }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(UnmapViewOfFile2)(ev, v, 0));
    ERR1(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    UMVF(v);
    CloseHandle(m);
}
TEST(mv_flush_view) {
    REQUIRE(U(FlushViewOfFile)); REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *v = mapall(m);
    SET_ERR(0);
    CHECK(U(FlushViewOfFile)(v, 0));
    CHECK(U(FlushViewOfFile)(v, GR));
    CHECK(U(FlushViewOfFile)((char *)v + 100, 1));
    SET_ERR(0);
    BOOL r = U(FlushViewOfFile)(NULL, 0);
    t_note("FlushViewOfFile(NULL,0): %d err=%lu", r, GetLastError());
    char *p = mv_alloc(PG);
    SET_ERR(0);
    r = U(FlushViewOfFile)(p, PG);
    t_note("FlushViewOfFile(private memory): %d err=%lu", r, GetLastError());
    mv_free(p);
    UMVF(v);
    SET_ERR(0);
    r = U(FlushViewOfFile)(v, 0);
    CHECK(!r);
    t_note("FlushViewOfFile(unmapped): err=%lu", GetLastError());
    CloseHandle(m);
}

/* ---------------- SEC_RESERVE / SEC_IMAGE ---------------- */
TEST(mv_fm_sec_reserve_commit_view) {
    REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE | SEC_RESERVE, 0, 4 * GR, NULL);
    CHECK(m != NULL);
    if (!m) return;
    char *v = (char *)mapall(m);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION q = mv_q(v);
        t_note("SEC_RESERVE view: State=0x%lx Protect=0x%lx Region=0x%Ix", q.State, q.Protect, q.RegionSize);
        EXPECT_EQ(q.Type, MEM_MAPPED);
        DWORD rc = mv_wr(v, 1);
        t_note("write to uncommitted SEC_RESERVE page: 0x%lx", rc);
        SET_ERR(0);
        void *c = VALLOC(v, 2 * PG, MEM_COMMIT, PAGE_READWRITE);
        EXPECT_EQ(c, v);
        EXPECT_EQ(mv_wr(v, 0x21), 0);
        EXPECT_EQ(mv_wr(v + PG, 0x22), 0);
        q = mv_q(v);
        EXPECT_EQ(q.State, MEM_COMMIT); EXPECT_EQ(q.Type, MEM_MAPPED);
        UMVF(v);
    }
    CloseHandle(m);
}
TEST(mv_fm_sec_image_exe_file) {
    REQUIRE(CFMA); REQUIRE(VQ);
    char sys[MAX_PATH], path[MAX_PATH];
    GetSystemDirectoryA(sys, sizeof sys);
    snprintf(path, sizeof path, "%s\\kernel32.dll", sys);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) return;
    HANDLE m = CFMA(f, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    CHECKF(m != NULL, "err %lu", GetLastError());
    if (m) {
        char *v = (char *)MVF(m, FILE_MAP_READ, 0, 0, 0);
        CHECK(v != NULL);
        if (v) {
            CHECK(v[0] == 'M' && v[1] == 'Z');
            MEMORY_BASIC_INFORMATION q = mv_q(v);
            EXPECT_EQ(q.Type, MEM_IMAGE); EXPECT_EQ(q.AllocationBase, v); EXPECT_EQ(q.State, MEM_COMMIT);
            EXPECT_EQ(q.Protect, PAGE_READONLY);
            IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)v;
            IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(v + d->e_lfanew);
            EXPECT_EQ(nt->Signature, IMAGE_NT_SIGNATURE);
            IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
            q = mv_q(v + sec[0].VirtualAddress);
            EXPECT_EQ(q.Type, MEM_IMAGE);
            CHECK(q.Protect == PAGE_EXECUTE_READ || q.Protect == PAGE_READONLY);
            UMVF(v);
        }
        CloseHandle(m);
    }
    CloseHandle(f);
}
TEST(mv_fm_sec_image_non_pe) {
    REQUIRE(CFMA);
    HANDLE f = mk_file("notpe.bin", 8192, GENERIC_READ | GENERIC_WRITE);
    SET_ERR(0);
    HANDLE m = CFMA(f, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    CHECK(m == NULL);
    ERR1(ERROR_BAD_EXE_FORMAT);
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(mv_fm_sec_file_flag_on_file) {
    REQUIRE(CFMA);
    struct { DWORD flags; DWORD err; } t[] = {
        { SEC_RESERVE, 0 }, { SEC_COMMIT, 0 }, { SEC_RESERVE | SEC_COMMIT, ERROR_INVALID_PARAMETER },
        { SEC_NOCACHE, ERROR_INVALID_PARAMETER }, { SEC_LARGE_PAGES, ERROR_INVALID_PARAMETER },
        { SEC_FILE, ERROR_INVALID_PARAMETER }, { 0, 0 },
        { SEC_RESERVE | SEC_NOCACHE, 0 }, { SEC_COMMIT | SEC_NOCACHE, 0 },
    };
    HANDLE f = mk_file("secflags.bin", 8192, GENERIC_READ | GENERIC_WRITE);
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SET_ERR(0);
        HANDLE m = CFMA(f, NULL, PAGE_READWRITE | t[i].flags, 0, 0, NULL);
        if (t[i].err) { CHECKF(m == NULL, "flags 0x%lx", t[i].flags); if (!m) ERR1(t[i].err); }
        else CHECKF(m != NULL, "flags 0x%lx err %lu", t[i].flags, GetLastError());
        if (m) CloseHandle(m);
    }
    CloseHandle(f);
}

/* ---------------- Win10 additions ---------------- */
TEST(mv_fm_from_app_basic) {
    REQUIRE(U(CreateFileMappingFromApp)); REQUIRE(U(MapViewOfFileFromApp)); REQUIRE(VQ);
    SET_ERR(0);
    HANDLE m = U(CreateFileMappingFromApp)(PAGEFILE, NULL, PAGE_READWRITE, GR, NULL);
    CHECK(m != NULL);
    if (!m) return;
    void *v = U(MapViewOfFileFromApp)(m, FILE_MAP_WRITE, 0, 0);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(mv_q(v).RegionSize, GR); EXPECT_EQ(mv_wr(v, 5), 0); UMVF(v); }
    v = U(MapViewOfFileFromApp)(m, FILE_MAP_READ, GR, 0);
    CHECK(v == NULL);
    if (v) UMVF(v);
    SET_ERR(0);
    v = U(MapViewOfFileFromApp)(m, FILE_MAP_READ, 1, 0);
    CHECK(v == NULL);
    CloseHandle(m);
}
TEST(mv_fm_from_app_size_zero) {
    REQUIRE(U(CreateFileMappingFromApp));
    SET_ERR(0);
    HANDLE m = U(CreateFileMappingFromApp)(PAGEFILE, NULL, PAGE_READWRITE, 0, NULL);
    CHECK(m == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
}
TEST(mv_fm_from_app_execute_needs_capability) {
    REQUIRE(U(CreateFileMappingFromApp));
    SET_ERR(0);
    HANDLE m = U(CreateFileMappingFromApp)(PAGEFILE, NULL, PAGE_EXECUTE_READWRITE, GR, NULL);
    t_note("CreateFileMappingFromApp(PAGE_EXECUTE_READWRITE): %p err=%lu", m, GetLastError());
    if (m) CloseHandle(m);
    SET_ERR(0);
    m = U(CreateFileMappingFromApp)(PAGEFILE, NULL, PAGE_READWRITE, GR, NULL);
    CHECK(m != NULL);
    if (m) {
        SET_ERR(0);
        void *v = U(MapViewOfFileFromApp)(m, FILE_MAP_WRITE | FILE_MAP_EXECUTE, 0, 0);
        t_note("MapViewOfFileFromApp(WRITE|EXECUTE): %p err=%lu", v, GetLastError());
        if (v) UMVF(v);
        CloseHandle(m);
    }
}
TEST(mv_mapviewoffile3_basic) {
    REQUIRE(U(MapViewOfFile3)); REQUIRE(CFMA); REQUIRE(VQ);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    SET_ERR(0);
    void *v = U(MapViewOfFile3)(m, NULL, NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION q = mv_q(v);
        EXPECT_EQ(q.Type, MEM_MAPPED); EXPECT_EQ(q.Protect, PAGE_READWRITE); EXPECT_EQ(q.RegionSize, GR);
        UMVF(v);
    }
    v = U(MapViewOfFile3)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READONLY, NULL, 0);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(mv_q(v).Protect, PAGE_READONLY); UMVF(v); }
    v = U(MapViewOfFile3)(m, NULL, NULL, 0, 2 * PG, 0, PAGE_READWRITE, NULL, 0);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(mv_q(v).RegionSize, 2 * PG); UMVF(v); }
    CloseHandle(m);
}
TEST(mv_mapviewoffile3_invalid) {
    REQUIRE(U(MapViewOfFile3)); REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    DWORD badtype[] = { MEM_COMMIT, MEM_DECOMMIT, MEM_RELEASE, MEM_RESET, MEM_WRITE_WATCH, 0x1, 0x2, 0x80000000 };
    for (int i = 0; i < 8; i++) {
        SET_ERR(0);
        void *v = U(MapViewOfFile3)(m, NULL, NULL, 0, 0, badtype[i], PAGE_READWRITE, NULL, 0);
        CHECKF(v == NULL, "alloc type 0x%lx", badtype[i]);
        if (v) UMVF(v); else ERR1(ERROR_INVALID_PARAMETER);
    }
    SET_ERR(0);
    void *v = U(MapViewOfFile3)(NULL, NULL, NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    CHECK(v == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    v = U(MapViewOfFile3)(m, (HANDLE)(ULONG_PTR)0x7771, NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    CHECK(v == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    /* protection incompatible with the section */
    HANDLE ro = CFMA(PAGEFILE, NULL, PAGE_READONLY, 0, GR, NULL);
    SET_ERR(0);
    v = U(MapViewOfFile3)(ro, NULL, NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    CHECK(v == NULL);
    if (v) UMVF(v); else ERR1(ERROR_ACCESS_DENIED);
    CloseHandle(ro);
    CloseHandle(m);
}
TEST(mv_mapviewoffile3_ext_params) {
    REQUIRE(U(MapViewOfFile3)); REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    MEM_EXTENDED_PARAMETER ep; memset(&ep, 0, sizeof ep);
    DWORD badtypes[] = { 0, 3, 4, 5, 6, 0x3F };
    for (int i = 0; i < 6; i++) {
        ep.Type = badtypes[i];
        SET_ERR(0);
        void *v = U(MapViewOfFile3)(m, NULL, NULL, 0, 0, 0, PAGE_READWRITE, &ep, 1);
        CHECKF(v == NULL, "ext type %lu", badtypes[i]);
        if (v) UMVF(v); else ERR1(ERROR_INVALID_PARAMETER);
    }
    /* NUMA node parameter */
    ep.Type = MemExtendedParameterNumaNode; ep.ULong = 0;
    SET_ERR(0);
    void *v = U(MapViewOfFile3)(m, NULL, NULL, 0, 0, 0, PAGE_READWRITE, &ep, 1);
    CHECKF(v != NULL, "numa node 0 err %lu", GetLastError());
    if (v) UMVF(v);
    /* count 0 with non-NULL pointer is accepted */
    v = U(MapViewOfFile3)(m, NULL, NULL, 0, 0, 0, PAGE_READWRITE, &ep, 0);
    CHECK(v != NULL);
    if (v) UMVF(v);
    CloseHandle(m);
}
TEST(mv_mapviewoffile3_address_requirements) {
    REQUIRE(U(MapViewOfFile3)); REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    MEM_ADDRESS_REQUIREMENTS req; memset(&req, 0, sizeof req);
    req.Alignment = 4 * GR;
    MEM_EXTENDED_PARAMETER ep; memset(&ep, 0, sizeof ep);
    ep.Type = MemExtendedParameterAddressRequirements; ep.Pointer = &req;
    SET_ERR(0);
    void *v = U(MapViewOfFile3)(m, NULL, NULL, 0, 0, 0, PAGE_READWRITE, &ep, 1);
    CHECKF(v != NULL, "err %lu", GetLastError());
    if (v) { CHECK(((ULONG_PTR)v & (4 * GR - 1)) == 0); UMVF(v); }
    req.Alignment = 3 * GR;                    /* not a power of two */
    SET_ERR(0);
    v = U(MapViewOfFile3)(m, NULL, NULL, 0, 0, 0, PAGE_READWRITE, &ep, 1);
    CHECK(v == NULL);
    if (v) UMVF(v); else ERR1(ERROR_INVALID_PARAMETER);
    CloseHandle(m);
}

/* ---------------- cross-process ---------------- */
CHILD(mapshare) {
    /* argv[1] = mapping name; verify parent's data, write reply */
    HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, argv[1]);
    if (!h) return 10;
    unsigned char *v = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!v) return 11;
    if (memcmp(v, "parent-says-hi", 14)) return 12;
    memcpy(v + 64, "child-here", 11);
    v[PG] = 0x5C;
    FlushViewOfFile(v, 0);
    UnmapViewOfFile(v);
    CloseHandle(h);
    return 0;
}
TEST(mv_fm_cross_process_shared) {
    REQUIRE(CFMA); REQUIRE(OFMA);
    char n[100]; mapname(n, sizeof n, "xproc");
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, n);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) {
        memcpy(v, "parent-says-hi", 14);
        DWORD code = t_run_child("mapshare", n, 20000);
        EXPECT_EQ(code, 0);
        CHECK(!memcmp(v + 64, "child-here", 11));
        EXPECT_EQ(v[PG], 0x5C);
        UMVF(v);
    }
    CloseHandle(m);
}
CHILD(mapinherit) {
    /* argv[1] = decimal handle value inherited from parent */
    HANDLE h = (HANDLE)(ULONG_PTR)_strtoui64(argv[1], NULL, 10);
    unsigned char *v = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!v) return 20;
    if (v[0] != 0xAB) return 21;
    v[1] = 0xCD;
    return 0;
}
TEST(mv_fm_cross_process_inherited_handle) {
    REQUIRE(CFMA);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE m = CFMA(PAGEFILE, &sa, PAGE_READWRITE, 0, GR, NULL);
    unsigned char *v = mapall(m);
    CHECK(v != NULL);
    if (v) {
        v[0] = 0xAB;
        char arg[64]; snprintf(arg, sizeof arg, "%llu", (unsigned long long)(ULONG_PTR)m);
        PROCESS_INFORMATION pi;
        HANDLE hp = t_spawn_child("mapinherit", arg, 0, NULL, NULL, NULL, TRUE, NULL, &pi);
        CHECK(hp != NULL);
        if (hp) {
            DWORD code = 0xFFFF;
            if (WaitForSingleObject(hp, 20000) != WAIT_OBJECT_0) TerminateProcess(hp, 1);
            GetExitCodeProcess(hp, &code);
            EXPECT_EQ(code, 0);
            EXPECT_EQ(v[1], 0xCD);
            CloseHandle(pi.hThread); CloseHandle(hp);
        }
        UMVF(v);
    }
    CloseHandle(m);
}
TEST(mv_fm_noninheritable_handle_not_inherited) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    char arg[64]; snprintf(arg, sizeof arg, "%llu", (unsigned long long)(ULONG_PTR)m);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("mapinherit", arg, 0, NULL, NULL, NULL, TRUE, NULL, &pi);
    CHECK(hp != NULL);
    if (hp) {
        DWORD code = 0xFFFF;
        if (WaitForSingleObject(hp, 20000) != WAIT_OBJECT_0) TerminateProcess(hp, 1);
        GetExitCodeProcess(hp, &code);
        EXPECT_EQ(code, 20);                       /* MapViewOfFile fails with the stale handle value */
        CloseHandle(pi.hThread); CloseHandle(hp);
    }
    CloseHandle(m);
}
TEST(mv_fm_dup_handle_rights) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    HANDLE d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), m, GetCurrentProcess(), &d, FILE_MAP_READ, FALSE, 0));
    SET_ERR(0);
    void *v = MVF(d, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(v == NULL);
    if (v) UMVF(v); else ERR1(ERROR_ACCESS_DENIED);
    v = MVF(d, FILE_MAP_READ, 0, 0, 0);
    CHECK(v != NULL);
    if (v) UMVF(v);
    SET_ERR(0);
    v = MVF(d, FILE_MAP_COPY, 0, 0, 0);
    t_note("FILE_MAP_COPY with FILE_MAP_READ-only handle: %p err=%lu", v, GetLastError());
    if (v) UMVF(v);
    CloseHandle(d);
    CloseHandle(m);
}
TEST(mv_fm_many_views_and_handles) {
    REQUIRE(CFMA);
    HANDLE m = CFMA(PAGEFILE, NULL, PAGE_READWRITE, 0, GR, NULL);
    void *v[64]; int n = 0;
    for (int i = 0; i < 64; i++) { v[i] = mapall(m); if (v[i]) n++; }
    EXPECT_EQ(n, 64);
    for (int i = 0; i < 64; i++) for (int j = i + 1; j < 64; j++) CHECK(v[i] != v[j]);
    for (int i = 0; i < 64; i++) if (v[i]) UMVF(v[i]);
    CloseHandle(m);
}
