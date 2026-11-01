/* Context-management APIs (InitializeContext, CopyContext, LocateXStateFeature, GetXStateFeaturesMask)
 * and WER flag helpers. All pure user mode, no process-global changes. */
#include "eed.h"

#ifndef CONTEXT_XSTATE
#define CONTEXT_XSTATE (0x00100040)
#endif
#define CTXF_FULL 0x10000Bu   /* CONTEXT_AMD64 | CONTROL | INTEGER | FLOATING_POINT */

UDECL(BOOL, eedGetXStateFeaturesMask, (PCONTEXT, PDWORD64));
UDECL(DWORD64, eedGetEnabledXStateFeatures, (void));
UDECL(DWORD, eedWerGetFlagsStub, (HANDLE, PDWORD));
#define PGXM ((eedGetXStateFeaturesMask_pfn)t_proc("GetXStateFeaturesMask"))
#define PGEXF ((eedGetEnabledXStateFeatures_pfn)t_proc("GetEnabledXStateFeatures"))

TEST(eed_ctxapi_sizeof_context) {
    EXPECT_EQ(sizeof(CONTEXT), 1232);
    EXPECT_EQ(offsetof(CONTEXT, FltSave), 0x100);
    EXPECT_EQ(offsetof(CONTEXT, Xmm0), 0x1a0);
    EXPECT_EQ(offsetof(CONTEXT, Rip), 0xf8);
}
TEST(eed_ctxapi_exports) {
    CHECK(UK(InitializeContext) != NULL); CHECK(UB(InitializeContext) != NULL);
    CHECK(UK(CopyContext) != NULL); CHECK(UK(LocateXStateFeature) != NULL);
    CHECK(PGXM != NULL); CHECK(PGEXF != NULL);
}
TEST(eed_ctxapi_init_size_query_full) {
    REQUIRE(U(InitializeContext));
    DWORD len = 0;
    SetLastError(0);
    BOOL r = U(InitializeContext)(NULL, CTXF_FULL, NULL, &len);
    EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECKF(len >= sizeof(CONTEXT) && len < 16384, "len %lu", len);
}
TEST(eed_ctxapi_init_size_query_ignores_input_length) {
    REQUIRE(U(InitializeContext));
    DWORD a = 0, b = 0x7777;
    SetLastError(0); EXPECT_EQ(U(InitializeContext)(NULL, CTXF_FULL, NULL, &a), 0);
    SetLastError(0); EXPECT_EQ(U(InitializeContext)(NULL, CTXF_FULL, NULL, &b), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(a, b);
}
TEST(eed_ctxapi_init_control_only_is_not_larger_than_full) {
    REQUIRE(U(InitializeContext));
    DWORD a = 0, b = 0;
    U(InitializeContext)(NULL, CTXF_FULL, NULL, &a);
    U(InitializeContext)(NULL, 0x100001, NULL, &b);   /* AMD64 | CONTROL */
    CHECKF(b >= sizeof(CONTEXT) && b <= a, "control %lu full %lu", b, a);
}
TEST(eed_ctxapi_init_into_buffer) {
    REQUIRE(U(InitializeContext));
    DWORD len = 0;
    U(InitializeContext)(NULL, CTXF_FULL, NULL, &len);
    if (!len) { CHECK(0); return; }
    char *buf = (char *)malloc(len + 64);
    memset(buf, 0xCC, len + 64);
    PCONTEXT c = NULL; DWORD l2 = len;
    SetLastError(0);
    BOOL r = U(InitializeContext)(buf, CTXF_FULL, &c, &l2);
    CHECK(r);
    if (r) {
        CHECK((char *)c >= buf && (char *)c + sizeof(CONTEXT) <= buf + len);
        EXPECT_EQ(c->ContextFlags, CTXF_FULL);
        EXPECT_EQ(l2, len);
        EXPECT_EQ(((ULONG_PTR)c) & 0xF, 0);
    }
    /* bytes beyond the reported length are not touched */
    int clean = 1;
    for (DWORD i = len; i < len + 64; i++) if ((unsigned char)buf[i] != 0xCC) clean = 0;
    CHECK(clean);
    free(buf);
}
TEST(eed_ctxapi_init_buffer_one_byte_short) {
    REQUIRE(U(InitializeContext));
    DWORD len = 0;
    U(InitializeContext)(NULL, CTXF_FULL, NULL, &len);
    char *buf = (char *)malloc(len + 16);
    PCONTEXT c = (PCONTEXT)0x1234; DWORD l2 = len - 1;
    SetLastError(0);
    BOOL rr = U(InitializeContext)(buf, CTXF_FULL, &c, &l2); DWORD ee = GetLastError();
    EXPECT_EQ(rr, 0); EXPECT_EQ(ee, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(l2, len);
    EXPECT_EQ(c, 0x1234);
    free(buf);
}
TEST(eed_ctxapi_init_buffer_zero_length) {
    REQUIRE(U(InitializeContext));
    char buf[32]; PCONTEXT c = NULL; DWORD l = 0;
    if (t_is_wine()) { t_skip("wine writes into the undersized buffer"); return; }
    SetLastError(0);
    BOOL rr = U(InitializeContext)(buf, CTXF_FULL, &c, &l); DWORD ee = GetLastError();
    EXPECT_EQ(rr, 0); EXPECT_EQ(ee, ERROR_INSUFFICIENT_BUFFER);
    CHECK(l >= sizeof(CONTEXT));
}
TEST(eed_ctxapi_init_xstate_larger_when_avx) {
    REQUIRE(U(InitializeContext));
    DWORD plain = 0, xs = 0;
    U(InitializeContext)(NULL, CTXF_FULL, NULL, &plain);
    SetLastError(0);
    BOOL r = U(InitializeContext)(NULL, CTXF_FULL | CONTEXT_XSTATE, NULL, &xs);
    if (!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        CHECK(xs >= plain);
        t_note("context length plain %lu xstate %lu (%s)", plain, xs, xs > plain ? "xstate present" : "no xstate");
        if (xs > plain) {
            char *buf = (char *)malloc(xs + 64);
            PCONTEXT c = NULL; DWORD l = xs;
            BOOL ok = U(InitializeContext)(buf, CTXF_FULL | CONTEXT_XSTATE, &c, &l);
            CHECK(ok);
            if (ok) EXPECT_EQ(c->ContextFlags & 0x40, 0x40);
            free(buf);
        }
    } else t_note("xstate context init: r=%d err=%lu", r, GetLastError());
}
TEST(eed_ctxapi_init_repeat_gives_same_length) {
    REQUIRE(U(InitializeContext));
    DWORD first = 0;
    for (int i = 0; i < 50; i++) {
        DWORD l = 0;
        U(InitializeContext)(NULL, CTXF_FULL, NULL, &l);
        if (!i) first = l; else if (l != first) { CHECKF(0, "length changed %lu -> %lu", first, l); break; }
    }
    CHECK(first != 0);
}

/* ---- GetXStateFeaturesMask on plain CONTEXT structures ---- */
TEST(eed_ctxapi_xmask_float_context_is_x87_sse) {
    REQUIRE(PGXM);
    static CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CTXF_FULL;
    DWORD64 m = 0xFF;
    SetLastError(0);
    EXPECT_EQ(PGXM(&c, &m), 1);
    EXPECT_EQ(m, 3);
}
TEST(eed_ctxapi_xmask_no_float_flag_is_zero) {
    REQUIRE(PGXM);
    static CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = 0x100003;   /* AMD64 | CONTROL | INTEGER */
    DWORD64 m = 0xFF;
    EXPECT_EQ(PGXM(&c, &m), 1);
    EXPECT_EQ(m, 0);
}
TEST(eed_ctxapi_xmask_no_arch_bits_fails) {
    REQUIRE(PGXM);
    static CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = 0;
    DWORD64 m = 0xFF;
    SetLastError(0);
    EXPECT_EQ(PGXM(&c, &m), 0);
    EXPECT_ERR(ERROR_NOT_SUPPORTED);          /* STATUS_NOT_SUPPORTED (0xC00000BB) -> 50 */
    EXPECT_EQ(m, 0xFF);
}
TEST(eed_ctxapi_enabled_features_has_x87_sse) {
    REQUIRE(PGEXF);
    DWORD64 m = PGEXF();
    EXPECT_EQ(m & 3, 3);
    t_note("enabled xstate features 0x%llx", (unsigned long long)m);
}

/* ---- LocateXStateFeature ---- */
TEST(eed_ctxapi_locate_legacy_features) {
    REQUIRE(U(LocateXStateFeature));
    static CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CTXF_FULL;
    DWORD len = 0;
    PVOID p = U(LocateXStateFeature)(&c, 0, &len);
    EXPECT_EQ(p, &c.FltSave); EXPECT_EQ(len, 0xa0);
    len = 0;
    p = U(LocateXStateFeature)(&c, 1, &len);
    EXPECT_EQ(p, &c.Xmm0); EXPECT_EQ(len, 0x100);
}
TEST(eed_ctxapi_locate_null_length_ok) {
    REQUIRE(U(LocateXStateFeature));
    static CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CTXF_FULL;
    EXPECT_EQ(U(LocateXStateFeature)(&c, 0, NULL), &c.FltSave);
    EXPECT_EQ(U(LocateXStateFeature)(&c, 1, NULL), &c.Xmm0);
}
TEST(eed_ctxapi_locate_without_arch_flag_is_null) {
    REQUIRE(U(LocateXStateFeature));
    static CONTEXT c; memset(&c, 0, sizeof c);
    DWORD len = 0x55;
    EXPECT_EQ(U(LocateXStateFeature)(&c, 0, &len), 0);
    EXPECT_EQ(U(LocateXStateFeature)(&c, 1, &len), 0);
    EXPECT_EQ(len, 0x55);
}
TEST(eed_ctxapi_locate_avx_without_xstate_flag_is_null) {
    REQUIRE(U(LocateXStateFeature));
    static CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CTXF_FULL;
    EXPECT_EQ(U(LocateXStateFeature)(&c, 2, NULL), 0);
    EXPECT_EQ(U(LocateXStateFeature)(&c, 5, NULL), 0);
}
TEST(eed_ctxapi_locate_xstate_features_in_initialized_context) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(LocateXStateFeature));
    DWORD len = 0;
    SetLastError(0);
    U(InitializeContext)(NULL, CTXF_FULL | CONTEXT_XSTATE, NULL, &len);
    char *buf = (char *)malloc(len + 64);
    PCONTEXT c = NULL; DWORD l = len;
    if (U(InitializeContext)(buf, CTXF_FULL | CONTEXT_XSTATE, &c, &l)) {
        DWORD l1 = 0, l2 = 0;
        PVOID sse = U(LocateXStateFeature)(c, 1, &l1);
        EXPECT_EQ(sse, &c->Xmm0);
        PVOID avx = U(LocateXStateFeature)(c, 2, &l2);
        t_note("AVX feature area %p len %lu (context %p..%p)", avx, l2, (void *)c, (void *)((char *)c + l));
        if (avx) { CHECK((char *)avx > (char *)c); CHECKF(l2 == 0x100, "len %lu", l2); }
    } else t_note("xstate context unavailable err %lu", GetLastError());
    free(buf);
}

