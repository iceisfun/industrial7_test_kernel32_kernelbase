/* Atom tables: every test here runs against both the global table (Global*Atom*) and the
 * per-process local table (*Atom*). */
#include "ap.h"

SCOPED(add_string_range) {
    char n[64]; ap_aname(n, sizeof n, g, "range");
    SET_ERR(0xdeadbeef);
    ATOM a = ap_add_a(g, n);
    CHECKF(a >= 0xC000, "atom %x", a);
    EXPECT_EQ(GetLastError(), 0xdeadbeef);   /* success leaves last error alone */
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(ap_find_a(g, n), a);
    EXPECT_EQ(GetLastError(), 0xdeadbeef);
    ap_del(g, a);
}

SCOPED(add_twice_same_value) {
    char n[64]; ap_aname(n, sizeof n, g, "twice");
    ATOM a = ap_add_a(g, n), b = ap_add_a(g, n);
    CHECK(a >= 0xC000);
    EXPECT_EQ(a, b);
    ap_del(g, a); ap_del(g, a);
}

SCOPED(find_case_insensitive) {
    char n[64], u[64], l[64]; ap_aname(n, sizeof n, g, "CaseMix");
    strcpy(u, n); strcpy(l, n);
    for (char *p = u; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    for (char *p = l; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    ATOM a = ap_add_a(g, n);
    CHECK(a >= 0xC000);
    EXPECT_EQ(ap_find_a(g, u), a);
    EXPECT_EQ(ap_find_a(g, l), a);
    WCHAR w[64]; ap_widen(w, u);
    EXPECT_EQ(ap_find_w(g, w), a);
    ap_widen(w, l);
    EXPECT_EQ(ap_find_w(g, w), a);
    ap_del(g, a);
}

SCOPED(add_case_variant_same_atom_refcounted) {
    char n[64], u[64]; ap_aname(n, sizeof n, g, "cv");
    strcpy(u, n); for (char *p = u; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    ATOM a = ap_add_a(g, n), b = ap_add_a(g, u);
    CHECK(a >= 0xC000);
    EXPECT_EQ(a, b);
    EXPECT_EQ(ap_del(g, a), 0);
    EXPECT_EQ(ap_find_a(g, n), a);           /* second reference still alive */
    EXPECT_EQ(ap_del(g, a), 0);
    SET_ERR(0);
    EXPECT_EQ(ap_find_a(g, n), 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}

SCOPED(name_preserves_first_case) {
    char n[64], u[64], out[128]; ap_aname(n, sizeof n, g, "KeepCase");
    strcpy(u, n); for (char *p = u; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    ATOM a = ap_add_a(g, n); ap_add_a(g, u);
    CHECK(a >= 0xC000);
    UINT len = ap_name_a(g, a, out, sizeof out);
    EXPECT_EQ(len, strlen(n));
    EXPECT_STR(out, n);
    ap_del(g, a); ap_del(g, a);
}

SCOPED(wide_ansi_identity) {
    char n[64]; WCHAR w[64]; ap_aname(n, sizeof n, g, "wa"); ap_widen(w, n);
    ATOM a = ap_add_a(g, n), b = ap_add_w(g, w);
    CHECK(a >= 0xC000);
    EXPECT_EQ(a, b);
    EXPECT_EQ(ap_find_w(g, w), a);
    EXPECT_EQ(ap_find_a(g, n), a);
    ap_del(g, a); ap_del(g, a);
    ap_widen(w, n); w[ap_wlen(w) - 1] = 'Z';
    ATOM c = ap_add_w(g, w);
    char n2[64]; strcpy(n2, n); n2[strlen(n2) - 1] = 'Z';
    EXPECT_EQ(ap_find_a(g, n2), c);
    ap_del(g, c);
}

SCOPED(refcount_three_adds_three_deletes) {
    char n[64]; ap_aname(n, sizeof n, g, "rc3");
    ATOM a = ap_add_a(g, n);
    CHECK(a >= 0xC000);
    EXPECT_EQ(ap_add_a(g, n), a);
    EXPECT_EQ(ap_add_a(g, n), a);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(ap_del(g, a), 0); EXPECT_ERR(0xdeadbeef);
    EXPECT_EQ(ap_find_a(g, n), a);
    EXPECT_EQ(ap_del(g, a), 0);
    EXPECT_EQ(ap_find_a(g, n), a);
    EXPECT_EQ(ap_del(g, a), 0);
    SET_ERR(0);
    EXPECT_EQ(ap_find_a(g, n), 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    char out[64] = "x";
    SET_ERR(0);
    EXPECT_EQ(ap_name_a(g, a, out, sizeof out), 0);
    CHECK(GetLastError() != 0);
}

SCOPED(refcount_many) {
    char n[64]; ap_aname(n, sizeof n, g, "rc100");
    ATOM a = ap_add_a(g, n);
    for (int i = 1; i < 100; i++) EXPECT_EQ(ap_add_a(g, n), a);
    for (int i = 0; i < 99; i++) { ap_del(g, a); if (ap_find_a(g, n) != a) { CHECKF(0, "gone early at %d", i); break; } }
    ap_del(g, a);
    EXPECT_EQ(ap_find_a(g, n), 0);
}

SCOPED(delete_missing_sets_error) {
    char n[64]; ap_aname(n, sizeof n, g, "delmissing");
    ATOM a = ap_add_a(g, n);
    ap_del(g, a);
    SET_ERR(0);
    ATOM r = ap_del(g, a);
    DWORD e = GetLastError();
    CHECKF(r == 0 || r == a, "ret %x", r);
    CHECKF(e != 0, "no error set");
    t_note("delete of removed atom: ret=0x%x err=%lu", r, e);
}

SCOPED(delete_integer_atoms_noop) {
    ATOM vals[] = { 0, 1, 2, 0x100, 0x1234, 0xBFFE, 0xBFFF };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        SET_ERR(0xdeadbeef);
        EXPECT_EQ(ap_del(g, vals[i]), 0);
        EXPECT_ERR(0xdeadbeef);
    }
}

SCOPED(integer_atoms_via_makeintatom) {
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(ap_add_a(g, NULL), 0);
    EXPECT_EQ(ap_add_a(g, MAKEINTATOM(0)), 0);
    EXPECT_EQ(ap_add_w(g, NULL), 0);
    EXPECT_ERR(0xdeadbeef);
    ULONG_PTR vals[] = { 1, 2, 3, 255, 256, 0x1000, 0x7FFF, 0x8000, 0xBFFE, 0xBFFF };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        SET_ERR(0xdeadbeef);
        EXPECT_EQ(ap_add_a(g, (LPCSTR)vals[i]), vals[i]);
        EXPECT_EQ(ap_add_w(g, (LPCWSTR)vals[i]), vals[i]);
        EXPECT_EQ(ap_find_a(g, (LPCSTR)vals[i]), vals[i]);
        EXPECT_EQ(ap_find_w(g, (LPCWSTR)vals[i]), vals[i]);
        EXPECT_ERR(0xdeadbeef);
    }
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(ap_find_a(g, NULL), 0);
    EXPECT_ERR(0xdeadbeef);
}

SCOPED(integer_atom_range_exhaustive_add) {
    int bad = 0;
    for (ULONG_PTR i = 1; i <= 0xBFFF; i++) {
        if (ap_add_a(g, (LPCSTR)i) != i) bad++;
        if (ap_add_w(g, (LPCWSTR)i) != i) bad++;
    }
    EXPECT_EQ(bad, 0);
}

SCOPED(integer_atom_out_of_range_fails) {
    ULONG_PTR vals[] = { 0xC000, 0xC001, 0xC0FF, 0xD000, 0xFFFE, 0xFFFF };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        SET_ERR(0);
        EXPECT_EQ(ap_add_a(g, (LPCSTR)vals[i]), 0);
        t_note("add %Ix err=%lu", vals[i], GetLastError());
        SET_ERR(0);
        EXPECT_EQ(ap_add_w(g, (LPCWSTR)vals[i]), 0);

        SET_ERR(0);
        EXPECT_EQ(ap_find_a(g, (LPCSTR)vals[i]), 0);

        SET_ERR(0);
        EXPECT_EQ(ap_find_w(g, (LPCWSTR)vals[i]), 0);
    }
}

SCOPED(hash_integer_strings) {
    struct { const char *s; ATOM want; } t[] = {
        { "#1", 1 }, { "#2", 2 }, { "#1234", 1234 }, { "#49151", 0xBFFF }, { "#100", 100 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[32]; ap_widen(w, t[i].s);
        SET_ERR(0xdeadbeef);
        ATOM a = ap_add_a(g, t[i].s);
        if (a != t[i].want) CHECKF(0, "add \"%s\" -> %x want %x", t[i].s, a, t[i].want);
        EXPECT_ERR(0xdeadbeef);
        EXPECT_EQ(ap_add_w(g, w), t[i].want);
        EXPECT_EQ(ap_find_a(g, t[i].s), t[i].want);
        EXPECT_EQ(ap_find_w(g, w), t[i].want);
        EXPECT_EQ(ap_del(g, t[i].want), 0);   /* deleting an integer atom is a no-op */
    }
}

SCOPED(hash_non_integer_strings_are_string_atoms) {
    const char *t[] = { "#", "#abc", "#12abc", "#-5", "# 5", "#1.5", "##1" };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        ATOM a = ap_add_a(g, t[i]);
        CHECKF(a >= 0xC000, "\"%s\" -> %x", t[i], a);
        if (a) { char o[32] = ""; ap_name_a(g, a, o, sizeof o); EXPECT_STR(o, t[i]); ap_del(g, a); }
    }
}

SCOPED(hash_special_values_note) {
    const char *t[] = { "#0", "#49152", "#65535", "#65536", "#99999999999" };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SET_ERR(0);
        ATOM a = ap_add_a(g, t[i]);
        t_note("add \"%s\" -> 0x%x err=%lu", t[i], a, GetLastError());
        if (a >= 0xC000) ap_del(g, a);
    }
}

SCOPED(getname_basic_a) {
    char n[64], out[128]; ap_aname(n, sizeof n, g, "gnA");
    ATOM a = ap_add_a(g, n);
    CHECK(a >= 0xC000);
    memset(out, '.', sizeof out);
    SET_ERR(0xdeadbeef);
    UINT len = ap_name_a(g, a, out, sizeof out);
    EXPECT_EQ(len, strlen(n));
    EXPECT_ERR(0xdeadbeef);
    EXPECT_STR(out, n);
    EXPECT_EQ(out[len + 1], '.');
    ap_del(g, a);
}

SCOPED(getname_basic_w) {
    char n[64]; WCHAR w[64], out[128]; ap_aname(n, sizeof n, g, "gnW"); ap_widen(w, n);
    ATOM a = ap_add_w(g, w);
    CHECK(a >= 0xC000);
    for (int i = 0; i < 128; i++) out[i] = L'.';
    SET_ERR(0xdeadbeef);
    UINT len = ap_name_w(g, a, out, 128);
    EXPECT_EQ(len, strlen(n));
    EXPECT_ERR(0xdeadbeef);
    CHECK(!memcmp(out, w, (len + 1) * sizeof(WCHAR)));
    EXPECT_EQ(out[len + 1], L'.');
    ap_del(g, a);
}

SCOPED(getname_exact_fit_a) {
    char n[64], out[64]; ap_aname(n, sizeof n, g, "fitA");
    size_t L = strlen(n);
    ATOM a = ap_add_a(g, n);
    memset(out, '.', sizeof out);
    EXPECT_EQ(ap_name_a(g, a, out, (int)L + 1), L);      /* room for the NUL */
    EXPECT_STR(out, n);
    EXPECT_EQ(out[L + 1], '.');
    ap_del(g, a);
}

SCOPED(getname_truncation_a) {
    char n[64], out[64]; ap_aname(n, sizeof n, g, "truncA");
    size_t L = strlen(n);
    ATOM a = ap_add_a(g, n);
    if (!g) {   /* local table truncation semantics are emulator-dependent: observe only */
        memset(out, '.', sizeof out);
        SET_ERR(0);
        UINT r0 = ap_name_a(g, a, out, (int)L);
        t_note("local GetAtomNameA(nSize=len) -> %u err=%lu", r0, GetLastError());
        ap_del(g, a);
        return;
    }
    /* buffer exactly strlen(): no room for NUL -> failure, truncated + terminated */
    memset(out, '.', sizeof out);
    SET_ERR(0);
    UINT r = ap_name_a(g, a, out, (int)L);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(!memcmp(out, n, L - 1));
    EXPECT_EQ(out[L - 1], 0);
    EXPECT_EQ(out[L], '.');
    /* a few smaller sizes */
    for (int sz = 2; sz < (int)L; sz += 3) {
        memset(out, '.', sizeof out);
        SET_ERR(0);
        r = ap_name_a(g, a, out, sz);
        EXPECT_EQ(r, 0);
        EXPECT_ERR(ERROR_MORE_DATA);
        CHECK(!memcmp(out, n, sz - 1));
        EXPECT_EQ(out[sz - 1], 0);
        EXPECT_EQ(out[sz], '.');
    }
    ap_del(g, a);
}

SCOPED(getname_size_zero_and_one) {
    char n[64], out[16]; ap_aname(n, sizeof n, g, "sz01");
    ATOM a = ap_add_a(g, n);
    memset(out, '.', sizeof out);
    SET_ERR(0);
    EXPECT_EQ(ap_name_a(g, a, out, 0), 0);
    CHECK(out[0] == '.');
    t_note("GetAtomNameA(nSize=0) err=%lu", GetLastError());
    SET_ERR(0);
    UINT r = ap_name_a(g, a, out, 1);
    EXPECT_EQ(r, 0);
    CHECKF(out[0] == 0 || out[0] == '.', "out[0]=%d", out[0]);
    EXPECT_EQ(out[1], '.');
    WCHAR wo[8]; for (int i = 0; i < 8; i++) wo[i] = L'.';
    SET_ERR(0);
    EXPECT_EQ(ap_name_w(g, a, wo, 0), 0);
    EXPECT_EQ(wo[0], L'.');
    ap_del(g, a);
}

SCOPED(getname_truncation_w) {
    char n[64]; WCHAR w[64], out[64]; ap_aname(n, sizeof n, g, "truncW"); ap_widen(w, n);
    UINT L = (UINT)strlen(n);
    ATOM a = ap_add_w(g, w);
    if (!g) {
        for (int i = 0; i < 64; i++) out[i] = L'.';
        UINT r0 = ap_name_w(g, a, out, 10);
        t_note("local GetAtomNameW(nSize=10) -> %u err=%lu", r0, GetLastError());
        CHECK(out[10] == L'.');
        ap_del(g, a);
        return;
    }
    for (int i = 0; i < 64; i++) out[i] = L'.';
    UINT r = ap_name_w(g, a, out, 10);
    t_note("GetAtomNameW(nSize=10) -> %u err=%lu", r, GetLastError());
    CHECKF(r == 10 || r == 0, "ret %u", r);
    if (r == 10) CHECK(!memcmp(out, w, 10 * sizeof(WCHAR)));
    EXPECT_EQ(out[10], L'.');                 /* never writes past nSize */
    for (int i = 0; i < 64; i++) out[i] = L'.';
    r = ap_name_w(g, a, out, (int)L);
    CHECKF(r == L || r == 0, "ret %u", r);
    EXPECT_EQ(out[L], L'.');
    for (int i = 0; i < 64; i++) out[i] = L'.';
    EXPECT_EQ(ap_name_w(g, a, out, (int)L + 1), L);
    EXPECT_EQ(out[L], 0);
    ap_del(g, a);
}

SCOPED(getname_integer_atoms) {
    struct { ATOM a; const char *s; } t[] = { {1,"#1"}, {9,"#9"}, {10,"#10"}, {1234,"#1234"}, {0x7FFF,"#32767"}, {0xBFFF,"#49151"} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char out[32]; WCHAR wo[32], ws[32];
        memset(out, 'a', sizeof out);
        SET_ERR(0xdeadbeef);
        UINT r = ap_name_a(g, t[i].a, out, sizeof out);
        EXPECT_EQ(r, strlen(t[i].s));
        EXPECT_STR(out, t[i].s);
        EXPECT_ERR(0xdeadbeef);
        for (int k = 0; k < 32; k++) wo[k] = L'a';
        ap_widen(ws, t[i].s);
        r = ap_name_w(g, t[i].a, wo, 32);
        EXPECT_EQ(r, strlen(t[i].s));
        CHECK(!lstrcmpW(wo, ws));
    }
}

SCOPED(getname_integer_atom_small_buffer) {
    char out[8];
    if (!g) {
        SET_ERR(0);
        UINT r0 = ap_name_a(g, 1234, out, 3);
        t_note("local GetAtomNameA(1234, nSize=3) -> %u err=%lu", r0, GetLastError());
        return;
    }
    memset(out, 'a', sizeof out);
    SET_ERR(0);
    UINT r = ap_name_a(g, 1234, out, 3);       /* "#1234" does not fit in 3 */
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(out[2], 0);
    CHECK(!memcmp(out, "#1", 2));
    memset(out, 'a', sizeof out);
    r = ap_name_a(g, 1234, out, 6);
    EXPECT_EQ(r, 5);
    EXPECT_STR(out, "#1234");
}

SCOPED(getname_atom_zero) {
    char out[16]; WCHAR wo[16];
    memset(out, 'a', sizeof out);
    SET_ERR(0);
    EXPECT_EQ(ap_name_a(g, 0, out, sizeof out), 0);
    CHECK(GetLastError() != 0);
    SET_ERR(0);
    EXPECT_EQ(ap_name_w(g, 0, wo, 16), 0);
    CHECK(GetLastError() != 0);
    t_note("GetAtomName(0) err=%lu", GetLastError());
}

SCOPED(getname_nonexistent_string_atom) {
    char n[64], out[64] = "zz"; ap_aname(n, sizeof n, g, "ghost");
    ATOM a = ap_add_a(g, n);
    ap_del(g, a);
    SET_ERR(0);
    EXPECT_EQ(ap_name_a(g, a, out, sizeof out), 0);
    DWORD e = GetLastError();
    CHECK(e != 0);
    t_note("name of deleted atom: err=%lu", e);
    SET_ERR(0);
    EXPECT_EQ(ap_name_a(g, 0xFFFF, out, sizeof out), 0);
    CHECK(GetLastError() != 0);
}

SCOPED(name_length_255_ok) {
    char in[300], out[300]; WCHAR w[300], wo[300];
    for (int i = 0; i < 255; i++) in[i] = "abcdefghij"[i % 10];
    in[255] = 0;
    /* prefix with a unique tag to avoid collisions */
    char tag[32]; snprintf(tag, sizeof tag, "%c%07lx", g ? 'G' : 'L', GetCurrentProcessId() & 0xFFFFFFF);
    memcpy(in, tag, 8);
    ATOM a = ap_add_a(g, in);
    CHECK(a >= 0xC000);
    EXPECT_EQ(ap_name_a(g, a, out, sizeof out), 255);
    CHECK(!strcmp(out, in));
    ap_widen(w, in);
    EXPECT_EQ(ap_add_w(g, w), a);
    EXPECT_EQ(ap_name_w(g, a, wo, 300), 255);
    CHECK(!lstrcmpW(wo, w));
    /* exactly 255 chars into a 255 buffer: truncated */
    if (g) {
        SET_ERR(0);
        EXPECT_EQ(ap_name_a(g, a, out, 255), 0);
        EXPECT_ERR(ERROR_MORE_DATA);
        EXPECT_EQ(out[254], 0);
    }
    ap_del(g, a); ap_del(g, a);
}

SCOPED(name_length_256_fails) {
    char in[300]; WCHAR w[300];
    memset(in, 'q', 256); in[256] = 0;
    SET_ERR(0);
    EXPECT_EQ(ap_add_a(g, in), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    ap_widen(w, in);
    SET_ERR(0);
    EXPECT_EQ(ap_add_w(g, w), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(ap_find_a(g, in), 0);
    CHECK(GetLastError() != 0);
    t_note("find of 256 chars err=%lu", GetLastError());
    EXPECT_EQ(ap_find_w(g, w), 0);
    memset(in, 'q', 299); in[299] = 0;
    EXPECT_EQ(ap_add_a(g, in), 0);
    char big[2000]; memset(big, 'z', 1999); big[1999] = 0;
    EXPECT_EQ(ap_add_a(g, big), 0);
}

SCOPED(name_single_char) {
    char n[8]; snprintf(n, sizeof n, "%c", g ? 'G' : 'L');
    char n2[8]; snprintf(n2, sizeof n2, "%c", g ? 'g' : 'l');
    ATOM a = ap_add_a(g, n);
    CHECK(a >= 0xC000);
    EXPECT_EQ(ap_find_a(g, n2), a);
    ap_del(g, a);
}

SCOPED(name_empty_string) {
    SET_ERR(0);
    ATOM a = ap_add_a(g, "");
    t_note("add \"\" -> %x err=%lu", a, GetLastError());
    EXPECT_EQ(a, 0);
    CHECK(GetLastError() != 0);
    WCHAR e = 0;
    SET_ERR(0);
    EXPECT_EQ(ap_add_w(g, &e), 0);
    CHECK(GetLastError() != 0);
    SET_ERR(0);
    EXPECT_EQ(ap_find_a(g, ""), 0);
    CHECK(GetLastError() != 0);
    if (g) EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0);
    EXPECT_EQ(ap_find_w(g, &e), 0);
    CHECK(GetLastError() != 0);
}

SCOPED(name_special_characters) {
    const char *suffix[] = { " lead", "trail ", "two  spaces", "tab\tchar", "a.b", "a\\b", "a/b", "a*?b", "[x]", "%PATH%", "\"q\"", "semi;colon", "eq=ual" };
    for (unsigned i = 0; i < sizeof suffix / sizeof *suffix; i++) {
        char n[96], out[96]; snprintf(n, sizeof n, "k32t%lu%c%s", GetCurrentProcessId(), g ? 'G' : 'L', suffix[i]);
        ATOM a = ap_add_a(g, n);
        if (a < 0xC000) { CHECKF(0, "add \"%s\" -> %x", n, a); continue; }
        EXPECT_EQ(ap_name_a(g, a, out, sizeof out), strlen(n));
        if (strcmp(out, n)) CHECKF(0, "name mismatch for \"%s\" got \"%s\"", n, out);
        EXPECT_EQ(ap_find_a(g, n), a);
        ap_del(g, a);
    }
}

SCOPED(name_distinct_strings_distinct_atoms) {
    char n1[64], n2[64]; ap_aname(n1, sizeof n1, g, "d1"); ap_aname(n2, sizeof n2, g, "d2");
    ATOM a = ap_add_a(g, n1), b = ap_add_a(g, n2);
    CHECK(a >= 0xC000 && b >= 0xC000);
    EXPECT_NE(a, b);
    ap_del(g, a); ap_del(g, b);
}

SCOPED(name_trailing_space_distinct) {
    char n1[64], n2[64]; ap_aname(n1, sizeof n1, g, "ts"); snprintf(n2, sizeof n2, "%s ", n1);
    ATOM a = ap_add_a(g, n1), b = ap_add_a(g, n2);
    CHECK(a >= 0xC000 && b >= 0xC000);
    EXPECT_NE(a, b);
    ap_del(g, a); ap_del(g, b);
}

SCOPED(wide_non_ascii_names) {
    WCHAR w[64], o[64], lo[64];
    wsprintfW(w, L"k32t%lu%c_éÉЖ中", GetCurrentProcessId(), g ? 'G' : 'L');
    ATOM a = ap_add_w(g, w);
    CHECK(a >= 0xC000);
    EXPECT_EQ(ap_name_w(g, a, o, 64), (UINT)ap_wlen(w));
    CHECK(!lstrcmpW(o, w));
    EXPECT_EQ(ap_find_w(g, w), a);
    /* case folding applies to non-ASCII too: e-acute vs E-acute */
    wsprintfW(lo, L"k32t%lu%c_ÉÉЖ中", GetCurrentProcessId(), g ? 'G' : 'L');
    ATOM b = ap_find_w(g, lo);
    t_note("find with upper-cased e-acute -> %x (atom %x)", b, a);
    wsprintfW(lo, L"k32t%lu%c_éÉж中", GetCurrentProcessId(), g ? 'G' : 'L');
    b = ap_find_w(g, lo);
    t_note("find with lower-cased cyrillic -> %x (atom %x)", b, a);
    ap_del(g, a);
}

SCOPED(reuse_after_delete) {
    char n[64]; ap_aname(n, sizeof n, g, "reuse");
    for (int i = 0; i < 20; i++) {
        ATOM a = ap_add_a(g, n);
        if (a < 0xC000) { CHECKF(0, "iter %d add failed", i); break; }
        EXPECT_EQ(ap_find_a(g, n), a);
        EXPECT_EQ(ap_del(g, a), 0);
        EXPECT_EQ(ap_find_a(g, n), 0);
    }
}

SCOPED(many_atoms_2000) {
    enum { N = 2000 };
    static ATOM at[N];
    char n[80]; int fail = 0, dup = 0, bad = 0;
    for (int i = 0; i < N; i++) {
        snprintf(n, sizeof n, "k32t%lu_%c_many_%d", GetCurrentProcessId(), g ? 'G' : 'L', i);
        at[i] = ap_add_a(g, n);
        if (at[i] < 0xC000) fail++;
    }
    EXPECT_EQ(fail, 0);
    for (int i = 0; i < N; i++) for (int j = i + 1; j < i + 8 && j < N; j++) if (at[i] == at[j]) dup++;
    EXPECT_EQ(dup, 0);
    for (int i = 0; i < N; i++) {
        snprintf(n, sizeof n, "k32t%lu_%c_many_%d", GetCurrentProcessId(), g ? 'G' : 'L', i);
        if (ap_find_a(g, n) != at[i]) bad++;
    }
    EXPECT_EQ(bad, 0);
    for (int i = 0; i < N; i++) if (at[i] >= 0xC000) ap_del(g, at[i]);
    bad = 0;
    for (int i = 0; i < N; i++) {
        snprintf(n, sizeof n, "k32t%lu_%c_many_%d", GetCurrentProcessId(), g ? 'G' : 'L', i);
        if (ap_find_a(g, n) != 0) bad++;
    }
    EXPECT_EQ(bad, 0);
}

SCOPED(atom_value_range) {
    char n[64]; ap_aname(n, sizeof n, g, "val");
    ATOM a = ap_add_a(g, n);
    CHECK(a >= 0xC000 && a <= 0xFFFF);
    ap_del(g, a);
}

SCOPED(find_does_not_add_reference) {
    char n[64]; ap_aname(n, sizeof n, g, "findref");
    ATOM a = ap_add_a(g, n);
    for (int i = 0; i < 10; i++) EXPECT_EQ(ap_find_a(g, n), a);
    ap_del(g, a);
    EXPECT_EQ(ap_find_a(g, n), 0);
}

SCOPED(getname_does_not_add_reference) {
    char n[64], o[64]; ap_aname(n, sizeof n, g, "gnref");
    ATOM a = ap_add_a(g, n);
    for (int i = 0; i < 10; i++) ap_name_a(g, a, o, sizeof o);
    ap_del(g, a);
    EXPECT_EQ(ap_find_a(g, n), 0);
}

SCOPED(add_whitespace_only) {
    ATOM a = ap_add_a(g, "   ");
    t_note("whitespace-only atom -> %x", a);
    if (a >= 0xC000) { char o[8]; EXPECT_EQ(ap_name_a(g, a, o, 8), 3); ap_del(g, a); }
}

SCOPED(getname_ansi_buffer_large_clamp) {
    char n[64]; ap_aname(n, sizeof n, g, "bigbuf");
    ATOM a = ap_add_a(g, n);
    static char big[0x9000];
    memset(big, 'x', sizeof big);
    /* absurdly large nSize must not overrun: result limited to the real name */
    EXPECT_EQ(ap_name_a(g, a, big, 0x8F00), strlen(n));
    EXPECT_STR(big, n);
    static WCHAR bw[0x9000];
    EXPECT_EQ(ap_name_w(g, a, bw, 0x8F00), strlen(n));
    ap_del(g, a);
}
