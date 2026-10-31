/* Power requests, enclaves, named pipe attributes, CONTEXT/XState, API sets, Internet time helpers,
 * remote pointer encoding, ANSI identity helpers, export probes. */
#define NTDDI_VERSION 0x0A000008
#include "t.h"
#include <winioctl.h>
#include <appmodel.h>
#include "am_compat.h"

#define PipeAttribute 0
#define PipeConnectionAttribute 1
#define PipeHandleAttribute 2
typedef int PIPE_ATTRIBUTE_TYPE;
UDECL(BOOL, GetNamedPipeAttribute, (HANDLE, PIPE_ATTRIBUTE_TYPE, PSTR, PVOID, PSIZE_T));
UDECL(BOOL, SetNamedPipeAttribute, (HANDLE, PIPE_ATTRIBUTE_TYPE, PSTR, PVOID, SIZE_T));
UDECL(BOOL, IsApiSetImplemented, (PCSTR));
UDECL(BOOL, IsEnclaveTypeSupported, (DWORD));

static int g_cf;
#define KC(c) do { if (!(c) && !g_cf) g_cf = __LINE__; } while (0)

/* ======================= Power requests ======================= */
static HANDLE pw_create(const WCHAR *reason) {
    REASON_CONTEXT rc; memset(&rc, 0, sizeof rc);
    rc.Version = POWER_REQUEST_CONTEXT_VERSION;
    rc.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    rc.Reason.SimpleReasonString = (LPWSTR)reason;
    return U(PowerCreateRequest)(&rc);
}
TEST(pwr_create_simple) {
    REQUIRE(U(PowerCreateRequest));
    HANDLE h = pw_create(L"k32t power request");
    CHECK(h != NULL && h != INVALID_HANDLE_VALUE);
    if (h && h != INVALID_HANDLE_VALUE) CHECK(CloseHandle(h));
}
TEST(pwr_create_null_context) {
    REQUIRE(U(PowerCreateRequest));
    HANDLE h = U(PowerCreateRequest)(NULL);
    t_note("PowerCreateRequest(NULL) -> %p (err %lu)", h, h == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
TEST(pwr_create_invalid_contexts) {
    REQUIRE(U(PowerCreateRequest));
    REASON_CONTEXT rc;
    WCHAR r[] = L"x";
    struct { ULONG ver; DWORD flags; } t[] = {
        { 1, POWER_REQUEST_CONTEXT_SIMPLE_STRING }, { 2, POWER_REQUEST_CONTEXT_SIMPLE_STRING }, { 0xFFFFFFFF, POWER_REQUEST_CONTEXT_SIMPLE_STRING },
        { 0, 0 }, { 0, 0x8 }, { 0, 0x10 }, { 0, 0x100 }, { 0, 0x40000000 },
    };
    for (size_t i = 0; i < ARRAYSIZE(t); i++) {
        memset(&rc, 0, sizeof rc);
        rc.Version = t[i].ver; rc.Flags = t[i].flags; rc.Reason.SimpleReasonString = r;
        SET_ERR(0);
        HANDLE h = U(PowerCreateRequest)(&rc);
        CHECKF(h == INVALID_HANDLE_VALUE, "case %zu returned %p", i, h);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}
TEST(pwr_create_detailed_probe) {
    REQUIRE(U(PowerCreateRequest));
    REASON_CONTEXT rc; memset(&rc, 0, sizeof rc);
    rc.Version = 0; rc.Flags = POWER_REQUEST_CONTEXT_DETAILED_STRING;
    rc.Reason.Detailed.LocalizedReasonModule = NULL;
    rc.Reason.Detailed.LocalizedReasonId = 1;
    rc.Reason.Detailed.ReasonStringCount = 0;
    SET_ERR(0);
    HANDLE h = U(PowerCreateRequest)(&rc);
    t_note("detailed context -> %p (err %lu)", h, h == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    rc.Reason.Detailed.LocalizedReasonId = 0x10000;   /* > 0xFFFF is rejected */
    SET_ERR(0);
    h = U(PowerCreateRequest)(&rc);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
TEST(pwr_set_clear_all_types) {
    REQUIRE(U(PowerCreateRequest)); REQUIRE(U(PowerSetRequest)); REQUIRE(U(PowerClearRequest));
    HANDLE h = pw_create(L"k32t power types");
    if (!h || h == INVALID_HANDLE_VALUE) { t_note("cannot create power request (%lu)", GetLastError()); return; }
    static const int types[] = { PowerRequestDisplayRequired, PowerRequestSystemRequired, PowerRequestAwayModeRequired, 3 };
    for (size_t i = 0; i < ARRAYSIZE(types); i++) {
        SET_ERR(0xdead);
        BOOL r = U(PowerSetRequest)(h, (POWER_REQUEST_TYPE)types[i]);
        if (!r && types[i] == PowerRequestAwayModeRequired) { t_note("AwayMode set failed: %lu", GetLastError()); continue; }
        CHECKF(r, "set type %d err %lu", types[i], GetLastError());
        r = U(PowerSetRequest)(h, (POWER_REQUEST_TYPE)types[i]);   /* idempotent */
        CHECKF(r, "second set type %d", types[i]);
        r = U(PowerClearRequest)(h, (POWER_REQUEST_TYPE)types[i]);
        CHECKF(r, "clear type %d err %lu", types[i], GetLastError());
        r = U(PowerClearRequest)(h, (POWER_REQUEST_TYPE)types[i]);   /* clearing twice is fine */
        CHECKF(r, "second clear type %d", types[i]);
    }
    CloseHandle(h);
}
TEST(pwr_invalid_request_types) {
    REQUIRE(U(PowerCreateRequest)); REQUIRE(U(PowerSetRequest)); REQUIRE(U(PowerClearRequest));
    HANDLE h = pw_create(L"k32t power invalid type");
    if (!h || h == INVALID_HANDLE_VALUE) { t_note("cannot create power request"); return; }
    static const int bad[] = { 4, 5, 8, 100, -1, 0x7FFFFFFF, (int)0x80000000 };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        SET_ERR(0);
        BOOL r = U(PowerSetRequest)(h, (POWER_REQUEST_TYPE)bad[i]);
        CHECKF(!r, "set type %d", bad[i]);
        t_note("PowerSetRequest(type %d) err %lu", bad[i], GetLastError());
        r = U(PowerClearRequest)(h, (POWER_REQUEST_TYPE)bad[i]);
        CHECKF(!r, "clear type %d", bad[i]);
        CHECK(GetLastError() != 0);
    }
    CloseHandle(h);
}
TEST(pwr_invalid_handles) {
    REQUIRE(U(PowerSetRequest)); REQUIRE(U(PowerClearRequest));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = { NULL, INVALID_HANDLE_VALUE, ev, (HANDLE)(ULONG_PTR)0x7FFFF0, GetCurrentProcess() };
    for (size_t i = 0; i < ARRAYSIZE(hs); i++) {
        SET_ERR(0);
        BOOL r = U(PowerSetRequest)(hs[i], PowerRequestSystemRequired);
        CHECKF(!r, "set handle %zu", i);
        DWORD e = GetLastError();
        CHECKF(e != 0, "set handle %zu no error", i);
        t_note("PowerSetRequest(handle %zu) err %lu", i, e);
        r = U(PowerClearRequest)(hs[i], PowerRequestSystemRequired);
        CHECKF(!r, "clear handle %zu", i);
    }
    CloseHandle(ev);
}
TEST(pwr_closed_handle) {
    REQUIRE(U(PowerCreateRequest)); REQUIRE(U(PowerSetRequest));
    HANDLE h = pw_create(L"k32t closed");
    if (!h || h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    SET_ERR(0);
    CHECK(!U(PowerSetRequest)(h, PowerRequestSystemRequired));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(pwr_close_while_set) {
    REQUIRE(U(PowerCreateRequest)); REQUIRE(U(PowerSetRequest));
    HANDLE h = pw_create(L"k32t close while set");
    if (!h || h == INVALID_HANDLE_VALUE) return;
    CHECK(U(PowerSetRequest)(h, PowerRequestSystemRequired));
    CHECK(CloseHandle(h));   /* releases the request */
}
TEST(pwr_duplicate_handle_shares_request) {
    REQUIRE(U(PowerCreateRequest)); REQUIRE(U(PowerSetRequest)); REQUIRE(U(PowerClearRequest));
    HANDLE h = pw_create(L"k32t dup");
    if (!h || h == INVALID_HANDLE_VALUE) return;
    HANDLE d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    if (d) {
        CHECK(U(PowerSetRequest)(d, PowerRequestSystemRequired));
        CHECK(U(PowerClearRequest)(h, PowerRequestSystemRequired));
        CloseHandle(d);
    }
    CloseHandle(h);
}
static DWORD WINAPI pw_thread(LPVOID p) {
    HANDLE h = *(HANDLE *)p;
    BOOL a = U(PowerSetRequest)(h, PowerRequestSystemRequired);
    BOOL b = U(PowerClearRequest)(h, PowerRequestSystemRequired);
    return (a && b) ? 0 : 1;
}
TEST(pwr_cross_thread) {
    REQUIRE(U(PowerCreateRequest));
    HANDLE h = pw_create(L"k32t thread");
    if (!h || h == INVALID_HANDLE_VALUE) return;
    HANDLE t = CreateThread(NULL, 0, pw_thread, &h, 0, NULL);
    CHECK(t != NULL);
    if (t) { WaitForSingleObject(t, 5000); DWORD c = 9; GetExitCodeThread(t, &c); EXPECT_EQ(c, 0); CloseHandle(t); }
    CloseHandle(h);
}
TEST(pwr_many_requests) {
    REQUIRE(U(PowerCreateRequest));
    HANDLE hs[64]; int n = 0;
    for (int i = 0; i < 64; i++) { hs[n] = pw_create(L"k32t many"); if (!hs[n] || hs[n] == INVALID_HANDLE_VALUE) break; n++; }
    CHECK(n == 64);
    for (int i = 0; i < n; i++) CloseHandle(hs[i]);
}
TEST(pwr_handle_type_info) {
    REQUIRE(U(PowerCreateRequest));
    HANDLE h = pw_create(L"k32t type");
    if (!h || h == INVALID_HANDLE_VALUE) return;
    DWORD fl = 0;
    CHECK(GetHandleInformation(h, &fl));
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    CHECK(!SetEvent(h));   /* not an event */
    CloseHandle(h);
}

/* ======================= Enclaves ======================= */
TEST(enc_type_supported_table) {
    REQUIRE(UN(IsEnclaveTypeSupported));
    static const DWORD unknown[] = { 0, 3, 4, 5, 8, 0x11, 0x20, 0x100, 0x7777, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < ARRAYSIZE(unknown); i++) {
        BOOL r = UN(IsEnclaveTypeSupported)(unknown[i]);
        CHECKF(r == FALSE, "type 0x%lx", unknown[i]);
    }
}
TEST(enc_type_supported_known_values_are_bool) {
    REQUIRE(UN(IsEnclaveTypeSupported));
    static const DWORD known[] = { 1, 2, 0x10 };
    for (size_t i = 0; i < ARRAYSIZE(known); i++) {
        BOOL r = UN(IsEnclaveTypeSupported)(known[i]);
        CHECKF(r == 0 || r == 1, "type %lu -> %d", known[i], r);
        t_note("IsEnclaveTypeSupported(0x%lx) = %d", known[i], r);
    }
}
TEST(enc_type_supported_stable_and_no_error) {
    REQUIRE(UN(IsEnclaveTypeSupported));
    SET_ERR(0x5151);
    BOOL a = UN(IsEnclaveTypeSupported)(0x10), b = UN(IsEnclaveTypeSupported)(0x10);
    EXPECT_EQ(a, b);
    EXPECT_EQ(GetLastError(), 0x5151);
}
UDECL(PVOID, CreateEnclave, (HANDLE, LPVOID, SIZE_T, SIZE_T, DWORD, LPCVOID, DWORD, LPDWORD));
UDECL(BOOL, InitializeEnclave, (HANDLE, LPVOID, LPCVOID, DWORD, LPDWORD));
UDECL(BOOL, LoadEnclaveImageW, (LPVOID, LPCWSTR));
UDECL(BOOL, LoadEnclaveData, (HANDLE, LPVOID, LPCVOID, SIZE_T, DWORD, LPCVOID, DWORD, PSIZE_T, LPDWORD));
UDECL(BOOL, TerminateEnclave, (LPVOID, BOOL));
UDECL(BOOL, DeleteEnclave, (LPVOID));
CHILD(kbx_enclave) {
    typedef PVOID (WINAPI *ce_t)(HANDLE, LPVOID, SIZE_T, SIZE_T, DWORD, LPCVOID, DWORD, LPDWORD);
    typedef BOOL (WINAPI *ie_t)(HANDLE, LPVOID, LPCVOID, DWORD, LPDWORD);
    typedef BOOL (WINAPI *le_t)(LPVOID, LPCWSTR);
    typedef BOOL (WINAPI *te_t)(LPVOID, BOOL);
    typedef BOOL (WINAPI *de_t)(LPVOID);
    ce_t ce = (ce_t)t_proc("CreateEnclave"); ie_t ie = (ie_t)t_proc("InitializeEnclave");
    le_t le = (le_t)t_proc("LoadEnclaveImageW"); te_t te = (te_t)t_proc("TerminateEnclave");
    de_t de = (de_t)t_proc("DeleteEnclave");
    if (!ce || !ie || !le || !te || !de) return 0;
    BYTE info[64]; memset(info, 0, sizeof info);
    DWORD err = 0x77;
    SetLastError(0);
    PVOID e = ce(GetCurrentProcess(), NULL, 0x200000, 0, 0x7777, info, sizeof info, &err);
    KC(e == NULL);
    DWORD le1 = GetLastError();
    KC(le1 != 0);
    if (e) de(e);
    SetLastError(0);
    e = ce(GetCurrentProcess(), NULL, 0x200000, 0, 0, info, sizeof info, &err);   /* type 0 does not exist */
    KC(e == NULL);
    if (e) de(e);
    /* operations on an address that is not an enclave */
    PVOID mem = VirtualAlloc(NULL, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    KC(mem != NULL);
    if (mem) {
        SetLastError(0);
        KC(!ie(GetCurrentProcess(), mem, info, sizeof info, &err));
        KC(GetLastError() != 0);
        SetLastError(0);
        KC(!te(mem, TRUE));
        KC(GetLastError() != 0);
        SetLastError(0);
        KC(!de(mem));
        KC(GetLastError() != 0);
        SetLastError(0);
        KC(!le(mem, L"k32t_no_such_enclave.dll"));
        KC(GetLastError() != 0);
        VirtualFree(mem, 0, MEM_RELEASE);
    }
    SetLastError(0);
    KC(!le(NULL, L"k32t_no_such_enclave.dll"));
    KC(GetLastError() != 0);
    return g_cf;
}
TEST(enc_error_paths_in_child) {
    REQUIRE(UN(CreateEnclave)); REQUIRE(UN(LoadEnclaveImageW));
    DWORD code = t_run_child("kbx_enclave", "", 25000);
    if ((code & 0xF0000000) == 0xC0000000) t_note("enclave error-path child crashed 0x%lx", code);
    else CHECKF(code == 0, "child first failing line %lu", code);
}
TEST(enc_exports_present) {
    static const char *n[] = { "CreateEnclave", "InitializeEnclave", "LoadEnclaveData", "IsEnclaveTypeSupported", "CallEnclave", "TerminateEnclave", "DeleteEnclave", "LoadEnclaveImageW", "LoadEnclaveImageA" };
    for (size_t i = 0; i < ARRAYSIZE(n); i++) CHECKF(t_proc(n[i]) != NULL, "%s", n[i]);
}

/* ======================= Named pipe attributes ======================= */
static HANDLE np_server(const char *name) {
    char full[128]; snprintf(full, sizeof full, "\\\\.\\pipe\\k32t_%lu_%s", GetCurrentProcessId(), name);
    return CreateNamedPipeA(full, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
}
TEST(npa_invalid_type) {
    if (t_is_wine()) { t_skip("pipe attribute APIs are unimplemented stubs under wine"); return; }
    REQUIRE(UN(GetNamedPipeAttribute)); REQUIRE(UN(SetNamedPipeAttribute));
    HANDLE p = np_server("itype");
    CHECK(p != INVALID_HANDLE_VALUE);
    if (p == INVALID_HANDLE_VALUE) return;
    static const int bad[] = { 3, 4, 100, -1, 0x7FFFFFFF };
    char v[8] = "val"; SIZE_T len = 4;
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        SET_ERR(0);
        BOOL r = UN(SetNamedPipeAttribute)(p, (PIPE_ATTRIBUTE_TYPE)bad[i], (PSTR)"k32t", v, 4);
        CHECKF(!r, "set type %d", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        len = 8; SET_ERR(0);
        r = UN(GetNamedPipeAttribute)(p, (PIPE_ATTRIBUTE_TYPE)bad[i], (PSTR)"k32t", v, &len);
        CHECKF(!r, "get type %d", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CloseHandle(p);
}
TEST(npa_roundtrip_probe) {
    if (t_is_wine()) { t_skip("pipe attribute APIs are unimplemented stubs under wine"); return; }
    REQUIRE(UN(GetNamedPipeAttribute)); REQUIRE(UN(SetNamedPipeAttribute));
    HANDLE p = np_server("rt");
    if (p == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    char v[16] = "hello"; SIZE_T len = 6;
    SET_ERR(0);
    BOOL r = UN(SetNamedPipeAttribute)(p, PipeAttribute, (PSTR)"k32t_attr", v, 6);
    t_note("SetNamedPipeAttribute -> %d (err %lu)", r, r ? 0 : GetLastError());
    if (r) {
        char o[16]; memset(o, 0, sizeof o); len = sizeof o;
        r = UN(GetNamedPipeAttribute)(p, PipeAttribute, (PSTR)"k32t_attr", o, &len);
        CHECK(r);
        if (r) { EXPECT_EQ(len, 6); EXPECT_STR(o, "hello"); }
        /* overwrite */
        r = UN(SetNamedPipeAttribute)(p, PipeAttribute, (PSTR)"k32t_attr", "bye", 4);
        CHECK(r);
        len = sizeof o;
        r = UN(GetNamedPipeAttribute)(p, PipeAttribute, (PSTR)"k32t_attr", o, &len);
        if (r) { EXPECT_EQ(len, 4); EXPECT_STR(o, "bye"); }
        /* too small */
        len = 1; SET_ERR(0);
        r = UN(GetNamedPipeAttribute)(p, PipeAttribute, (PSTR)"k32t_attr", o, &len);
        CHECK(!r);
        t_note("Get with 1-byte buffer -> err %lu len %zu", GetLastError(), (size_t)len);
    }
    /* unknown attribute name */
    char o[16]; len = sizeof o; SET_ERR(0);
    r = UN(GetNamedPipeAttribute)(p, PipeAttribute, (PSTR)"k32t_missing_attr", o, &len);
    CHECK(!r);
    t_note("Get missing attribute -> err %lu", GetLastError());
    CloseHandle(p);
}
TEST(npa_wrong_handles) {
    if (t_is_wine()) { t_skip("pipe attribute APIs are unimplemented stubs under wine"); return; }
    REQUIRE(UN(GetNamedPipeAttribute)); REQUIRE(UN(SetNamedPipeAttribute));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    char v[8] = "x"; SIZE_T len = 8;
    SET_ERR(0);
    CHECK(!UN(SetNamedPipeAttribute)(ev, PipeAttribute, (PSTR)"k32t", v, 2));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!UN(GetNamedPipeAttribute)(ev, PipeAttribute, (PSTR)"k32t", v, &len));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!UN(SetNamedPipeAttribute)(INVALID_HANDLE_VALUE, PipeAttribute, (PSTR)"k32t", v, 2));
    CHECK(!UN(GetNamedPipeAttribute)(INVALID_HANDLE_VALUE, PipeAttribute, (PSTR)"k32t", v, &len));
    CHECK(!UN(SetNamedPipeAttribute)(NULL, PipeAttribute, (PSTR)"k32t", v, 2));
    CHECK(!UN(GetNamedPipeAttribute)(NULL, PipeAttribute, (PSTR)"k32t", v, &len));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "npa_file.bin");
    HANDLE f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        len = 8; SET_ERR(0);
        CHECK(!UN(GetNamedPipeAttribute)(f, PipeAttribute, (PSTR)"k32t", v, &len));
        t_note("Get on a regular file -> err %lu", GetLastError());
        CHECK(!UN(SetNamedPipeAttribute)(f, PipeAttribute, (PSTR)"k32t", v, 2));
        CloseHandle(f);
    }
    CloseHandle(ev);
}
TEST(npa_connection_attribute_probe) {
    if (t_is_wine()) { t_skip("pipe attribute APIs are unimplemented stubs under wine"); return; }
    REQUIRE(UN(GetNamedPipeAttribute));
    HANDLE p = np_server("conn");
    if (p == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    char o[64]; SIZE_T len = sizeof o; SET_ERR(0);
    BOOL r = UN(GetNamedPipeAttribute)(p, PipeConnectionAttribute, (PSTR)"ClientProcessId", o, &len);
    CHECK(!r);   /* no client is connected */
    t_note("Get connection attribute without client -> err %lu", GetLastError());
    len = sizeof o;
    r = UN(GetNamedPipeAttribute)(p, PipeHandleAttribute, (PSTR)"ClientProcessId", o, &len);
    CHECK(!r);
    CloseHandle(p);
}

/* ======================= CONTEXT / XState ======================= */
#define CTXFLAGS_ALL CONTEXT_ALL
static DWORD cx_len(DWORD flags, BOOL *ok, DWORD *err) {
    DWORD len = 0; SET_ERR(0);
    PCONTEXT c = NULL;
    BOOL r = U(InitializeContext)(NULL, flags, &c, &len);
    if (ok) *ok = r;
    if (err) *err = GetLastError();
    return len;
}
TEST(cx_init_size_query) {
    REQUIRE(U(InitializeContext));
    BOOL ok; DWORD err;
    DWORD len = cx_len(CTXFLAGS_ALL, &ok, &err);
    CHECK(!ok); EXPECT_EQ(err, ERROR_INSUFFICIENT_BUFFER);
    CHECK(len >= sizeof(CONTEXT));
    CHECK(len <= sizeof(CONTEXT) + 0x18 + 0xF);
    EXPECT_EQ(len, sizeof(CONTEXT) + 0x18 + 0xF);
}
TEST(cx_init_buffer_boundaries) {
    REQUIRE(U(InitializeContext));
    DWORD need = cx_len(CTXFLAGS_ALL, NULL, NULL);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, need + 64);
    PCONTEXT c = NULL; DWORD len;
    /* one byte short */
    len = need - 1; SET_ERR(0);
    CHECK(!U(InitializeContext)(buf, CTXFLAGS_ALL, &c, &len));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(len, need);
    /* zero length with a buffer */
    len = 0; SET_ERR(0);
    CHECK(!U(InitializeContext)(buf, CTXFLAGS_ALL, &c, &len));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(len, need);
    /* exact */
    len = need; c = NULL;
    CHECK(U(InitializeContext)(buf, CTXFLAGS_ALL, &c, &len));
    EXPECT_EQ(len, need);
    CHECK(c != NULL);
    if (c) {
        CHECK(((ULONG_PTR)c & 15) == 0);
        CHECK((BYTE *)c >= buf && (BYTE *)c + sizeof(CONTEXT) <= buf + need);
        EXPECT_EQ(c->ContextFlags, CTXFLAGS_ALL);
    }
    /* larger buffer: reports the needed size, not the buffer size */
    len = need + 64; c = NULL;
    CHECK(U(InitializeContext)(buf, CTXFLAGS_ALL, &c, &len));
    EXPECT_EQ(len, need);
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(cx_init_flag_variants) {
    REQUIRE(U(InitializeContext));
    static const DWORD f[] = { CONTEXT_CONTROL, CONTEXT_INTEGER, CONTEXT_SEGMENTS, CONTEXT_FLOATING_POINT, CONTEXT_DEBUG_REGISTERS, CONTEXT_FULL, CONTEXT_ALL };
    for (size_t i = 0; i < ARRAYSIZE(f); i++) {
        BOOL ok; DWORD err;
        DWORD len = cx_len(f[i], &ok, &err);
        CHECKF(!ok && err == ERROR_INSUFFICIENT_BUFFER, "flags 0x%lx ok=%d err=%lu", f[i], ok, err);
        CHECKF(len >= sizeof(CONTEXT), "flags 0x%lx len %lu", f[i], len);
        BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
        PCONTEXT c = NULL; DWORD l2 = len;
        CHECKF(U(InitializeContext)(buf, f[i], &c, &l2), "init 0x%lx", f[i]);
        if (c) EXPECT_EQ(c->ContextFlags, f[i]);
        HeapFree(GetProcessHeap(), 0, buf);
    }
}
TEST(cx_init_invalid_flags) {
    REQUIRE(U(InitializeContext));
    static const DWORD bad[] = { 0, 0x1, 0x2, 0x100001 | 0x20000000, 0xFFFFFFFF, 0x80000000 };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        BOOL ok; DWORD err;
        BYTE buf[4096]; PCONTEXT c = NULL; DWORD len = sizeof buf;
        SET_ERR(0);
        BOOL r = U(InitializeContext)(buf, bad[i], &c, &len);
        err = GetLastError(); ok = r;
        CHECKF(!ok, "flags 0x%lx accepted", bad[i]);
        t_note("InitializeContext(flags 0x%lx) err %lu", bad[i], err);
    }
}
TEST(cx_init_xstate) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(GetEnabledXStateFeatures));
    DWORD plain = cx_len(CONTEXT_ALL, NULL, NULL);
    BOOL ok; DWORD err;
    DWORD xs = cx_len(CONTEXT_ALL | CONTEXT_XSTATE, &ok, &err);
    CHECK(!ok); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(xs >= plain);
    DWORD64 feat = U(GetEnabledXStateFeatures)();
    if (feat & XSTATE_MASK_AVX) CHECK(xs > plain + 0x100);
    t_note("plain %lu xstate %lu enabled features 0x%llx", plain, xs, (unsigned long long)feat);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, xs);
    PCONTEXT c = NULL; DWORD len = xs;
    CHECK(U(InitializeContext)(buf, CONTEXT_ALL | CONTEXT_XSTATE, &c, &len));
    EXPECT_EQ(len, xs);
    if (c) {
        EXPECT_EQ(c->ContextFlags, CONTEXT_ALL | CONTEXT_XSTATE);
        CHECK(((ULONG_PTR)c & 15) == 0);
        DWORD64 mask = 0xdead;
        CHECK(U(GetXStateFeaturesMask)(c, &mask));
        CHECK((mask & 3) == 3);
        t_note("fresh xstate mask 0x%llx", (unsigned long long)mask);
    }
    len = xs - 1; SET_ERR(0);
    CHECK(!U(InitializeContext)(buf, CONTEXT_ALL | CONTEXT_XSTATE, &c, &len));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(cx_init2_compaction_mask) {
    REQUIRE(U(InitializeContext2)); REQUIRE(U(GetEnabledXStateFeatures));
    DWORD64 feat = U(GetEnabledXStateFeatures)();
    DWORD l_all = 0, l_legacy = 0, l_zero = 0;
    PCONTEXT c = NULL;
    SET_ERR(0);
    CHECK(!U(InitializeContext2)(NULL, CONTEXT_ALL | CONTEXT_XSTATE, &c, &l_all, feat));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(!U(InitializeContext2)(NULL, CONTEXT_ALL | CONTEXT_XSTATE, &c, &l_legacy, 3));
    CHECK(!U(InitializeContext2)(NULL, CONTEXT_ALL | CONTEXT_XSTATE, &c, &l_zero, 0));
    CHECK(l_legacy <= l_all);
    CHECK(l_zero <= l_legacy);
    t_note("InitializeContext2 sizes: all %lu legacy %lu zero %lu", l_all, l_legacy, l_zero);
    /* without CONTEXT_XSTATE the compaction mask has no effect */
    DWORD a = 0, b = 0;
    U(InitializeContext2)(NULL, CONTEXT_ALL, &c, &a, feat);
    U(InitializeContext2)(NULL, CONTEXT_ALL, &c, &b, 0);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, cx_len(CONTEXT_ALL, NULL, NULL));
}
TEST(cx_enabled_features) {
    REQUIRE(U(GetEnabledXStateFeatures));
    DWORD64 f = U(GetEnabledXStateFeatures)();
    CHECK((f & 3) == 3);    /* x87 + SSE always */
    EXPECT_EQ(f, U(GetEnabledXStateFeatures)());
    if (!t_is_wine()) {
        DWORD64 shared = *(volatile DWORD64 *)(ULONG_PTR)0x7FFE03D8;   /* KUSER_SHARED_DATA.XState.EnabledFeatures */
        CHECK((f & shared) == f);
        t_note("enabled features 0x%llx (shared 0x%llx)", (unsigned long long)f, (unsigned long long)shared);
    }
}
TEST(cx_features_mask_plain_context) {
    REQUIRE(U(GetXStateFeaturesMask));
    CONTEXT c; memset(&c, 0, sizeof c);
    DWORD64 m = 0xdead;
    c.ContextFlags = CONTEXT_ALL;
    CHECK(U(GetXStateFeaturesMask)(&c, &m)); EXPECT_EQ(m, 3);
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    m = 0xdead;
    CHECK(U(GetXStateFeaturesMask)(&c, &m)); EXPECT_EQ(m, 0);
    c.ContextFlags = CONTEXT_FLOATING_POINT;
    CHECK(U(GetXStateFeaturesMask)(&c, &m)); EXPECT_EQ(m, 3);
}
TEST(cx_features_mask_invalid_context) {
    REQUIRE(U(GetXStateFeaturesMask));
    CONTEXT c; memset(&c, 0, sizeof c);
    DWORD64 m = 0xdead;
    c.ContextFlags = 0;
    SET_ERR(0);
    CHECK(!U(GetXStateFeaturesMask)(&c, &m));
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
    EXPECT_EQ(m, 0xdead);
}
TEST(cx_locate_legacy_features) {
    REQUIRE(U(LocateXStateFeature));
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_ALL;
    DWORD len = 0;
    PVOID p = U(LocateXStateFeature)(&c, XSTATE_LEGACY_FLOATING_POINT, &len);
    CHECK(p == (PVOID)((BYTE *)&c + offsetof(CONTEXT, FltSave)));
    EXPECT_EQ(len, 0xa0);
    p = U(LocateXStateFeature)(&c, XSTATE_LEGACY_SSE, &len);
    CHECK(p == (PVOID)&c.Xmm0);
    EXPECT_EQ(len, 0x100);
    p = U(LocateXStateFeature)(&c, XSTATE_LEGACY_SSE, NULL);
    CHECK(p == (PVOID)&c.Xmm0);
}
TEST(cx_locate_avx_needs_xstate_context) {
    REQUIRE(U(LocateXStateFeature));
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_ALL;
    DWORD len = 0x1234;
    PVOID p = U(LocateXStateFeature)(&c, XSTATE_AVX, &len);
    CHECK(p == NULL);
    p = U(LocateXStateFeature)(&c, 63, &len);
    CHECK(p == NULL);
    c.ContextFlags = 0;
    CHECK(U(LocateXStateFeature)(&c, 0, &len) == NULL);
    CHECK(U(LocateXStateFeature)(&c, 1, &len) == NULL);
}
TEST(cx_locate_in_xstate_context) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(LocateXStateFeature));
    DWORD xs = cx_len(CONTEXT_ALL | CONTEXT_XSTATE, NULL, NULL);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, xs);
    PCONTEXT c = NULL; DWORD len = xs;
    if (!U(InitializeContext)(buf, CONTEXT_ALL | CONTEXT_XSTATE, &c, &len)) { CHECK(0); HeapFree(GetProcessHeap(), 0, buf); return; }
    DWORD l = 0;
    PVOID sse = U(LocateXStateFeature)(c, XSTATE_LEGACY_SSE, &l);
    CHECK(sse == (PVOID)&c->Xmm0); EXPECT_EQ(l, 0x100);
    DWORD64 feat = U(GetEnabledXStateFeatures)();
    if (feat & XSTATE_MASK_AVX) {
        PVOID ymmh = U(LocateXStateFeature)(c, XSTATE_AVX, &l);
        CHECK(ymmh != NULL);
        if (ymmh) {
            CHECK((BYTE *)ymmh > (BYTE *)c && (BYTE *)ymmh + l <= buf + xs);
            CHECK(((ULONG_PTR)ymmh & 15) == 0);
            EXPECT_EQ(l, 0x100);
        }
    }
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(cx_set_features_mask_plain) {
    REQUIRE(U(SetXStateFeaturesMask));
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_CONTROL;
    CHECK(U(SetXStateFeaturesMask)(&c, 0));
    EXPECT_EQ(c.ContextFlags, CONTEXT_CONTROL);
    CHECK(U(SetXStateFeaturesMask)(&c, 3));
    CHECK((c.ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT);
    SET_ERR(0);
    CHECK(!U(SetXStateFeaturesMask)(&c, 4));   /* AVX needs an XSTATE context */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(SetXStateFeaturesMask)(&c, 0x8000000000000000ULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    memset(&c, 0, sizeof c);
    SET_ERR(0);
    CHECK(!U(SetXStateFeaturesMask)(&c, 3));
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
}
TEST(cx_set_features_mask_xstate) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(SetXStateFeaturesMask));
    DWORD64 feat = U(GetEnabledXStateFeatures)();
    if (!(feat & XSTATE_MASK_AVX)) { t_note("AVX not enabled"); return; }
    DWORD xs = cx_len(CONTEXT_ALL | CONTEXT_XSTATE, NULL, NULL);
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, xs);
    PCONTEXT c = NULL; DWORD len = xs;
    if (!U(InitializeContext)(buf, CONTEXT_ALL | CONTEXT_XSTATE, &c, &len)) { CHECK(0); HeapFree(GetProcessHeap(), 0, buf); return; }
    CHECK(U(SetXStateFeaturesMask)(c, 7));
    DWORD64 m = 0;
    CHECK(U(GetXStateFeaturesMask)(c, &m));
    CHECK((m & 7) == 7);
    CHECK(U(SetXStateFeaturesMask)(c, 3));
    CHECK(U(GetXStateFeaturesMask)(c, &m));
    CHECK((m & 4) == 0);
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(cx_copy_context_integer) {
    REQUIRE(U(CopyContext));
    __attribute__((aligned(16))) CONTEXT src, dst;
    memset(&src, 0, sizeof src); memset(&dst, 0, sizeof dst);
    src.ContextFlags = CONTEXT_FULL; dst.ContextFlags = CONTEXT_FULL;
    src.Rax = 0x1111; src.Rbx = 0x2222; src.R15 = 0x3333; src.Rip = 0x4444; src.Rsp = 0x5555;
    dst.Rax = 0xAAAA; dst.Rip = 0xBBBB; dst.Rsp = 0xCCCC;
    SET_ERR(0);
    BOOL r = U(CopyContext)(&dst, CONTEXT_INTEGER, &src);
    CHECK(r);
    EXPECT_EQ(dst.Rax, 0x1111); EXPECT_EQ(dst.Rbx, 0x2222); EXPECT_EQ(dst.R15, 0x3333);
    EXPECT_EQ(dst.Rip, 0xBBBB); EXPECT_EQ(dst.Rsp, 0xCCCC);
    r = U(CopyContext)(&dst, CONTEXT_CONTROL, &src);
    CHECK(r);
    EXPECT_EQ(dst.Rip, 0x4444); EXPECT_EQ(dst.Rsp, 0x5555);
}
TEST(cx_copy_context_float) {
    REQUIRE(U(CopyContext));
    __attribute__((aligned(16))) CONTEXT src, dst;
    memset(&src, 0, sizeof src); memset(&dst, 0, sizeof dst);
    src.ContextFlags = CONTEXT_ALL; dst.ContextFlags = CONTEXT_ALL;
    src.Xmm3.Low = 0x1234567890ABCDEFULL; src.Xmm3.High = 0x0FEDCBA987654321ULL; src.MxCsr = 0x1F80;
    CHECK(U(CopyContext)(&dst, CONTEXT_FLOATING_POINT, &src));
    EXPECT_EQ(dst.Xmm3.Low, 0x1234567890ABCDEFULL);
    EXPECT_EQ(dst.Xmm3.High, 0x0FEDCBA987654321ULL);
    t_note("MxCsr copied: %d", dst.MxCsr == 0x1F80);
    EXPECT_EQ(dst.Rax, 0);
}
TEST(cx_copy_context_invalid) {
    REQUIRE(U(CopyContext));
    __attribute__((aligned(16))) CONTEXT src, dst;
    memset(&src, 0, sizeof src); memset(&dst, 0, sizeof dst);
    src.ContextFlags = CONTEXT_FULL; dst.ContextFlags = CONTEXT_FULL;
    src.Rax = 0x77;
    SET_ERR(0);
    BOOL r = U(CopyContext)(&dst, 0, &src);
    t_note("CopyContext(flags 0) -> %d err %lu", r, GetLastError());
    dst.Rax = 0;
    r = U(CopyContext)(&dst, 0xFFFFFFFF, &src);
    CHECK(!r);
    EXPECT_EQ(dst.Rax, 0);
}

/* ======================= IsApiSetImplemented ======================= */
TEST(apiset_known_present) {
    REQUIRE(UN(IsApiSetImplemented));
    static const char *n[] = {
        "api-ms-win-core-file-l1-1-0", "api-ms-win-core-processthreads-l1-1-0", "api-ms-win-core-memory-l1-1-0",
        "api-ms-win-core-synch-l1-1-0", "api-ms-win-core-libraryloader-l1-1-0", "api-ms-win-core-handle-l1-1-0",
        "api-ms-win-core-heap-l1-1-0", "api-ms-win-core-errorhandling-l1-1-0", "api-ms-win-core-console-l1-1-0",
        "api-ms-win-core-sysinfo-l1-1-0", "api-ms-win-core-timezone-l1-1-0", "api-ms-win-core-localization-l1-2-0",
        "api-ms-win-core-datetime-l1-1-0", "api-ms-win-core-interlocked-l1-1-0", "api-ms-win-core-namedpipe-l1-1-0",
        "api-ms-win-core-string-l1-1-0", "api-ms-win-core-util-l1-1-0", "api-ms-win-core-profile-l1-1-0",
        "api-ms-win-core-rtlsupport-l1-1-0", "api-ms-win-core-threadpool-l1-1-0", "api-ms-win-core-job-l1-1-0",
        "api-ms-win-core-path-l1-1-0", "api-ms-win-core-synch-l1-2-0", "api-ms-win-core-memory-l1-1-1",
        "api-ms-win-core-file-l1-2-0", "api-ms-win-security-base-l1-1-0",
    };
    for (size_t i = 0; i < ARRAYSIZE(n); i++) {
        BOOL r = UN(IsApiSetImplemented)(n[i]);
        CHECKF(r == TRUE, "%s", n[i]);
    }
}
TEST(apiset_absent) {
    REQUIRE(UN(IsApiSetImplemented));
    static const char *n[] = {
        "api-ms-win-k32t-nonexistent-l1-1-0",
        "api-ms-win-core-k32tfake-l1-1-0", "k32t-nonexistent", "", "api-ms-", "ext-ms-win-k32t-fake-l1-1-0",
        "not an api set name",
    };
    for (size_t i = 0; i < ARRAYSIZE(n); i++) {
        BOOL r = UN(IsApiSetImplemented)(n[i]);
        CHECKF(r == FALSE, "'%s' -> %d", n[i], r);
    }
}
TEST(apiset_case_and_suffix_probe) {
    REQUIRE(UN(IsApiSetImplemented));
    t_note("file-l1-99-0 -> %d", UN(IsApiSetImplemented)("api-ms-win-core-file-l1-99-0"));
    t_note("file-l9-1-0 -> %d", UN(IsApiSetImplemented)("api-ms-win-core-file-l9-1-0"));
    t_note("memory-l1-1-9 -> %d", UN(IsApiSetImplemented)("api-ms-win-core-memory-l1-1-9"));
    t_note("upper-case name -> %d", UN(IsApiSetImplemented)("API-MS-WIN-CORE-FILE-L1-1-0"));
    t_note("name with .dll -> %d", UN(IsApiSetImplemented)("api-ms-win-core-file-l1-1-0.dll"));
    t_note("ext-ms-win-ntuser-window-l1-1-0 -> %d", UN(IsApiSetImplemented)("ext-ms-win-ntuser-window-l1-1-0"));
    t_note("ext-ms-win-kernel32-package-l1-1-0 -> %d", UN(IsApiSetImplemented)("ext-ms-win-kernel32-package-l1-1-0"));
    t_note("api-ms-win-core-wow64-l1-1-0 -> %d", UN(IsApiSetImplemented)("api-ms-win-core-wow64-l1-1-0"));
}
TEST(apiset_long_name) {
    REQUIRE(UN(IsApiSetImplemented));
    char big[4096]; memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
    CHECK(UN(IsApiSetImplemented)(big) == FALSE);
    memcpy(big, "api-ms-", 7);
    CHECK(UN(IsApiSetImplemented)(big) == FALSE);
}
TEST(apiset_repeat_stable) {
    REQUIRE(UN(IsApiSetImplemented));
    for (int i = 0; i < 500; i++) {
        if (!UN(IsApiSetImplemented)("api-ms-win-core-file-l1-1-0") || UN(IsApiSetImplemented)("api-ms-win-k32t-none-l1-1-0")) { CHECK(0); break; }
    }
}
TEST(apiset_matches_loader) {
    REQUIRE(UN(IsApiSetImplemented));
    /* every implemented contract must be loadable by name */
    HMODULE m = LoadLibraryA("api-ms-win-core-file-l1-1-0.dll");
    CHECK(m != NULL);
    if (m) {
        CHECK(GetProcAddress(m, "CreateFileW") != NULL);
        FreeLibrary(m);
    }
    SetLastError(0);
    m = LoadLibraryA("api-ms-win-k32t-none-l1-1-0.dll");
    CHECK(m == NULL);
    if (m) FreeLibrary(m);
}

/* ======================= Internet time helpers ======================= */
UDECL(BOOL, InternetTimeFromSystemTimeA, (const SYSTEMTIME *, DWORD, LPSTR, DWORD));
UDECL(BOOL, InternetTimeFromSystemTimeW, (const SYSTEMTIME *, DWORD, LPWSTR, DWORD));
UDECL(BOOL, InternetTimeToSystemTimeA, (LPCSTR, SYSTEMTIME *, DWORD));
UDECL(BOOL, InternetTimeToSystemTimeW, (LPCWSTR, SYSTEMTIME *, DWORD));
static SYSTEMTIME st_(WORD y, WORD mo, WORD dow, WORD d, WORD h, WORD mi, WORD s, WORD ms) {
    SYSTEMTIME t = { y, mo, dow, d, h, mi, s, ms }; return t;
}
TEST(inet_from_systemtime_basic) {
    REQUIRE(UN(InternetTimeFromSystemTimeA));
    SYSTEMTIME t = st_(1994, 11, 0, 6, 8, 49, 37, 0);
    char b[64]; memset(b, 'X', sizeof b);
    CHECK(UN(InternetTimeFromSystemTimeA)(&t, 0, b, 30));
    EXPECT_STR(b, "Sun, 06 Nov 1994 08:49:37 GMT");
    EXPECT_EQ(b[30], 'X');
}
TEST(inet_from_systemtime_table) {
    REQUIRE(UN(InternetTimeFromSystemTimeA));
    struct { SYSTEMTIME t; const char *s; } c[] = {
        { st_(2000, 1, 6, 1, 0, 0, 0, 0), "Sat, 01 Jan 2000 00:00:00 GMT" },
        { st_(2024, 2, 4, 29, 23, 59, 59, 999), "Thu, 29 Feb 2024 23:59:59 GMT" },
        { st_(2021, 12, 5, 31, 12, 30, 15, 0), "Fri, 31 Dec 2021 12:30:15 GMT" },
        { st_(1999, 7, 4, 4, 9, 8, 7, 0), "Thu, 04 Jul 1999 09:08:07 GMT" },
        { st_(2012, 5, 3, 2, 1, 2, 3, 0), "Wed, 02 May 2012 01:02:03 GMT" },
    };
    for (size_t i = 0; i < ARRAYSIZE(c); i++) {
        char b[64];
        CHECKF(UN(InternetTimeFromSystemTimeA)(&c[i].t, 0, b, sizeof b), "case %zu err %lu", i, GetLastError());
        CHECKF(!strcmp(b, c[i].s), "case %zu got '%s'", i, b);
    }
}
TEST(inet_from_systemtime_weekday_recomputed) {
    REQUIRE(UN(InternetTimeFromSystemTimeA));
    SYSTEMTIME t = st_(1994, 11, 7, 6, 8, 49, 37, 0);   /* wDayOfWeek out of range: derived from the date */
    char b[64];
    CHECK(UN(InternetTimeFromSystemTimeA)(&t, 0, b, sizeof b));
    EXPECT_STR(b, "Sun, 06 Nov 1994 08:49:37 GMT");
    t.wDayOfWeek = 0xFFFF;
    CHECK(UN(InternetTimeFromSystemTimeA)(&t, 0, b, sizeof b));
    EXPECT_STR(b, "Sun, 06 Nov 1994 08:49:37 GMT");
}
TEST(inet_from_systemtime_buffer_sizes) {
    REQUIRE(UN(InternetTimeFromSystemTimeA));
    SYSTEMTIME t = st_(1994, 11, 0, 6, 8, 49, 37, 0);
    char b[64];
    for (DWORD cb = 0; cb < 30; cb++) {
        SET_ERR(0);
        CHECKF(!UN(InternetTimeFromSystemTimeA)(&t, 0, b, cb), "cb %lu", cb);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    CHECK(UN(InternetTimeFromSystemTimeA)(&t, 0, b, 30));
    CHECK(UN(InternetTimeFromSystemTimeA)(&t, 0, b, 31));
    SET_ERR(0);
    CHECK(!UN(InternetTimeFromSystemTimeA)(&t, 0, NULL, 30));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!UN(InternetTimeFromSystemTimeA)(NULL, 0, b, 30));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(inet_from_systemtime_bad_rfc_and_time) {
    REQUIRE(UN(InternetTimeFromSystemTimeA));
    SYSTEMTIME t = st_(1994, 11, 0, 6, 8, 49, 37, 0);
    char b[64];
    static const DWORD rfc[] = { 1, 2, 0x10, 0xFFFFFFFF };
    for (size_t i = 0; i < ARRAYSIZE(rfc); i++) {
        SET_ERR(0);
        CHECKF(!UN(InternetTimeFromSystemTimeA)(&t, rfc[i], b, 30), "rfc %lu", rfc[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    SYSTEMTIME bad[] = { st_(1994, 13, 0, 6, 8, 49, 37, 0), st_(1994, 0, 0, 6, 8, 49, 37, 0), st_(1994, 2, 0, 30, 8, 49, 37, 0),
        st_(1994, 11, 0, 0, 8, 49, 37, 0), st_(1994, 11, 0, 6, 24, 49, 37, 0), st_(1994, 11, 0, 6, 8, 60, 37, 0),
        st_(1994, 11, 0, 6, 8, 49, 60, 0), st_(1994, 11, 0, 6, 8, 49, 37, 1000), st_(1500, 11, 0, 6, 8, 49, 37, 0) };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        SET_ERR(0);
        BOOL r = UN(InternetTimeFromSystemTimeA)(&bad[i], 0, b, 30);
        CHECKF(!r, "invalid time %zu accepted", i);
        if (!r) EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(inet_from_systemtime_wide) {
    REQUIRE(UN(InternetTimeFromSystemTimeW));
    SYSTEMTIME t = st_(1994, 11, 0, 6, 8, 49, 37, 0);
    WCHAR w[64];
    CHECK(UN(InternetTimeFromSystemTimeW)(&t, 0, w, 128));
    EXPECT_WSTR(w, L"Sun, 06 Nov 1994 08:49:37 GMT");
    SET_ERR(0);
    CHECK(!UN(InternetTimeFromSystemTimeW)(&t, 0, w, 59));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    CHECK(!UN(InternetTimeFromSystemTimeW)(&t, 0, NULL, 128));
    CHECK(GetLastError() != 0);
    /* byte count: exactly 60 bytes is rejected even though 30 WCHARs fit (the conversion needs headroom) */
    SET_ERR(0);
    BOOL r = UN(InternetTimeFromSystemTimeW)(&t, 0, w, 60);
    t_note("Wide, 60-byte buffer -> %d (err %lu)", r, r ? 0 : GetLastError());
    r = UN(InternetTimeFromSystemTimeW)(&t, 0, w, 61);
    CHECK(r);
}
TEST(inet_to_systemtime_rfc1123) {
    REQUIRE(UN(InternetTimeToSystemTimeA));
    SYSTEMTIME t; memset(&t, 0xCC, sizeof t);
    CHECK(UN(InternetTimeToSystemTimeA)("Sun, 06 Nov 1994 08:49:37 GMT", &t, 0));
    EXPECT_EQ(t.wYear, 1994); EXPECT_EQ(t.wMonth, 11); EXPECT_EQ(t.wDay, 6);
    EXPECT_EQ(t.wHour, 8); EXPECT_EQ(t.wMinute, 49); EXPECT_EQ(t.wSecond, 37);
    EXPECT_EQ(t.wDayOfWeek, 0); EXPECT_EQ(t.wMilliseconds, 0);
}
TEST(inet_to_systemtime_other_formats) {
    REQUIRE(UN(InternetTimeToSystemTimeA));
    SYSTEMTIME t;
    memset(&t, 0, sizeof t);
    BOOL r = UN(InternetTimeToSystemTimeA)("Sunday, 06-Nov-94 08:49:37 GMT", &t, 0);
    t_note("RFC 850 form -> %d (%04u-%02u-%02u)", r, t.wYear, t.wMonth, t.wDay);
    if (r) { EXPECT_EQ(t.wMonth, 11); EXPECT_EQ(t.wDay, 6); EXPECT_EQ(t.wHour, 8); }
    memset(&t, 0, sizeof t);
    r = UN(InternetTimeToSystemTimeA)("Sun Nov  6 08:49:37 1994", &t, 0);
    t_note("asctime form -> %d (%04u-%02u-%02u)", r, t.wYear, t.wMonth, t.wDay);
    if (r) { EXPECT_EQ(t.wYear, 1994); EXPECT_EQ(t.wMonth, 11); EXPECT_EQ(t.wDay, 6); }
}
TEST(inet_to_systemtime_invalid) {
    REQUIRE(UN(InternetTimeToSystemTimeA));
    SYSTEMTIME t;
    SET_ERR(0);
    CHECK(!UN(InternetTimeToSystemTimeA)(NULL, &t, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(!UN(InternetTimeToSystemTimeA)("Sun, 06 Nov 1994 08:49:37 GMT", NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(!UN(InternetTimeToSystemTimeA)("Sun, 06 Nov 1994 08:49:37 GMT", &t, 1)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(!UN(InternetTimeToSystemTimeA)("Sun, 06 Nov 1994 08:49:37 GMT", &t, 0xFFFFFFFF)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    static const char *bad[] = { "", "garbage", "Sun, 32 Nov 1994 08:49:37 GMT", "Sun, 06 Xyz 1994 08:49:37 GMT", "Sun, 06 Nov 1994 25:49:37 GMT", "12345" };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        SET_ERR(0);
        BOOL r = UN(InternetTimeToSystemTimeA)(bad[i], &t, 0);
        CHECKF(!r, "'%s' accepted", bad[i]);
        if (!r) EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(inet_to_systemtime_wide_and_roundtrip) {
    REQUIRE(UN(InternetTimeToSystemTimeW)); REQUIRE(UN(InternetTimeFromSystemTimeA));
    SYSTEMTIME t; memset(&t, 0, sizeof t);
    CHECK(UN(InternetTimeToSystemTimeW)(L"Sun, 06 Nov 1994 08:49:37 GMT", &t, 0));
    EXPECT_EQ(t.wYear, 1994); EXPECT_EQ(t.wDay, 6); EXPECT_EQ(t.wSecond, 37);
    SET_ERR(0);
    CHECK(!UN(InternetTimeToSystemTimeW)(NULL, &t, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(!UN(InternetTimeToSystemTimeW)(L"Sun, 06 Nov 1994 08:49:37 GMT", NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(!UN(InternetTimeToSystemTimeW)(L"nonsense", &t, 0));
    /* round trip over a range of dates */
    for (int i = 0; i < 400; i++) {
        SYSTEMTIME a = st_((WORD)(1980 + i % 60), (WORD)(1 + i % 12), 9, (WORD)(1 + i % 28), (WORD)(i % 24), (WORD)(i % 60), (WORD)((i * 7) % 60), 0), b;
        char s[64];
        if (!UN(InternetTimeFromSystemTimeA)(&a, 0, s, sizeof s)) { CHECK(0); break; }
        memset(&b, 0, sizeof b);
        if (!UN(InternetTimeToSystemTimeA)(s, &b, 0)) { CHECKF(0, "parse '%s'", s); break; }
        if (a.wYear != b.wYear || a.wMonth != b.wMonth || a.wDay != b.wDay || a.wHour != b.wHour || a.wMinute != b.wMinute || a.wSecond != b.wSecond) { CHECKF(0, "roundtrip '%s'", s); break; }
    }
}

/* ======================= Remote pointer encoding ======================= */
UDECL(HRESULT, EncodeRemotePointer, (HANDLE, PVOID, PVOID *));
UDECL(HRESULT, DecodeRemotePointer, (HANDLE, PVOID, PVOID *));
TEST(remoteptr_roundtrip_self) {
    REQUIRE(UN(EncodeRemotePointer)); REQUIRE(UN(DecodeRemotePointer));
    static const ULONG_PTR vals[] = { 0x1000, 0x7FF612340000ULL, 0xFFFFFFFFFFFFFFFFULL, 0x1, 0x8000000000000000ULL, 0x123456789ABCDEF0ULL };
    for (size_t i = 0; i < ARRAYSIZE(vals); i++) {
        PVOID e = NULL, d = NULL;
        HRESULT h1 = UN(EncodeRemotePointer)(GetCurrentProcess(), (PVOID)vals[i], &e);
        HRESULT h2 = UN(DecodeRemotePointer)(GetCurrentProcess(), e, &d);
        CHECKF(h1 == S_OK && h2 == S_OK, "val %zu hr 0x%lx 0x%lx", i, (ULONG)h1, (ULONG)h2);
        CHECKF(d == (PVOID)vals[i], "val %zu decoded %p", i, d);
    }
}
TEST(remoteptr_matches_local) {
    REQUIRE(UN(EncodeRemotePointer)); REQUIRE(U(EncodePointer)); REQUIRE(U(DecodePointer));
    PVOID p = (PVOID)(ULONG_PTR)0x7FF6DEADBEEFULL, e = NULL;
    CHECK(UN(EncodeRemotePointer)(GetCurrentProcess(), p, &e) == S_OK);
    EXPECT_EQ(e, U(EncodePointer)(p));
    CHECK(U(DecodePointer)(e) == p);
    HANDLE real = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (real) {
        PVOID e2 = NULL;
        CHECK(UN(EncodeRemotePointer)(real, p, &e2) == S_OK);
        EXPECT_EQ(e2, e);
        CloseHandle(real);
    }
}
TEST(remoteptr_other_process_differs) {
    REQUIRE(UN(EncodeRemotePointer));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("kbx_idle", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { CHECK(0); return; }
    PVOID p = (PVOID)(ULONG_PTR)0x7FF6DEADBEEFULL, e1 = NULL, e2 = NULL, d = NULL;
    HRESULT hr = UN(EncodeRemotePointer)(h, p, &e1);
    CHECK(hr == S_OK);
    CHECK(UN(EncodeRemotePointer)(GetCurrentProcess(), p, &e2) == S_OK);
    CHECK(e1 != e2);   /* per-process cookie */
    CHECK(UN(DecodeRemotePointer)(h, e1, &d) == S_OK);
    CHECK(d == p);
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(remoteptr_bad_handles) {
    REQUIRE(UN(EncodeRemotePointer)); REQUIRE(UN(DecodeRemotePointer));
    PVOID o = (PVOID)(ULONG_PTR)0x1234;
    HRESULT hr = UN(EncodeRemotePointer)(NULL, (PVOID)0x1000, &o);
    CHECK(FAILED(hr)); EXPECT_EQ(o, 0x1234);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    hr = UN(EncodeRemotePointer)(ev, (PVOID)0x1000, &o);
    CHECK(FAILED(hr));
    hr = UN(DecodeRemotePointer)(ev, (PVOID)0x1000, &o);
    CHECK(FAILED(hr));
    hr = UN(DecodeRemotePointer)(NULL, (PVOID)0x1000, &o);
    CHECK(FAILED(hr));
    CloseHandle(ev);
}

/* ======================= ANSI identity helpers (kernelbase A variants) ======================= */
UDECL(LONG, PackageFamilyNameFromFullNameA, (PCSTR, UINT32 *, PSTR));
UDECL(LONG, PackageNameAndPublisherIdFromFamilyNameA, (PCSTR, UINT32 *, PSTR, UINT32 *, PSTR));
UDECL(LONG, ParseApplicationUserModelIdA, (PCSTR, UINT32 *, PSTR, UINT32 *, PSTR));
UDECL(LONG, FormatApplicationUserModelIdA, (PCSTR, PCSTR, UINT32 *, PSTR));
UDECL(LONG, VerifyPackageFullNameA, (PCSTR));
UDECL(LONG, VerifyPackageFamilyNameA, (PCSTR));
UDECL(LONG, VerifyApplicationUserModelIdA, (PCSTR));
UDECL(LONG, VerifyPackageRelativeApplicationIdA, (PCSTR));
UDECL(LONG, PackageIdFromFullNameA, (PCSTR, UINT32, UINT32 *, BYTE *));
UDECL(LONG, PackageFamilyNameFromFullName, (PCWSTR, UINT32 *, PWSTR));
static void to_w(WCHAR *w, const char *a) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, 512); }
TEST(amA_family_from_full_parity) {
    REQUIRE(UN(PackageFamilyNameFromFullNameA)); REQUIRE(UN(PackageFamilyNameFromFullName));
    static const char *in[] = { "Microsoft.WindowsCalculator_11.2.0.0_x64__8wekyb3d8bbwe", "Contoso.App_1.0.0.0_neutral__8wekyb3d8bbwe",
        "bad", "", "Name_1.0.0.0_x64__8wekyb3d8bbw" };
    for (size_t i = 0; i < ARRAYSIZE(in); i++) {
        char a[100]; WCHAR w[100], wi[512]; UINT32 la = 100, lw = 100;
        LONG ra = UN(PackageFamilyNameFromFullNameA)(in[i], &la, a);
        to_w(wi, in[i]);
        LONG rw = UN(PackageFamilyNameFromFullName)(wi, &lw, w);
        CHECKF(ra == rw, "case %zu: A=%ld W=%ld", i, ra, rw);
        if (ra == 0 && rw == 0) { CHECK(la == lw); WCHAR cw[100]; to_w(cw, a); CHECK(!lstrcmpW(cw, w)); }
    }
    UINT32 len = 0;
    EQ(UN(PackageFamilyNameFromFullNameA)("Microsoft.WindowsCalculator_11.2.0.0_x64__8wekyb3d8bbwe", &len, NULL), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(len, 42);
    EQ(UN(PackageFamilyNameFromFullNameA)(NULL, &len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(amA_namepub_parity) {
    REQUIRE(UN(PackageNameAndPublisherIdFromFamilyNameA));
    char n[64], p[32]; UINT32 nl = 64, pl = 32;
    EQ(UN(PackageNameAndPublisherIdFromFamilyNameA)("Microsoft.WindowsCalculator_8wekyb3d8bbwe", &nl, n, &pl, p), 0);
    EXPECT_STR(n, "Microsoft.WindowsCalculator"); EXPECT_STR(p, "8wekyb3d8bbwe");
    EXPECT_EQ(nl, 28); EXPECT_EQ(pl, 14);
    nl = 0; pl = 0;
    EQ(UN(PackageNameAndPublisherIdFromFamilyNameA)("Microsoft.WindowsCalculator_8wekyb3d8bbwe", &nl, NULL, &pl, NULL), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(nl, 28); EXPECT_EQ(pl, 14);
    nl = 64; pl = 32;
    EQ(UN(PackageNameAndPublisherIdFromFamilyNameA)("garbage", &nl, n, &pl, p), ERROR_INVALID_PARAMETER);
    EQ(UN(PackageNameAndPublisherIdFromFamilyNameA)(NULL, &nl, n, &pl, p), ERROR_INVALID_PARAMETER);
}
TEST(amA_parse_format_aumid) {
    REQUIRE(UN(ParseApplicationUserModelIdA)); REQUIRE(UN(FormatApplicationUserModelIdA));
    char f[100], a[100]; UINT32 fl = 100, al = 100;
    EQ(UN(ParseApplicationUserModelIdA)("Microsoft.WindowsCalculator_8wekyb3d8bbwe!App", &fl, f, &al, a), 0);
    EXPECT_STR(f, "Microsoft.WindowsCalculator_8wekyb3d8bbwe"); EXPECT_STR(a, "App");
    EXPECT_EQ(fl, 42); EXPECT_EQ(al, 4);
    char o[200]; UINT32 ol = 200;
    EQ(UN(FormatApplicationUserModelIdA)(f, a, &ol, o), 0);
    EXPECT_STR(o, "Microsoft.WindowsCalculator_8wekyb3d8bbwe!App");
    EXPECT_EQ(ol, 45);
    ol = 0;
    EQ(UN(FormatApplicationUserModelIdA)(f, a, &ol, NULL), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(ol, 45);
    fl = 100; al = 100;
    EQ(UN(ParseApplicationUserModelIdA)("nonsense", &fl, f, &al, a), ERROR_INVALID_PARAMETER);
    EQ(UN(FormatApplicationUserModelIdA)(NULL, "App", &ol, o), ERROR_INVALID_PARAMETER);
}
TEST(amA_verify_variants) {
    REQUIRE(UN(VerifyPackageFullNameA)); REQUIRE(UN(VerifyPackageFamilyNameA));
    REQUIRE(UN(VerifyApplicationUserModelIdA)); REQUIRE(UN(VerifyPackageRelativeApplicationIdA));
    EQ(UN(VerifyPackageFullNameA)("Microsoft.WindowsCalculator_11.2.0.0_x64__8wekyb3d8bbwe"), 0);
    EQ(UN(VerifyPackageFullNameA)("Microsoft.WindowsCalculator_8wekyb3d8bbwe"), ERROR_INVALID_PARAMETER);
    EQ(UN(VerifyPackageFullNameA)(NULL), ERROR_INVALID_PARAMETER);
    EQ(UN(VerifyPackageFamilyNameA)("Microsoft.WindowsCalculator_8wekyb3d8bbwe"), 0);
    EQ(UN(VerifyPackageFamilyNameA)("Microsoft.WindowsCalculator_8wekyb3d8bbw"), ERROR_INVALID_PARAMETER);
    EQ(UN(VerifyPackageFamilyNameA)(NULL), ERROR_INVALID_PARAMETER);
    EQ(UN(VerifyApplicationUserModelIdA)("Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"), 0);
    EQ(UN(VerifyApplicationUserModelIdA)("Microsoft.WindowsCalculator_8wekyb3d8bbwe"), ERROR_INVALID_PARAMETER);
    EQ(UN(VerifyApplicationUserModelIdA)(NULL), ERROR_INVALID_PARAMETER);
    EQ(UN(VerifyPackageRelativeApplicationIdA)("App"), 0);
    EQ(UN(VerifyPackageRelativeApplicationIdA)("A B"), ERROR_INVALID_PARAMETER);
    EQ(UN(VerifyPackageRelativeApplicationIdA)(NULL), ERROR_INVALID_PARAMETER);
}
TEST(amA_id_from_full_name) {
    REQUIRE(UN(PackageIdFromFullNameA));
    UINT32 len = 0;
    EQ(UN(PackageIdFromFullNameA)("Microsoft.WindowsCalculator_11.2.0.0_x64__8wekyb3d8bbwe", 0, &len, NULL), ERROR_INSUFFICIENT_BUFFER);
    CHECK(len >= sizeof(PACKAGE_ID));
    BYTE *b = HeapAlloc(GetProcessHeap(), 0, len);
    UINT32 l2 = len;
    EQ(UN(PackageIdFromFullNameA)("Microsoft.WindowsCalculator_11.2.0.0_x64__8wekyb3d8bbwe", 0, &l2, b), 0);
    PACKAGE_ID *id = (PACKAGE_ID *)b;
    EXPECT_EQ(id->processorArchitecture, PROCESSOR_ARCHITECTURE_AMD64);
    EXPECT_EQ(id->version.Major, 11);
    CHECK(id->name && !strcmp((char *)id->name, "Microsoft.WindowsCalculator"));
    CHECK(id->publisherId && !strcmp((char *)id->publisherId, "8wekyb3d8bbwe"));
    HeapFree(GetProcessHeap(), 0, b);
    EQ(UN(PackageIdFromFullNameA)(NULL, 0, &len, NULL), ERROR_INVALID_PARAMETER);
}

/* ======================= Export probes ======================= */
#define PROBE_LIST_K32(X) \
    X(GetCurrentPackageId) X(GetCurrentPackageFullName) X(GetCurrentPackageFamilyName) X(GetCurrentPackagePath) \
    X(GetCurrentPackageInfo) X(GetCurrentApplicationUserModelId) X(GetPackageId) X(GetPackageFullName) \
    X(GetPackageFamilyName) X(GetPackagePath) X(GetPackagePathByFullName) X(GetPackageApplicationIds) \
    X(GetApplicationUserModelId) X(PackageFamilyNameFromFullName) X(PackageFullNameFromId) X(PackageIdFromFullName) \
    X(PackageNameAndPublisherIdFromFamilyName) X(ParseApplicationUserModelId) X(FormatApplicationUserModelId) \
    X(VerifyPackageFullName) X(VerifyPackageFamilyName) X(VerifyPackageId) X(VerifyApplicationUserModelId) \
    X(OpenPackageInfoByFullName) X(ClosePackageInfo) X(GetPackageInfo) X(FindPackagesByPackageFamily) \
    X(GetStagedPackagePathByFullName) X(GetStagedPackageOrigin) X(GetPackagesByPackageFamily) X(PackageFamilyNameFromId) \
    X(GetApplicationUserModelIdFromToken) X(GetPackageFamilyNameFromToken) X(GetPackageFullNameFromToken) \
    X(GetCurrentPackageInfo2) X(GetCurrentPackagePath2) X(GetPackagePathByFullName2) X(GetStagedPackagePathByFullName2) \
    X(AppPolicyGetProcessTerminationMethod) X(AppPolicyGetThreadInitializationType) X(AppPolicyGetWindowingModel) \
    X(AppPolicyGetShowDeveloperDiagnostic) X(AppPolicyGetMediaFoundationCodecLoading) X(AppPolicyGetClrCompat) \
    X(AppPolicyGetCreateFileAccess) X(AppPolicyGetLifecycleManagement) \
    X(CreateFile2) X(CopyFile2) X(PowerCreateRequest) X(PowerSetRequest) X(PowerClearRequest) \
    X(GetProcessMitigationPolicy) X(SetProcessMitigationPolicy) X(IsEnclaveTypeSupported) X(CreateEnclave) \
    X(InitializeEnclave) X(LoadEnclaveData) X(CallEnclave) X(TerminateEnclave) X(DeleteEnclave) \
    X(GetProcessDefaultCpuSets) X(SetProcessDefaultCpuSets) X(GetThreadSelectedCpuSets) X(SetThreadSelectedCpuSets) \
    X(GetSystemCpuSetInformation) X(GetNamedPipeAttribute) X(SetNamedPipeAttribute) X(InitializeContext) \
    X(InitializeContext2) X(GetXStateFeaturesMask) X(LocateXStateFeature) X(SetXStateFeaturesMask) X(CopyContext) \
    X(GetEnabledXStateFeatures) X(SetProcessDynamicEHContinuationTargets) X(ApplicationRecoveryInProgress) \
    X(ApplicationRecoveryFinished) X(RegisterApplicationRecoveryCallback) X(UnregisterApplicationRecoveryCallback) \
    X(GetApplicationRecoveryCallback) X(RegisterApplicationRestart) X(UnregisterApplicationRestart) X(GetApplicationRestartSettings) \
    X(EnableThreadProfiling) X(DisableThreadProfiling) X(QueryThreadProfiling) X(ReadThreadProfilingData) \
    X(GetSystemFileCacheSize) X(SetSystemFileCacheSize) X(LoadEnclaveImageA) X(LoadEnclaveImageW)
#define PROBE_ONE(n) #n,
TEST(probe_kernel32_exports) {
    static const char *n[] = { PROBE_LIST_K32(PROBE_ONE) };
    for (size_t i = 0; i < ARRAYSIZE(n); i++) {
        CHECKF(t_proc_k32(n[i]) != NULL || t_proc_kb(n[i]) != NULL, "%s not resolvable", n[i]);
    }
}
TEST(probe_kernelbase_only_appmodel) {
    static const char *n[] = {
        "AppXFreeMemory", "AppXGetApplicationData", "AppXGetDevelopmentMode", "AppXGetOSMinVersion", "AppXGetPackageCapabilities",
        "AppXGetPackageSid", "AppXLookupDisplayName", "AppXLookupMoniker", "AppXPostSuccessExtension", "AppXPreCreationExtension",
        "AppXReleaseAppXContext", "AppXUpdatePackageCapabilities", "AppContainerDeriveSidFromMoniker", "AppContainerFreeMemory",
        "AppContainerLookupDisplayNameMrtReference", "AppContainerLookupMoniker", "AppContainerRegisterSid", "AppContainerUnregisterSid",
        "ApplicationUserModelIdFromProductId", "AddDependencyToProcessPackageGraph", "AddPackageToFamilyXref", "RemovePackageFromFamilyXref",
        "CouldMultiUserAppsBehaviorBePossibleForPackage", "DeriveCapabilitySidsFromName", "ExtensionProgIdExists", "EnumerateExtensionNames",
        "GetAlternatePackageRoots", "GetAppDataFolder", "GetAppModelVersion", "GetCurrentPackageApplicationContext",
        "GetCurrentPackageApplicationResourcesContext", "GetCurrentPackageContext", "GetCurrentPackageGlobalizationContext",
        "GetCurrentPackageInfo3", "GetCurrentPackageResourcesContext", "GetCurrentPackageSecurityContext", "GetCurrentTargetPlatformContext",
        "GetEffectivePackageStatusForUser", "GetEffectivePackageStatusForUserSid", "GetExtensionApplicationUserModelId", "GetExtensionProgIds",
        "GetExtensionProperty", "GetExtensionProperty2", "GetFallbackDisplayName", "GetPackageApplicationContext",
        "GetPackageApplicationProperty", "GetPackageApplicationPropertyString", "GetPackageApplicationResourcesContext", "GetPackageContext",
        "GetPackageFamilyNameFromProgId", "GetPackageGlobalizationContext", "GetPackageGlobalizationProperty", "GetPackageInfo2",
        "GetPackageInfo3", "GetPackageInstallTime", "GetPackageOSMaxVersionTested", "GetPackagePathOnVolume", "GetPackageProperty",
        "GetPackagePropertyString", "GetPackageResourcesContext", "GetPackageResourcesProperty", "GetPackageSecurityContext",
        "GetPackageSecurityProperty", "GetPackageStatus", "GetPackageStatusForUser", "GetPackageStatusForUserSid",
        "GetPackageTargetPlatformProperty", "GetPackageVolumeSisPath", "GetProtocolAumid", "GetProtocolProperty", "GetPublisherCacheFolder",
        "GetPublisherRootFolder", "GetSecureSystemAppDataFolder", "GetSharedLocalFolder", "GetStateRootFolder", "GetStateRootFolderBase",
        "GetStateSettingsFolder", "GetStateVersion", "GetSystemMetadataPath", "GetSystemMetadataPathForPackage",
        "GetSystemMetadataPathForPackageFamily", "GetTargetPlatformContext", "IncrementPackageStatusVersion", "InvalidateAppModelVersionCache",
        "IsDeveloperModeEnabled", "IsDeveloperModePolicyApplied", "IsMrtResourceRedirectionEnabled", "IsSideloadingEnabled",
        "IsSideloadingPolicyApplied", "OpenPackageInfoByFullNameForMachine", "OpenPackageInfoByFullNameForUser", "PackageSidFromFamilyName",
        "PackageSidFromProductId", "PackageFamilyNameFromProductId", "PackageFullNameFromProductId", "PackageIdFromProductId",
        "PackageRelativeApplicationIdFromProductId", "RemovePackageStatus", "RemovePackageStatusForUser", "VerifyPackagePublisher",
        "VerifyPackagePublisherA", "VerifyPackageIdA", "PackageFamilyNameFromIdA", "PackageFullNameFromIdA",
        "CloseStateAtom", "CloseStateChangeNotification", "CloseStateContainer", "CloseStateLock", "CommitStateAtom", "CreateStateAtom",
        "CreateStateChangeNotification", "CreateStateContainer", "CreateStateLock", "CreateStateSubcontainer", "DeleteStateAtomValue",
        "DeleteStateContainer", "DeleteStateContainerValue", "DuplicateStateContainerHandle", "EnumerateStateAtomValues",
        "EnumerateStateContainerItems", "GetStateContainerDepth", "OpenStateAtom", "OpenStateExplicitForUserSid",
        "OpenStateExplicitForUserSidString", "PublishStateChangeNotification", "QueryStateAtomValueInfo", "QueryStateContainerCreatedNew",
        "QueryStateContainerItemInfo", "ReadStateAtomValue", "ReadStateContainerValue", "RegisterStateChangeNotification", "RegisterStateLock",
        "AcquireStateLock", "ReleaseStateLock", "UnregisterStateChangeNotification", "UnregisterStateLock", "SubscribeStateChangeNotification",
        "UnsubscribeStateChangeNotification", "WriteStateAtomValue", "WriteStateContainerValue", "CheckIfStateChangeNotificationExists",
    };
    int missing = 0;
    for (size_t i = 0; i < ARRAYSIZE(n); i++) {
        if (!t_proc_kb(n[i])) { missing++; CHECKF(0, "kernelbase export %s", n[i]); }
    }
}
TEST(probe_kernelbase_internal_helpers) {
    static const char *n[] = {
        "BaseDllFreeResourceId", "BaseDllMapResourceIdW", "BaseFreeAppCompatDataForProcess", "BaseGetConsoleReference", "BaseMarkFileForDelete",
        "BasepAdjustObjectAttributesForPrivateNamespace", "BasepCopyFileCallback", "BasepCopyFileExW", "BasepNotifyTrackingService",
        "BaseReadAppCompatDataForProcess", "DelayLoadFailureHookLookup", "DecodeRemotePointer", "EncodeRemotePointer",
        "InternetTimeFromSystemTimeA", "InternetTimeFromSystemTimeW", "InternetTimeToSystemTimeA", "InternetTimeToSystemTimeW",
        "ParseURLA", "ParseURLW", "GetIsEdpEnabled", "GetIsWdagEnabled", "CompareObjectHandles", "CreateFileMapping2",
        "ConvertAuxiliaryCounterToPerformanceCounter", "ConvertPerformanceCounterToAuxiliaryCounter", "CopyMemoryNonTemporal",
        "GuardCheckLongJumpTarget", "GetProcAddressForCaller", "IsApiSetImplemented", "GetOsSafeBootMode", "GetOsManufacturingMode",
        "GetGamingDeviceModelInformation", "GetIntegratedDisplaySize", "GetSystemLeapSecondInformation", "GetSystemTimeAdjustmentPrecise",
        "GetDynamicTimeZoneInformationEffectiveYears", "EnumDynamicTimeZoneInformation", "GetSystemWow64Directory2A", "GetSystemWow64Directory2W",
        "CreatePseudoConsoleAsUser", "GetSerializedAtomBytes", "GetStringTableEntry",
    };
    for (size_t i = 0; i < ARRAYSIZE(n); i++) CHECKF(t_proc_kb(n[i]) != NULL, "kernelbase export %s", n[i]);
}
TEST(probe_ums_exports_note) {
    static const char *n[] = { "CreateUmsCompletionList", "CreateUmsThreadContext", "DeleteUmsCompletionList", "DeleteUmsThreadContext",
        "DequeueUmsCompletionListItems", "EnterUmsSchedulingMode", "ExecuteUmsThread", "GetCurrentUmsThread", "GetNextUmsListItem",
        "GetUmsCompletionListEvent", "GetUmsSystemThreadInformation", "QueryUmsThreadInformation", "SetUmsThreadInformation", "UmsThreadYield" };
    int present = 0;
    for (size_t i = 0; i < ARRAYSIZE(n); i++) if (t_proc_k32(n[i])) present++;
    t_note("UMS exports present: %d/%zu", present, ARRAYSIZE(n));
}
TEST(ums_current_thread_not_ums) {
    if (!t_proc_k32("GetCurrentUmsThread")) { t_note("GetCurrentUmsThread not exported"); return; }
    typedef PVOID (WINAPI *f_t)(void);
    f_t f = (f_t)t_proc_k32("GetCurrentUmsThread");
    SET_ERR(0);
    PVOID r = f();
    CHECK(r == NULL);
    t_note("GetCurrentUmsThread on a normal thread: err %lu", GetLastError());
}
TEST(probe_kernel32_forwarders_resolve) {
    /* appmodel exports in kernel32 forward to api-ms contract DLLs / kernelbase; the forwarded address is not inside kernel32 */
    static const char *n[] = { "GetCurrentPackageId", "GetPackageFullName", "ParseApplicationUserModelId", "AppPolicyGetWindowingModel",
        "VerifyPackageFullName", "FindPackagesByPackageFamily", "GetApplicationUserModelId" };
    HMODULE k = t_kernel32();
    for (size_t i = 0; i < ARRAYSIZE(n); i++) {
        FARPROC p = GetProcAddress(k, n[i]);
        CHECKF(p != NULL, "%s", n[i]);
        if (p) {
            HMODULE owner = NULL;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
            t_note("%s resolves into module %p (kernel32 %p, kernelbase %p)", n[i], owner, k, t_kernelbase());
        }
    }
}