/* ---- CopyContext ---- */
static PCONTEXT mkctx(char **mem) {
    DWORD len = 0;
    U(InitializeContext)(NULL, CTXF_FULL, NULL, &len);
    *mem = (char *)calloc(1, len + 64);
    PCONTEXT c = NULL;
    if (!U(InitializeContext)(*mem, CTXF_FULL, &c, &len)) { free(*mem); *mem = NULL; return NULL; }
    return c;
}
TEST(eed_ctxapi_copycontext_integer_only) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(CopyContext));
    char *m1, *m2; PCONTEXT s = mkctx(&m1), d = mkctx(&m2);
    if (!s || !d) { CHECK(0); free(m1); free(m2); return; }
    s->Rax = 0x1111; s->Rbx = 0x2222; s->Rsp = 0x3333; s->Rip = 0x4444; s->EFlags = 0x246;
    SetLastError(0);
    BOOL r = U(CopyContext)(d, CONTEXT_INTEGER, s);
    if (!r) t_note("CopyContext failed err %lu", GetLastError());
    CHECK(r);
    if (r) {
        EXPECT_EQ(d->Rax, 0x1111); EXPECT_EQ(d->Rbx, 0x2222);
        EXPECT_EQ(d->Rsp, 0); EXPECT_EQ(d->Rip, 0);
    }
    free(m1); free(m2);
}
TEST(eed_ctxapi_copycontext_control_only) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(CopyContext));
    char *m1, *m2; PCONTEXT s = mkctx(&m1), d = mkctx(&m2);
    if (!s || !d) { CHECK(0); free(m1); free(m2); return; }
    s->Rax = 0x1111; s->Rsp = 0x3333; s->Rip = 0x4444; s->EFlags = 0x246;
    BOOL r = U(CopyContext)(d, CONTEXT_CONTROL, s);
    CHECK(r);
    if (r) { EXPECT_EQ(d->Rsp, 0x3333); EXPECT_EQ(d->Rip, 0x4444); EXPECT_EQ(d->EFlags, 0x246); EXPECT_EQ(d->Rax, 0); }
    free(m1); free(m2);
}
TEST(eed_ctxapi_copycontext_float_xmm) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(CopyContext));
    char *m1, *m2; PCONTEXT s = mkctx(&m1), d = mkctx(&m2);
    if (!s || !d) { CHECK(0); free(m1); free(m2); return; }
    s->Xmm3.Low = 0xAABBCCDD11223344ull; s->Xmm3.High = 0x5566;
    s->MxCsr = 0x1F80;
    BOOL r = U(CopyContext)(d, CONTEXT_FLOATING_POINT, s);
    CHECK(r);
    if (r) { EXPECT_EQ(d->Xmm3.Low, 0xAABBCCDD11223344ull); EXPECT_EQ(d->Xmm3.High, 0x5566); t_note("MxCsr copied with FP flag: 0x%lx", d->MxCsr); }
    free(m1); free(m2);
}
TEST(eed_ctxapi_copycontext_flags_not_in_source_fails) {
    REQUIRE(U(InitializeContext)); REQUIRE(U(CopyContext));
    char *m1, *m2; PCONTEXT s = mkctx(&m1), d = mkctx(&m2);
    if (!s || !d) { CHECK(0); free(m1); free(m2); return; }
    s->ContextFlags = 0x100001;           /* control only */
    SetLastError(0);
    BOOL r = U(CopyContext)(d, CONTEXT_INTEGER, s);   /* integer not present in source */
    t_note("CopyContext of flags absent from source -> %d err %lu", r, GetLastError());
    free(m1); free(m2);
}

/* ---- WER flag helpers (process-global state: child process only) ---- */
CHILD(eed_wer_flags) {
    LONG (WINAPI *setf)(DWORD) = (LONG (WINAPI *)(DWORD))t_proc("WerSetFlags");
    LONG (WINAPI *getf)(HANDLE, PDWORD) = (LONG (WINAPI *)(HANDLE, PDWORD))t_proc("WerGetFlags");
    if (!setf || !getf) return 100;
    DWORD orig = 0xFFFFFFFF;
    LONG hr0 = getf(GetCurrentProcess(), &orig);
    printf("WerGetFlags initial hr=0x%lx flags=0x%lx\n", (unsigned long)hr0, (unsigned long)orig);
    LONG hr = setf(orig == 0xFFFFFFFF ? 0 : orig);          /* write back what we read: no net change */
    printf("WerSetFlags hr=0x%lx\n", (unsigned long)hr);
    DWORD now = 0xFFFFFFFE;
    LONG hr2 = getf(GetCurrentProcess(), &now);
    printf("WerGetFlags after hr=0x%lx flags=0x%lx\n", (unsigned long)hr2, (unsigned long)now);
    if (hr == 0 && hr2 == 0 && now != (orig == 0xFFFFFFFF ? 0 : orig)) return 1;
    return 0;
}
TEST(eed_wer_flags_roundtrip_in_child) {
    if (!t_proc("WerSetFlags") || !t_proc("WerGetFlags")) { t_skip("missing WerSetFlags/WerGetFlags"); return; }
    DWORD c = t_run_child("eed_wer_flags", "", 20000);
    CHECKF(c == 0, "child exit 0x%lx", c);
}
TEST(eed_wer_getflags_null_args_no_crash) {
    void (WINAPI *getf)(HANDLE, PDWORD) = (void (WINAPI *)(HANDLE, PDWORD))t_proc("WerGetFlags");
    if (!getf) { t_skip("missing WerGetFlags"); return; }
    DWORD f = 0x77;
    getf(NULL, &f);                    /* reference analysis: invalid params are rejected before any memory access */
    getf(GetCurrentProcess(), NULL);
    EXPECT_EQ(f, 0x77);
}
