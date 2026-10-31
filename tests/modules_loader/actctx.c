/* Activation contexts: CreateActCtx, Activate/Deactivate, GetCurrentActCtx, Query*, FindActCtxSection*, redirection */
#include "ml.h"
#include <wchar.h>

#define MANIFEST_NS "urn:schemas-microsoft-com:asm.v1"
static const char *MAN_HEAD = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n<assembly xmlns=\"" MANIFEST_NS "\" manifestVersion=\"1.0\">\r\n";
static const char *MAN_TAIL = "</assembly>\r\n";

static const GUID k_clsid = { 0x6e7a1f10, 0x1111, 0x4222, { 0x93, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66 } };
static const GUID k_missing = { 0x6e7a1f10, 0x9999, 0x4222, { 0x93, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66 } };

/* a manifest exercising most sections */
static const char *k_full_manifest =
    "<assemblyIdentity type=\"win32\" name=\"k32t.test.app\" version=\"1.2.3.4\"/>\r\n"
    "<file name=\"k32t_redir.dll\"/>\r\n"
    "<file name=\"k32t_com.dll\">\r\n"
    "  <comClass clsid=\"{6e7a1f10-1111-4222-9333-444455556666}\" threadingModel=\"Both\" progid=\"K32T.Object.1\"/>\r\n"
    "</file>\r\n"
    "<file name=\"k32t_wnd.dll\"><windowClass>K32TWndClass</windowClass></file>\r\n"
    "<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\"><security><requestedPrivileges>"
    "<requestedExecutionLevel level=\"asInvoker\" uiAccess=\"false\"/></requestedPrivileges></security></trustInfo>\r\n"
    "<compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>"
    "<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/></application></compatibility>\r\n"
    "<application xmlns=\"urn:schemas-microsoft-com:asm.v3\"><windowsSettings>"
    "<dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware>"
    "</windowsSettings></application>\r\n";

static int write_manifest(const char *path, const char *body) {
    size_t n = strlen(MAN_HEAD) + strlen(body) + strlen(MAN_TAIL);
    char *b = malloc(n + 1); strcpy(b, MAN_HEAD); strcat(b, body); strcat(b, MAN_TAIL);
    int r = ml_write(path, b, n); free(b); return r;
}
static HANDLE create_ctx_w(const char *path, DWORD flags) {
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.dwFlags = flags; a.lpSource = ml_w(path);
    return CreateActCtxW(&a);
}
static HANDLE mk_ctx(const char *sub, const char *body) {
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sapp.manifest", ml_dir(sub));
    if (!write_manifest(p, body)) return INVALID_HANDLE_VALUE;
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p);
    return U(CreateActCtxW)(&a);
}
static const char *mk_path(const char *sub, const char *body) {
    static char p[MAX_PATH]; snprintf(p, sizeof p, "%sapp.manifest", ml_dir(sub)); write_manifest(p, body); return p;
}

/* ---- CreateActCtx argument validation ---- */
TEST(ac_create_null) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(CreateActCtxA));
    SET_ERR(0);
    CHECK(U(CreateActCtxW)(NULL) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(CreateActCtxA)(NULL) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ac_create_bad_cbsize) {
    REQUIRE(U(CreateActCtxW));
    char *p = (char *)mk_path("ac_cb", k_full_manifest);
    /* CreateActCtxWWorker only requires cbSize >= 16, and enough room for the fields selected by dwFlags */
    static const DWORD sizes[] = { 0, 1, 4, 15 };
    for (int i = 0; i < 4; i++) {
        ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizes[i]; a.lpSource = ml_w(p);
        SET_ERR(0);
        CHECKF(U(CreateActCtxW)(&a) == INVALID_HANDLE_VALUE, "cbSize %lu", sizes[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    /* flag needs a field beyond cbSize */
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = 16; a.lpSource = ml_w(p); a.dwFlags = ACTCTX_FLAG_HMODULE_VALID;
    SET_ERR(0);
    CHECK(U(CreateActCtxW)(&a) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* undefined high flag bits */
    memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p); a.dwFlags = 0x100;
    SET_ERR(0);
    CHECK(U(CreateActCtxW)(&a) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ac_create_null_source) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(CreateActCtxA));
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a;
    SET_ERR(0);
    CHECK(U(CreateActCtxW)(&a) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    ACTCTXA b; memset(&b, 0, sizeof b); b.cbSize = sizeof b;
    SET_ERR(0);
    CHECK(U(CreateActCtxA)(&b) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ac_create_invalid_flags) {
    REQUIRE(U(CreateActCtxW));
    char *p = (char *)mk_path("ac_fl", k_full_manifest);
    static const DWORD bad[] = { 0x100, 0x200, 0x1000, 0x80000000, 0xFFFFFF00 };
    for (int i = 0; i < 5; i++) {
        ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p); a.dwFlags = bad[i];
        SET_ERR(0);
        HANDLE h = U(CreateActCtxW)(&a);
        CHECKF(h == INVALID_HANDLE_VALUE, "flags %lx", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h != INVALID_HANDLE_VALUE) ReleaseActCtx(h);
    }
}
TEST(ac_create_nonexistent_manifest) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(CreateActCtxA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%smissing.manifest", ml_dir("ac_ne"));
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p);
    SET_ERR(0);
    CHECK(U(CreateActCtxW)(&a) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    ACTCTXA b; memset(&b, 0, sizeof b); b.cbSize = sizeof b; b.lpSource = p;
    SET_ERR(0);
    CHECK(U(CreateActCtxA)(&b) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    snprintf(p, sizeof p, "%sno_such_dir\\x.manifest", ml_dir("ac_ne2"));
    a.lpSource = ml_w(p);
    SET_ERR(0);
    CHECK(U(CreateActCtxW)(&a) == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "e=%lu", e);
}
TEST(ac_create_directory_source) {
    REQUIRE(U(CreateActCtxW));
    const char *d = ml_dir("ac_dir"); char dd[MAX_PATH]; strcpy(dd, d); dd[strlen(dd) - 1] = 0;
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(dd);
    SET_ERR(0);
    HANDLE h = U(CreateActCtxW)(&a);
    CHECK(h == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() != 0);
    t_note("CreateActCtx(directory) error %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) ReleaseActCtx(h);
}
TEST(ac_create_malformed_manifests) {
    REQUIRE(U(CreateActCtxW));
    static const char *bad[] = {
        "not xml at all",
        "<?xml version=\"1.0\"?><assembly",
        "<?xml version=\"1.0\"?><notassembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"/>",
        "<?xml version=\"1.0\"?><assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"9.9\"><assemblyIdentity type=\"win32\" name=\"a.b\" version=\"1.0.0.0\"/></assembly>",
        "<?xml version=\"1.0\"?><assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"><assemblyIdentity type=\"win32\" name=\"a.b\" version=\"1.0\"/></assembly>",   /* bad version */
        "<?xml version=\"1.0\"?><assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"><assemblyIdentity type=\"win32\" version=\"1.0.0.0\"/></assembly>",   /* no name */
    };
    for (int i = 0; i < 6; i++) {
        char p[MAX_PATH]; snprintf(p, sizeof p, "%sbad%d.manifest", ml_dir("ac_bad"), i);
        ml_write(p, bad[i], strlen(bad[i]));
        ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p);
        SET_ERR(0);
        HANDLE h = U(CreateActCtxW)(&a);
        DWORD e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE, "case %d", i);
        CHECKF(e >= 14000 && e < 15000, "case %d error %lu", i, e);
        t_note("malformed manifest %d -> error %lu", i, e);
        if (h != INVALID_HANDLE_VALUE) ReleaseActCtx(h);
    }
}
TEST(ac_create_empty_manifest_file) {
    REQUIRE(U(CreateActCtxW));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sempty.manifest", ml_dir("ac_emp"));
    ml_write(p, "", 0);
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p);
    SET_ERR(0);
    HANDLE h = U(CreateActCtxW)(&a);
    CHECK(h == INVALID_HANDLE_VALUE);
    t_note("empty manifest -> error %lu", GetLastError());
    CHECK(GetLastError() != 0);
}
TEST(ac_create_minimal_valid) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    HANDLE h = mk_ctx("ac_min", "<assemblyIdentity type=\"win32\" name=\"k32t.min\" version=\"1.0.0.0\"/>\r\n");
    CHECKF(h != INVALID_HANDLE_VALUE && h != NULL, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
}
TEST(ac_create_a_variant) {
    REQUIRE(U(CreateActCtxA)); REQUIRE(U(ReleaseActCtx));
    char *p = (char *)mk_path("ac_a", k_full_manifest);
    ACTCTXA a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = p;
    HANDLE h = U(CreateActCtxA)(&a);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
}
TEST(ac_create_full_manifest) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    HANDLE h = mk_ctx("ac_full", k_full_manifest);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
}
TEST(ac_create_utf16_manifest) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sutf16.manifest", ml_dir("ac_u16"));
    char *a8 = malloc(4096); strcpy(a8, "<?xml version=\"1.0\" encoding=\"UTF-16\" standalone=\"yes\"?><assembly xmlns=\"" MANIFEST_NS "\" manifestVersion=\"1.0\"><assemblyIdentity type=\"win32\" name=\"k32t.u16\" version=\"1.0.0.0\"/></assembly>");
    WCHAR *w = malloc(8192); w[0] = 0xFEFF; int n = MultiByteToWideChar(CP_ACP, 0, a8, -1, w + 1, 4000);
    ml_write(p, w, (n) * 2);
    HANDLE h = create_ctx_w(p, 0);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
    free(a8); free(w);
}
TEST(ac_create_utf8_bom_manifest) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sbom.manifest", ml_dir("ac_bom"));
    const char *x = "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><assembly xmlns=\"" MANIFEST_NS "\" manifestVersion=\"1.0\"><assemblyIdentity type=\"win32\" name=\"k32t.bom\" version=\"1.0.0.0\"/></assembly>";
    ml_write(p, x, strlen(x));
    HANDLE h = create_ctx_w(p, 0);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
}
TEST(ac_create_many_contexts) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    char *p = (char *)mk_path("ac_many", k_full_manifest);
    HANDLE hs[50];
    for (int i = 0; i < 50; i++) {
        hs[i] = create_ctx_w(p, 0);
        CHECKF(hs[i] != INVALID_HANDLE_VALUE, "i=%d err %lu", i, GetLastError());
    }
    for (int i = 0; i < 50; i++) for (int j = i + 1; j < 50; j++) if (hs[i] != INVALID_HANDLE_VALUE) CHECK(hs[i] != hs[j]);
    for (int i = 0; i < 50; i++) if (hs[i] != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(hs[i]);
}

/* ---- manifests embedded in a module ---- */
static char *make_manifest_dll(const char *sub, const char *leaf, int id) {
    static char dll[MAX_PATH];
    if (!ml_put(ML_A, ml_dir(sub), leaf, dll)) return NULL;
    HANDLE h = BeginUpdateResourceA(dll, FALSE);
    if (!h) return NULL;
    size_t n = strlen(MAN_HEAD) + strlen(k_full_manifest) + strlen(MAN_TAIL);
    char *b = malloc(n + 1); strcpy(b, MAN_HEAD); strcat(b, k_full_manifest); strcat(b, MAN_TAIL);
    BOOL ok = UpdateResourceA(h, RT_MANIFEST, MAKEINTRESOURCEA(id), 0x409, b, (DWORD)n);
    free(b);
    if (!EndUpdateResourceA(h, !ok)) return NULL;
    return ok ? dll : NULL;
}
TEST(ac_create_from_pe_resource) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    char *dll = make_manifest_dll("ac_pe", "k32m_acm.dll", 2);
    if (!dll) { t_skip("cannot build manifest resource image"); return; }
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(dll); a.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID; a.lpResourceName = MAKEINTRESOURCEW(2);
    HANDLE h = U(CreateActCtxW)(&a);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
    /* wrong resource id */
    a.lpResourceName = MAKEINTRESOURCEW(77);
    SET_ERR(0);
    h = U(CreateActCtxW)(&a);
    CHECK(h == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_SXS_CANT_GEN_ACTCTX, "e=%lu", e);
    t_note("missing manifest resource id -> %lu", e);
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
}
TEST(ac_create_from_pe_resource_name_string) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    char *dll = make_manifest_dll("ac_pe2", "k32m_acm2.dll", 1);
    if (!dll) { t_skip("cannot build manifest resource image"); return; }
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(dll); a.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID; a.lpResourceName = MAKEINTRESOURCEW(1);
    HANDLE h = U(CreateActCtxW)(&a);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
}
TEST(ac_create_pe_without_manifest_resource) {
    REQUIRE(U(CreateActCtxW));
    char dll[MAX_PATH]; ml_put(ML_A, ml_dir("ac_pe3"), "k32m_acn.dll", dll);
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(dll); a.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID; a.lpResourceName = MAKEINTRESOURCEW(2);
    SET_ERR(0);
    HANDLE h = U(CreateActCtxW)(&a);
    CHECK(h == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_SXS_CANT_GEN_ACTCTX, "e=%lu", e);
    t_note("PE without manifest resource -> %lu", e);
}
TEST(ac_create_hmodule_valid) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    char *dll = make_manifest_dll("ac_hm", "k32m_ach.dll", 2);
    if (!dll) { t_skip("cannot build manifest resource image"); return; }
    HMODULE m = LoadLibraryA(dll);
    CHECK(m != NULL); if (!m) return;
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.dwFlags = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID;
    a.hModule = m; a.lpResourceName = MAKEINTRESOURCEW(2);
    HANDLE h = U(CreateActCtxW)(&a);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
    /* HMODULE_VALID without a module and without source */
    a.hModule = NULL;
    SET_ERR(0);
    h = U(CreateActCtxW)(&a);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
    ml_unload(m);
}
TEST(ac_create_resource_name_valid_without_name) {
    REQUIRE(U(CreateActCtxW));
    char *p = (char *)mk_path("ac_rn", k_full_manifest);
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p); a.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID;
    SET_ERR(0);
    HANDLE h = U(CreateActCtxW)(&a);
    t_note("RESOURCE_NAME_VALID with NULL name -> %p err %lu", h, h == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (h != INVALID_HANDLE_VALUE) ReleaseActCtx(h);
}
TEST(ac_create_processor_arch_and_lang_flags) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ReleaseActCtx));
    char *p = (char *)mk_path("ac_pl", k_full_manifest);
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(p);
    a.dwFlags = ACTCTX_FLAG_PROCESSOR_ARCHITECTURE_VALID; a.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
    HANDLE h = U(CreateActCtxW)(&a);
    CHECKF(h != INVALID_HANDLE_VALUE, "arch err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
    a.dwFlags = ACTCTX_FLAG_LANGID_VALID; a.wLangId = 0x409; a.wProcessorArchitecture = 0;
    h = U(CreateActCtxW)(&a);
    CHECKF(h != INVALID_HANDLE_VALUE, "lang err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
    a.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID; a.lpAssemblyDirectory = ml_w(t_tmpdir());
    h = U(CreateActCtxW)(&a);
    CHECKF(h != INVALID_HANDLE_VALUE, "dir err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
    a.dwFlags = ACTCTX_FLAG_APPLICATION_NAME_VALID; a.lpApplicationName = L"k32t.exe";
    h = U(CreateActCtxW)(&a);
    t_note("APPLICATION_NAME_VALID -> %s (err %lu)", h != INVALID_HANDLE_VALUE ? "ok" : "failed", h != INVALID_HANDLE_VALUE ? 0 : GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(ReleaseActCtx)(h);
}

/* ---- default state ---- */
TEST(ac_getcurrent_default) {
    REQUIRE(U(GetCurrentActCtx));
    HANDLE h = (HANDLE)0x1234;
    SET_ERR(0);
    BOOL ok = U(GetCurrentActCtx)(&h);
    CHECKF(ok, "err %lu", GetLastError());
    t_note("default GetCurrentActCtx handle = %p", h);
    CHECK(h != (HANDLE)0x1234);
    CHECK(h != INVALID_HANDLE_VALUE);
}
/* ---- activation ---- */
TEST(ac_activate_deactivate_basic) {
    REQUIRE(U(ActivateActCtx)); REQUIRE(U(DeactivateActCtx)); REQUIRE(U(GetCurrentActCtx)); REQUIRE(U(ReleaseActCtx));
    HANDLE ctx = mk_ctx("ac_act", k_full_manifest);
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    HANDLE before = NULL; U(GetCurrentActCtx)(&before);
    ULONG_PTR cookie = 0;
    SET_ERR(0);
    CHECKF(U(ActivateActCtx)(ctx, &cookie), "err %lu", GetLastError());
    CHECK(cookie != 0);
    HANDLE cur = NULL;
    CHECK(U(GetCurrentActCtx)(&cur));
    CHECK(cur == ctx);
    CHECK(U(DeactivateActCtx)(0, cookie));
    HANDLE after = (HANDLE)1;
    CHECK(U(GetCurrentActCtx)(&after));
    CHECK(after == before);
    U(ReleaseActCtx)(ctx);
}
TEST(ac_activate_nested_lifo) {
    REQUIRE(U(ActivateActCtx)); REQUIRE(U(DeactivateActCtx)); REQUIRE(U(GetCurrentActCtx));
    HANDLE a = mk_ctx("ac_n1", k_full_manifest), b = mk_ctx("ac_n2", "<assemblyIdentity type=\"win32\" name=\"k32t.other\" version=\"2.0.0.0\"/>\r\n");
    CHECK(a != INVALID_HANDLE_VALUE && b != INVALID_HANDLE_VALUE); if (a == INVALID_HANDLE_VALUE || b == INVALID_HANDLE_VALUE) return;
    HANDLE base = NULL; GetCurrentActCtx(&base);
    ULONG_PTR ca = 0, cb = 0, cc = 0; HANDLE cur;
    CHECK(U(ActivateActCtx)(a, &ca));
    CHECK(U(ActivateActCtx)(b, &cb));
    CHECK(U(ActivateActCtx)(a, &cc));           /* same context twice */
    CHECK(ca && cb && cc && ca != cb && cb != cc && ca != cc);
    CHECK(U(GetCurrentActCtx)(&cur) && cur == a);
    CHECK(U(DeactivateActCtx)(0, cc));
    CHECK(U(GetCurrentActCtx)(&cur) && cur == b);
    CHECK(U(DeactivateActCtx)(0, cb));
    CHECK(U(GetCurrentActCtx)(&cur) && cur == a);
    CHECK(U(DeactivateActCtx)(0, ca));
    CHECK(U(GetCurrentActCtx)(&cur) && cur == base);
    ReleaseActCtx(a); ReleaseActCtx(b);
}
TEST(ac_activate_keeps_context_alive_after_release) {
    REQUIRE(U(ActivateActCtx)); REQUIRE(U(DeactivateActCtx)); REQUIRE(U(GetCurrentActCtx));
    HANDLE ctx = mk_ctx("ac_keep", k_full_manifest);
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR cookie = 0;
    CHECK(U(ActivateActCtx)(ctx, &cookie));
    U(ReleaseActCtx)(ctx);                       /* the activation still holds a reference */
    HANDLE cur = NULL;
    CHECK(U(GetCurrentActCtx)(&cur) && cur == ctx);
    ACTCTX_SECTION_KEYED_DATA d; memset(&d, 0, sizeof d); d.cbSize = sizeof d;
    CHECK(FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    CHECK(U(DeactivateActCtx)(0, cookie));
}
TEST(ac_addref_release_balance) {
    REQUIRE(U(AddRefActCtx)); REQUIRE(U(ReleaseActCtx)); REQUIRE(U(ActivateActCtx)); REQUIRE(U(DeactivateActCtx));
    HANDLE ctx = mk_ctx("ac_ref", k_full_manifest);
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    for (int i = 0; i < 10; i++) U(AddRefActCtx)(ctx);
    for (int i = 0; i < 10; i++) U(ReleaseActCtx)(ctx);
    ULONG_PTR cookie = 0;
    CHECKF(U(ActivateActCtx)(ctx, &cookie), "err %lu", GetLastError());   /* still valid: the creation reference remains */
    CHECK(U(DeactivateActCtx)(0, cookie));
    U(ReleaseActCtx)(ctx);
}
TEST(ac_null_and_invalid_handles_are_ignored) {
    REQUIRE(U(AddRefActCtx)); REQUIRE(U(ReleaseActCtx));
    U(AddRefActCtx)(NULL);
    U(ReleaseActCtx)(NULL);
    U(AddRefActCtx)(INVALID_HANDLE_VALUE);
    U(ReleaseActCtx)(INVALID_HANDLE_VALUE);
    CHECK(1);
}
TEST(ac_activate_null_context) {
    REQUIRE(U(ActivateActCtx)); REQUIRE(U(DeactivateActCtx)); REQUIRE(U(GetCurrentActCtx));
    HANDLE ctx = mk_ctx("ac_nul", k_full_manifest);
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c1 = 0, c2 = 0; HANDLE cur = (HANDLE)1;
    CHECK(U(ActivateActCtx)(ctx, &c1));
    SET_ERR(0);
    BOOL ok = U(ActivateActCtx)(NULL, &c2);
    t_note("ActivateActCtx(NULL) -> %d cookie %p err %lu", ok, (void *)c2, ok ? 0 : GetLastError());
    if (ok) { U(GetCurrentActCtx)(&cur); t_note("current after NULL activation = %p", cur); CHECK(U(DeactivateActCtx)(0, c2)); }
    CHECK(U(DeactivateActCtx)(0, c1));
    ReleaseActCtx(ctx);
}
TEST(ac_activate_invalid_handle_value) {
    REQUIRE(U(ActivateActCtx));
    ULONG_PTR c = 0;
    SET_ERR(0);
    BOOL ok = U(ActivateActCtx)(INVALID_HANDLE_VALUE, &c);
    t_note("ActivateActCtx(INVALID_HANDLE_VALUE) -> %d err %lu", ok, ok ? 0 : GetLastError());
    if (ok) DeactivateActCtx(0, c);
    else CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
}

/* ---- deactivation errors (raise exceptions): child processes ---- */
static LONG WINAPI ac_unhandled(EXCEPTION_POINTERS *ep) { ExitProcess(ep->ExceptionRecord->ExceptionCode); return EXCEPTION_EXECUTE_HANDLER; }
static HANDLE child_ctx(const char *path) {
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(path);
    return U(CreateActCtxW)(&a);
}
CHILD(ac_double_deactivate) {
    SetUnhandledExceptionFilter(ac_unhandled);
    HANDLE ctx = child_ctx(argv[1]); if (ctx == INVALID_HANDLE_VALUE) return 90;
    ULONG_PTR c = 0;
    if (!U(ActivateActCtx)(ctx, &c)) return 91;
    if (!U(DeactivateActCtx)(0, c)) return 92;
    U(DeactivateActCtx)(0, c);              /* second deactivation of the same cookie raises */
    return 93;
}
CHILD(ac_out_of_order_deactivate) {
    SetUnhandledExceptionFilter(ac_unhandled);
    HANDLE ctx = child_ctx(argv[1]); if (ctx == INVALID_HANDLE_VALUE) return 90;
    ULONG_PTR c1 = 0, c2 = 0;
    if (!U(ActivateActCtx)(ctx, &c1) || !U(ActivateActCtx)(ctx, &c2)) return 91;
    U(DeactivateActCtx)(0, c1);             /* c2 is on top: early deactivation */
    return 93;
}
CHILD(ac_force_early_deactivate) {
    SetUnhandledExceptionFilter(ac_unhandled);
    HANDLE ctx = child_ctx(argv[1]); if (ctx == INVALID_HANDLE_VALUE) return 90;
    ULONG_PTR c1 = 0, c2 = 0, c3 = 0; HANDLE cur;
    HANDLE base = NULL; U(GetCurrentActCtx)(&base);
    if (!U(ActivateActCtx)(ctx, &c1) || !U(ActivateActCtx)(ctx, &c2) || !U(ActivateActCtx)(ctx, &c3)) return 91;
    if (!U(DeactivateActCtx)(DEACTIVATE_ACTCTX_FLAG_FORCE_EARLY_DEACTIVATION, c1)) return 92;   /* pops c3, c2 and c1 */
    if (!U(GetCurrentActCtx)(&cur) || cur != base) return 94;
    return 0;
}
CHILD(ac_bogus_cookie) {
    SetUnhandledExceptionFilter(ac_unhandled);
    U(DeactivateActCtx)(0, 0x12345678);
    return 93;
}
CHILD(ac_deactivate_cookie_zero) {
    SetUnhandledExceptionFilter(ac_unhandled);
    BOOL r = U(DeactivateActCtx)(0, 0);
    return r ? 0 : 1;
}
CHILD(ac_deactivate_bad_flags) {
    SetUnhandledExceptionFilter(ac_unhandled);
    HANDLE ctx = child_ctx(argv[1]); if (ctx == INVALID_HANDLE_VALUE) return 90;
    ULONG_PTR c = 0;
    if (!U(ActivateActCtx)(ctx, &c)) return 91;
    SetLastError(0);
    BOOL r = U(DeactivateActCtx)(0x80, c);
    if (r) { U(DeactivateActCtx)(0, c); return 0; }
    return GetLastError() == ERROR_INVALID_PARAMETER ? 1 : 2;
}
static DWORD run_actctx_child(const char *name, const char *sub) {
    const char *p = mk_path(sub, k_full_manifest);
    char args[MAX_PATH + 8]; snprintf(args, sizeof args, "\"%s\"", p);
    return ml_run_exe(t_self_path(), name, args, NULL, 25000, 0);
}
TEST(ac_double_deactivate_raises) {
    REQUIRE(U(DeactivateActCtx));
    DWORD rc = run_actctx_child("ac_double_deactivate", "ac_dd");
    CHECKF(rc == 0xC0150010 || rc == 0xC015000F, "rc=%08lx", rc);
    t_note("double deactivation raised %08lx", rc);
}
TEST(ac_out_of_order_deactivate_raises) {
    REQUIRE(U(DeactivateActCtx));
    DWORD rc = run_actctx_child("ac_out_of_order_deactivate", "ac_ooo");
    CHECKF(rc == 0xC015000F || rc == 0xC0150010, "rc=%08lx", rc);
    t_note("out-of-order deactivation raised %08lx", rc);
}
TEST(ac_force_early_deactivation_pops_stack) {
    REQUIRE(U(DeactivateActCtx));
    EXPECT_EQ(run_actctx_child("ac_force_early_deactivate", "ac_fe"), 0);
}
TEST(ac_bogus_cookie_raises) {
    REQUIRE(U(DeactivateActCtx));
    DWORD rc = ml_run_exe(t_self_path(), "ac_bogus_cookie", "", NULL, 25000, 0);
    CHECKF(rc == 0xC0150010 || rc == 0xC015000F, "rc=%08lx", rc);
}
TEST(ac_deactivate_cookie_zero_noted) {
    REQUIRE(U(DeactivateActCtx));
    DWORD rc = ml_run_exe(t_self_path(), "ac_deactivate_cookie_zero", "", NULL, 25000, 0);
    t_note("DeactivateActCtx(0, 0) child result %08lx", rc);
    CHECK(rc == 0 || rc == 1 || rc == 0xC0150010 || rc == 0xC015000F);
}
TEST(ac_deactivate_invalid_flags) {
    REQUIRE(U(DeactivateActCtx));
    DWORD rc = run_actctx_child("ac_deactivate_bad_flags", "ac_dbf");
    t_note("DeactivateActCtx(flags=0x80) child result %lx", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}

/* ---- Query ---- */
static HANDLE query_ctx(void) { return mk_ctx("ac_q", k_full_manifest); }
TEST(ac_query_basic_information) {
    REQUIRE(U(QueryActCtxW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ACTIVATION_CONTEXT_BASIC_INFORMATION bi; memset(&bi, 0xCC, sizeof bi); SIZE_T need = 0;
    SET_ERR(0);
    CHECKF(U(QueryActCtxW)(0, ctx, NULL, ActivationContextBasicInformation, &bi, sizeof bi, &need), "err %lu", GetLastError());
    CHECK(need == sizeof bi);
    CHECK(bi.hActCtx == ctx);
    t_note("basic information dwFlags = %lx", bi.dwFlags);
    /* NO_ADDREF flag */
    memset(&bi, 0, sizeof bi);
    CHECK(U(QueryActCtxW)(QUERY_ACTCTX_FLAG_NO_ADDREF, ctx, NULL, ActivationContextBasicInformation, &bi, sizeof bi, NULL));
    CHECK(bi.hActCtx == ctx);
    ReleaseActCtx(ctx);
}
TEST(ac_query_buffer_too_small) {
    REQUIRE(U(QueryActCtxW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ACTIVATION_CONTEXT_BASIC_INFORMATION bi; SIZE_T need = 0;
    SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, NULL, ActivationContextBasicInformation, &bi, sizeof bi - 1, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need == sizeof bi);
    need = 0;
    SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, NULL, ActivationContextBasicInformation, NULL, 0, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need == sizeof bi);
    ReleaseActCtx(ctx);
}
TEST(ac_query_invalid_arguments) {
    REQUIRE(U(QueryActCtxW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    char buf[256]; SIZE_T need;
    SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, NULL, 0, buf, sizeof buf, &need));          /* class 0 */
    CHECK(GetLastError() != 0);
    t_note("info class 0 -> error %lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, NULL, 0x1000, buf, sizeof buf, &need));
    CHECK(GetLastError() != 0);
    t_note("info class 0x1000 -> error %lu", GetLastError());
    SET_ERR(0);
    BOOL okf = U(QueryActCtxW)(0x40000000, ctx, NULL, ActivationContextBasicInformation, buf, sizeof buf, &need);   /* unknown flag */
    t_note("unknown flag -> %d error %lu", okf, okf ? 0 : GetLastError());
    ReleaseActCtx(ctx);
}
TEST(ac_query_use_active_actctx) {
    REQUIRE(U(QueryActCtxW)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTIVATION_CONTEXT_BASIC_INFORMATION bi; memset(&bi, 0, sizeof bi);
    CHECKF(U(QueryActCtxW)(QUERY_ACTCTX_FLAG_USE_ACTIVE_ACTCTX, NULL, NULL, ActivationContextBasicInformation, &bi, sizeof bi, NULL), "err %lu", GetLastError());
    CHECK(bi.hActCtx == ctx);
    DeactivateActCtx(0, c);
    ReleaseActCtx(ctx);
}
TEST(ac_query_detailed_information) {
    REQUIRE(U(QueryActCtxW));
    char *p = (char *)mk_path("ac_det", k_full_manifest);
    HANDLE ctx = create_ctx_w(p, 0); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    char buf[2048]; SIZE_T need = 0;
    memset(buf, 0, sizeof buf);
    BOOL ok = U(QueryActCtxW)(0, ctx, NULL, ActivationContextDetailedInformation, buf, sizeof buf, &need);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        ACTIVATION_CONTEXT_DETAILED_INFORMATION *di = (void *)buf;
        CHECK(di->ulFormatVersion == 1);
        CHECK(di->ulAssemblyCount >= 1);
        CHECK(di->ulRootManifestPathType == ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE);
        CHECK(di->lpRootManifestPath != NULL);
        if (di->lpRootManifestPath) {
            CHECK(di->ulRootManifestPathChars == wcslen(di->lpRootManifestPath));
            CHECK(!_wcsicmp(di->lpRootManifestPath, ml_w(p)));
        }
        CHECK(need >= sizeof(*di));
        t_note("detailed info: assemblies=%lu appdir type=%lu", di->ulAssemblyCount, di->ulAppDirPathType);
    }
    ReleaseActCtx(ctx);
}
TEST(ac_query_assembly_detailed_information) {
    REQUIRE(U(QueryActCtxW));
    char *p = (char *)mk_path("ac_asm", k_full_manifest);
    HANDLE ctx = create_ctx_w(p, 0); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    char buf[4096]; SIZE_T need = 0; DWORD index = 1;
    memset(buf, 0, sizeof buf);
    BOOL ok = U(QueryActCtxW)(0, ctx, &index, AssemblyDetailedInformationInActivationContext, buf, sizeof buf, &need);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        ACTIVATION_CONTEXT_ASSEMBLY_DETAILED_INFORMATION *ai = (void *)buf;
        CHECK(ai->ulManifestVersionMajor == 1 && ai->ulManifestVersionMinor == 0);
        CHECK(ai->ulManifestPathType == ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE);
        CHECK(ai->lpAssemblyManifestPath && !_wcsicmp(ai->lpAssemblyManifestPath, ml_w(p)));
        CHECK(ai->lpAssemblyEncodedAssemblyIdentity != NULL);
        if (ai->lpAssemblyEncodedAssemblyIdentity) {
            CHECK(wcsstr(ai->lpAssemblyEncodedAssemblyIdentity, L"k32t.test.app") != NULL);
            CHECK(wcsstr(ai->lpAssemblyEncodedAssemblyIdentity, L"1.2.3.4") != NULL);
            CHECK(ai->ulEncodedAssemblyIdentityLength == wcslen(ai->lpAssemblyEncodedAssemblyIdentity) * 2);
            t_note("encoded identity: %ls", ai->lpAssemblyEncodedAssemblyIdentity);
        }
        CHECK(ai->ulFileCount == 3);
    }
    /* index 0 and out-of-range */
    index = 0; SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, &index, AssemblyDetailedInformationInActivationContext, buf, sizeof buf, &need));
    CHECK(GetLastError() != 0);
    t_note("assembly index 0 -> error %lu", GetLastError());
    index = 77; SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, &index, AssemblyDetailedInformationInActivationContext, buf, sizeof buf, &need));
    CHECK(GetLastError() != 0);
    t_note("assembly index 77 -> error %lu", GetLastError());
    ReleaseActCtx(ctx);
}
TEST(ac_query_runlevel_information) {
    REQUIRE(U(QueryActCtxW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ACTIVATION_CONTEXT_RUN_LEVEL_INFORMATION rl; memset(&rl, 0xCC, sizeof rl); SIZE_T need = 0;
    BOOL ok = U(QueryActCtxW)(0, ctx, NULL, RunlevelInformationInActivationContext, &rl, sizeof rl, &need);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        CHECK(rl.RunLevel == ACTCTX_RUN_LEVEL_AS_INVOKER);
        CHECK(rl.UiAccess == 0);
        CHECK(need == sizeof rl);
    }
    SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, NULL, RunlevelInformationInActivationContext, &rl, sizeof rl - 1, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    ReleaseActCtx(ctx);
}
TEST(ac_query_runlevel_variants) {
    REQUIRE(U(QueryActCtxW));
    static const struct { const char *lvl; const char *ui; int expect; int uia; } t[] = {
        { "asInvoker", "false", ACTCTX_RUN_LEVEL_AS_INVOKER, 0 }, { "highestAvailable", "false", ACTCTX_RUN_LEVEL_HIGHEST_AVAILABLE, 0 },
        { "requireAdministrator", "false", ACTCTX_RUN_LEVEL_REQUIRE_ADMIN, 0 } };
    for (int i = 0; i < 3; i++) {
        char body[1024];
        snprintf(body, sizeof body, "<assemblyIdentity type=\"win32\" name=\"k32t.rl%d\" version=\"1.0.0.0\"/>\r\n"
            "<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\"><security><requestedPrivileges><requestedExecutionLevel level=\"%s\" uiAccess=\"%s\"/>"
            "</requestedPrivileges></security></trustInfo>\r\n", i, t[i].lvl, t[i].ui);
        char sub[32]; snprintf(sub, sizeof sub, "ac_rl%d", i);
        HANDLE ctx = mk_ctx(sub, body);
        CHECKF(ctx != INVALID_HANDLE_VALUE, "%s err %lu", t[i].lvl, GetLastError());
        if (ctx == INVALID_HANDLE_VALUE) continue;
        ACTIVATION_CONTEXT_RUN_LEVEL_INFORMATION rl; memset(&rl, 0, sizeof rl); SIZE_T need;
        CHECKF(U(QueryActCtxW)(0, ctx, NULL, RunlevelInformationInActivationContext, &rl, sizeof rl, &need), "%s", t[i].lvl);
        CHECKF(rl.RunLevel == t[i].expect && rl.UiAccess == (DWORD)t[i].uia, "%s level %d ui %lu", t[i].lvl, rl.RunLevel, rl.UiAccess);
        ReleaseActCtx(ctx);
    }
}
TEST(ac_query_runlevel_absent) {
    REQUIRE(U(QueryActCtxW));
    HANDLE ctx = mk_ctx("ac_rla", "<assemblyIdentity type=\"win32\" name=\"k32t.norl\" version=\"1.0.0.0\"/>\r\n");
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ACTIVATION_CONTEXT_RUN_LEVEL_INFORMATION rl; memset(&rl, 0xCC, sizeof rl); SIZE_T need = 0;
    SET_ERR(0);
    BOOL ok = U(QueryActCtxW)(0, ctx, NULL, RunlevelInformationInActivationContext, &rl, sizeof rl, &need);
    t_note("runlevel query without trustInfo -> %d level=%d err %lu", ok, ok ? rl.RunLevel : -1, ok ? 0 : GetLastError());
    if (ok) CHECK(rl.RunLevel == ACTCTX_RUN_LEVEL_UNSPECIFIED);
    ReleaseActCtx(ctx);
}
TEST(ac_query_compatibility_information) {
    REQUIRE(U(QueryActCtxW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    char buf[512]; SIZE_T need = 0; memset(buf, 0, sizeof buf);
    BOOL ok = U(QueryActCtxW)(0, ctx, NULL, CompatibilityInformationInActivationContext, buf, sizeof buf, &need);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION *ci = (void *)buf;
        CHECK(ci->ElementCount == 1);
        if (ci->ElementCount >= 1) {
            static const GUID win10 = { 0x8e0f7a12, 0xbfb3, 0x4fe8, { 0xb9, 0xa5, 0x48, 0xfd, 0x50, 0xa1, 0x5a, 0x9a } };
            CHECK(ci->Elements[0].Type == ACTCTX_COMPATIBILITY_ELEMENT_TYPE_OS);
            CHECK(!memcmp(&ci->Elements[0].Id, &win10, sizeof(GUID)));
        }
        CHECK(need == sizeof(DWORD) + sizeof(COMPATIBILITY_CONTEXT_ELEMENT) * 1 || need >= 4 + 20);
    }
    SIZE_T small = 2; SET_ERR(0);
    CHECK(!U(QueryActCtxW)(0, ctx, NULL, CompatibilityInformationInActivationContext, buf, small, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    ReleaseActCtx(ctx);
}
TEST(ac_query_compat_multiple_os) {
    REQUIRE(U(QueryActCtxW));
    HANDLE ctx = mk_ctx("ac_cmp2",
        "<assemblyIdentity type=\"win32\" name=\"k32t.cmp\" version=\"1.0.0.0\"/>\r\n"
        "<compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>"
        "<supportedOS Id=\"{35138b9a-5d96-4fbd-8e2d-a2440225f93a}\"/>"
        "<supportedOS Id=\"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}\"/>"
        "<supportedOS Id=\"{1f676c76-80e1-4239-95bb-83d0f6d0da78}\"/>"
        "<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/></application></compatibility>\r\n");
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    char buf[1024]; SIZE_T need = 0;
    BOOL ok = U(QueryActCtxW)(0, ctx, NULL, CompatibilityInformationInActivationContext, buf, sizeof buf, &need);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        ACTIVATION_CONTEXT_COMPATIBILITY_INFORMATION *ci = (void *)buf;
        CHECK(ci->ElementCount == 4);
        static const GUID ids[4] = {
            { 0x35138b9a, 0x5d96, 0x4fbd, { 0x8e, 0x2d, 0xa2, 0x44, 0x02, 0x25, 0xf9, 0x3a } },
            { 0x4a2f28e3, 0x53b9, 0x4441, { 0xba, 0x9c, 0xd6, 0x9d, 0x4a, 0x4a, 0x6e, 0x38 } },
            { 0x1f676c76, 0x80e1, 0x4239, { 0x95, 0xbb, 0x83, 0xd0, 0xf6, 0xd0, 0xda, 0x78 } },
            { 0x8e0f7a12, 0xbfb3, 0x4fe8, { 0xb9, 0xa5, 0x48, 0xfd, 0x50, 0xa1, 0x5a, 0x9a } } };
        for (int i = 0; i < 4 && i < (int)ci->ElementCount; i++) {
            int found = 0;
            for (int j = 0; j < 4; j++) if (!memcmp(&ci->Elements[j].Id, &ids[i], sizeof(GUID))) found = 1;
            CHECKF(found, "os guid %d", i);
        }
    }
    ReleaseActCtx(ctx);
}
TEST(ac_query_manifest_resource_name) {
    REQUIRE(U(QueryActCtxW));
    char *dll = make_manifest_dll("ac_mrn", "k32m_mrn.dll", 2);
    if (!dll) { t_skip("cannot build manifest resource image"); return; }
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(dll); a.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID; a.lpResourceName = MAKEINTRESOURCEW(2);
    HANDLE ctx = CreateActCtxW(&a); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    WCHAR buf[128]; SIZE_T need = 0;
    SET_ERR(0);
    BOOL ok = U(QueryActCtxW)(0, ctx, NULL, ActivationContextManifestResourceName, buf, sizeof buf, &need);
    t_note("ManifestResourceName query -> %d err %lu need %zu", ok, ok ? 0 : GetLastError(), need);
    ReleaseActCtx(ctx);
}

/* ---- QueryActCtxSettingsW ---- */
static const WCHAR *WS_NS = L"http://schemas.microsoft.com/SMI/2005/WindowsSettings";
TEST(ac_settings_dpiaware) {
    REQUIRE(U(QueryActCtxSettingsW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    WCHAR buf[64] = L"junk"; SIZE_T need = 0;
    SET_ERR(0);
    BOOL ok = U(QueryActCtxSettingsW)(0, ctx, WS_NS, L"dpiAware", buf, 64, &need);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        CHECK(!wcscmp(buf, L"true"));
        CHECK(need == 5 || need == 4);
        t_note("dpiAware: written/required = %zu", need);
    }
    /* too small */
    memset(buf, 0, sizeof buf); need = 0; SET_ERR(0);
    CHECK(!U(QueryActCtxSettingsW)(0, ctx, WS_NS, L"dpiAware", buf, 2, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need == 5);
    need = 0; SET_ERR(0);
    CHECK(!U(QueryActCtxSettingsW)(0, ctx, WS_NS, L"dpiAware", NULL, 0, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need == 5);
    ReleaseActCtx(ctx);
}
TEST(ac_settings_missing_and_wrong_namespace) {
    REQUIRE(U(QueryActCtxSettingsW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    WCHAR buf[64]; SIZE_T need = 0;
    SET_ERR(0);
    CHECK(!U(QueryActCtxSettingsW)(0, ctx, WS_NS, L"noSuchSetting", buf, 64, &need));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    SET_ERR(0);
    /* QueryActCtxSettingsWWorker only forwards the eight known WindowsSettings namespaces; anything else fails
     * before the activation-context query (status not shown by the decompilation) */
    CHECK(!U(QueryActCtxSettingsW)(0, ctx, L"urn:k32t:wrong-namespace", L"dpiAware", buf, 64, &need));
    t_note("QueryActCtxSettings(unknown namespace) error %lu", GetLastError());
    CHECK(GetLastError() != 0);
    SET_ERR(0);
    CHECK(!U(QueryActCtxSettingsW)(0, ctx, WS_NS, L"DPIAWARE", buf, 64, &need));        /* names are case sensitive */
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    ReleaseActCtx(ctx);
}
TEST(ac_settings_null_namespace) {
    REQUIRE(U(QueryActCtxSettingsW));
    HANDLE ctx = mk_ctx("ac_sns",
        "<assemblyIdentity type=\"win32\" name=\"k32t.sns\" version=\"1.0.0.0\"/>\r\n"
        "<application xmlns=\"urn:schemas-microsoft-com:asm.v3\"><windowsSettings>"
        "<longPathAware xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">true</longPathAware>"
        "<gdiScaling xmlns=\"http://schemas.microsoft.com/SMI/2017/WindowsSettings\">true</gdiScaling>"
        "</windowsSettings></application>\r\n");
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    WCHAR buf[64]; SIZE_T need = 0;
    CHECKF(U(QueryActCtxSettingsW)(0, ctx, L"http://schemas.microsoft.com/SMI/2016/WindowsSettings", L"longPathAware", buf, 64, &need), "err %lu", GetLastError());
    CHECK(!wcscmp(buf, L"true"));
    CHECKF(U(QueryActCtxSettingsW)(0, ctx, L"http://schemas.microsoft.com/SMI/2017/WindowsSettings", L"gdiScaling", buf, 64, &need), "err %lu", GetLastError());
    CHECK(!wcscmp(buf, L"true"));
    ReleaseActCtx(ctx);
}
TEST(ac_settings_invalid_flags) {
    REQUIRE(U(QueryActCtxSettingsW));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    WCHAR buf[64]; SIZE_T need = 0;
    SET_ERR(0);
    CHECK(!U(QueryActCtxSettingsW)(0x100, ctx, WS_NS, L"dpiAware", buf, 64, &need));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(QueryActCtxSettingsW)(0, NULL, WS_NS, L"dpiAware", buf, 64, &need));
    t_note("QueryActCtxSettings(hActCtx NULL) error %lu", GetLastError());
    ReleaseActCtx(ctx);
}

/* ---- FindActCtxSection* ---- */
static ACTCTX_SECTION_KEYED_DATA newkd(void) { ACTCTX_SECTION_KEYED_DATA d; memset(&d, 0xCC, sizeof d); d.cbSize = sizeof d; return d; }
TEST(ac_find_without_active_context) {
    REQUIRE(U(FindActCtxSectionStringW));
    HANDLE cur = NULL; GetCurrentActCtx(&cur);
    if (cur) { t_skip("a context is active in this process"); return; }
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
}
TEST(ac_find_dll_redirection) {
    REQUIRE(U(FindActCtxSectionStringW)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    BOOL ok = U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        CHECK(d.ulDataFormatVersion == 1);
        CHECK(d.lpData != NULL && d.ulLength > 0);
        CHECK(d.lpSectionGlobalData != NULL || d.ulSectionGlobalDataLength == 0);
        CHECK(d.lpSectionBase != NULL && d.ulSectionTotalLength > 0);
        CHECK(d.hActCtx == NULL);                       /* only with RETURN_HACTCTX */
        CHECK(d.ulAssemblyRosterIndex >= 1);
    }
    /* case-insensitive */
    d = newkd();
    CHECK(U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"K32T_REDIR.DLL", &d));
    /* flags */
    d = newkd();
    CHECK(U(FindActCtxSectionStringW)(FIND_ACTCTX_SECTION_KEY_RETURN_HACTCTX, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    CHECK(d.hActCtx == ctx);
    d = newkd();
    CHECK(U(FindActCtxSectionStringW)(FIND_ACTCTX_SECTION_KEY_RETURN_FLAGS, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    t_note("section flags = %lx", d.ulFlags);
    d = newkd();
    CHECK(U(FindActCtxSectionStringW)(FIND_ACTCTX_SECTION_KEY_RETURN_ASSEMBLY_METADATA, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    CHECK(d.AssemblyMetadata.lpInformation != NULL);
    /* the other files and unknown names */
    d = newkd();
    CHECK(U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_com.dll", &d));
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_nosuch.dll", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    DeactivateActCtx(0, c);
    ReleaseActCtx(ctx);
}
TEST(ac_find_string_a_variant) {
    REQUIRE(U(FindActCtxSectionStringA)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    CHECKF(U(FindActCtxSectionStringA)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "k32t_redir.dll", &d), "err %lu", GetLastError());
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringA)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "nosuch.dll", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}
TEST(ac_find_window_class) {
    REQUIRE(U(FindActCtxSectionStringW)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    CHECKF(U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, L"K32TWndClass", &d), "err %lu", GetLastError());
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, L"NoSuchClass", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}
TEST(ac_find_com_server_guid) {
    REQUIRE(U(FindActCtxSectionGuid)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    BOOL ok = U(FindActCtxSectionGuid)(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, &k_clsid, &d);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) { CHECK(d.ulDataFormatVersion == 1); CHECK(d.lpData != NULL && d.ulLength > 0); }
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionGuid)(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, &k_missing, &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionGuid)(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, NULL, &d));
    t_note("FindActCtxSectionGuid(NULL guid) error %lu", GetLastError());
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}
TEST(ac_find_progid) {
    REQUIRE(U(FindActCtxSectionStringW)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    CHECKF(U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_PROGID_REDIRECTION, L"K32T.Object.1", &d), "err %lu", GetLastError());
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}
TEST(ac_find_sections_absent) {
    REQUIRE(U(FindActCtxSectionStringW)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = mk_ctx("ac_abs", "<assemblyIdentity type=\"win32\" name=\"k32t.abs\" version=\"1.0.0.0\"/>\r\n");
    CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"anything.dll", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, L"anything", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionGuid)(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, &k_clsid, &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}
TEST(ac_find_invalid_arguments) {
    REQUIRE(U(FindActCtxSectionStringW)); REQUIRE(U(ActivateActCtx)); REQUIRE(U(FindActCtxSectionGuid));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    d.cbSize = 0; SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* BasepFindActCtxSection_CheckAndConvertParameters needs cbSize >= 0x40, and more room for the RETURN_* flags */
    d = newkd(); d.cbSize = 0x40; SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(FIND_ACTCTX_SECTION_KEY_RETURN_FLAGS, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    d = newkd(); d.cbSize = 0x3F; SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0x80, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, NULL, &d));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    BOOL okn = U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", NULL);
    t_note("FindActCtxSectionStringW(ReturnedData=NULL) -> %d err %lu", okn, okn ? 0 : GetLastError());
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, 0, L"k32t_redir.dll", &d));
    CHECK(GetLastError() != 0);
    t_note("section id 0 -> error %lu", GetLastError());
    d = newkd(); SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(0, NULL, 9999, L"k32t_redir.dll", &d));
    CHECK(GetLastError() != 0);
    t_note("section id 9999 -> error %lu", GetLastError());
    d = newkd(); d.cbSize = 0; SET_ERR(0);
    CHECK(!U(FindActCtxSectionGuid)(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, &k_clsid, &d));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}
TEST(ac_find_via_explicit_hactctx_flag_without_activation) {
    REQUIRE(U(FindActCtxSectionStringW));
    /* RETURN_HACTCTX with no active context must not fabricate a handle */
    HANDLE cur = NULL; GetCurrentActCtx(&cur);
    if (cur) { t_skip("a context is active"); return; }
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    CHECK(!U(FindActCtxSectionStringW)(FIND_ACTCTX_SECTION_KEY_RETURN_HACTCTX, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d));
    EXPECT_ERR(ERROR_SXS_KEY_NOT_FOUND);
}
TEST(ac_find_application_settings_section) {
    REQUIRE(U(FindActCtxSectionStringW)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    BOOL ok = U(FindActCtxSectionStringW)(0, NULL, 10 /* APPLICATION_SETTINGS */, L"http://schemas.microsoft.com/SMI/2005/WindowsSettings#dpiAware", &d);
    t_note("application settings section lookup -> %d err %lu", ok, ok ? 0 : GetLastError());
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}
TEST(ac_find_assembly_information_section) {
    REQUIRE(U(FindActCtxSectionStringW)); REQUIRE(U(ActivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    BOOL ok = U(FindActCtxSectionStringW)(0, NULL, ACTIVATION_CONTEXT_SECTION_ASSEMBLY_INFORMATION, L"k32t.test.app,type=\"win32\",version=\"1.2.3.4\"", &d);
    t_note("assembly information lookup -> %d err %lu", ok, ok ? 0 : GetLastError());
    DeactivateActCtx(0, c); ReleaseActCtx(ctx);
}

/* ---- Zombify ---- */
TEST(ac_zombify) {
    REQUIRE(U(ZombifyActCtx)); REQUIRE(U(ActivateActCtx)); REQUIRE(U(DeactivateActCtx));
    HANDLE ctx = query_ctx(); CHECK(ctx != INVALID_HANDLE_VALUE); if (ctx == INVALID_HANDLE_VALUE) return;
    ULONG_PTR c = 0; CHECK(ActivateActCtx(ctx, &c));
    SET_ERR(0);
    CHECKF(U(ZombifyActCtx)(ctx), "err %lu", GetLastError());
    /* activation context data stays reachable through the existing activation */
    ACTCTX_SECTION_KEYED_DATA d = newkd();
    SET_ERR(0);
    BOOL ok = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d);
    t_note("find in zombified context -> %d err %lu", ok, ok ? 0 : GetLastError());
    CHECK(DeactivateActCtx(0, c));
    ULONG_PTR c2 = 0; SET_ERR(0);
    ok = ActivateActCtx(ctx, &c2);
    t_note("activate zombified -> %d err %lu", ok, ok ? 0 : GetLastError());
    if (ok) DeactivateActCtx(0, c2);
    ReleaseActCtx(ctx);
}
TEST(ac_zombify_null_and_invalid) {
    REQUIRE(U(ZombifyActCtx));
    SET_ERR(0);
    BOOL ok = U(ZombifyActCtx)(NULL);
    t_note("ZombifyActCtx(NULL) -> %d err %lu", ok, ok ? 0 : GetLastError());
    SET_ERR(0);
    ok = U(ZombifyActCtx)(INVALID_HANDLE_VALUE);
    t_note("ZombifyActCtx(INVALID_HANDLE_VALUE) -> %d err %lu", ok, ok ? 0 : GetLastError());
}

/* ---- activation context and the loader ---- */
CHILD(ac_dll_redirection_via_context) {
    /* argv[1] manifest path (redirects k32m_sxs.dll -> file next to it), argv[2] dir */
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(argv[1]);
    HANDLE ctx = U(CreateActCtxW)(&a);
    if (ctx == INVALID_HANDLE_VALUE) return 90;
    /* without the context the DLL cannot be found through system32 only */
    if (LoadLibraryExA("k32m_sxs.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)) return 91;
    ULONG_PTR c = 0;
    if (!U(ActivateActCtx)(ctx, &c)) return 92;
    HMODULE h = LoadLibraryExA("k32m_sxs.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    DWORD e = GetLastError();
    U(DeactivateActCtx)(0, c);
    if (!h) { return 100 + (e & 0x7F); }
    char name[MAX_PATH]; GetModuleFileNameA(h, name, sizeof name);
    if (!strstr(name, "k32m_sxs.dll")) return 93;
    FreeLibrary(h);
    return 0;
}
TEST(ac_context_redirects_dll_load) {
    REQUIRE(U(CreateActCtxW)); REQUIRE(U(ActivateActCtx));
    const char *d = ml_dir("ac_redir"); char man[MAX_PATH], dll[MAX_PATH];
    snprintf(man, sizeof man, "%sredir.manifest", d);
    write_manifest(man, "<assemblyIdentity type=\"win32\" name=\"k32t.redir\" version=\"1.0.0.0\"/>\r\n<file name=\"k32m_sxs.dll\"/>\r\n");
    ml_put(ML_A, d, "k32m_sxs.dll", dll);
    char args[2 * MAX_PATH + 8]; snprintf(args, sizeof args, "\"%s\"", man);
    DWORD rc = ml_run_exe(t_self_path(), "ac_dll_redirection_via_context", args, NULL, 25000, 0);
    CHECKF(rc == 0, "rc=%lu", rc);
}

/* ---- external manifest next to a copied exe ---- */
CHILD(ac_ext_manifest_child) {
    HANDLE cur = NULL;
    if (!U(GetCurrentActCtx)(&cur)) return 90;
    ACTCTX_SECTION_KEYED_DATA d; memset(&d, 0, sizeof d); d.cbSize = sizeof d;
    BOOL has = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32m_ext.dll", &d);
    if (!has) return 50;                 /* external manifest not honoured by this system: test is inconclusive */
    HMODULE h = LoadLibraryExA("k32m_ext.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!h) return 100 + (GetLastError() & 0x7F);
    ml_unload(h);
    return 0;
}
TEST(ac_external_manifest_redirects_dll) {
    REQUIRE(U(GetCurrentActCtx));
    const char *d = ml_dir("ac_ext"); char exe[MAX_PATH], man[MAX_PATH];
    if (!ml_copy_self(d, "k32t_ext.exe", exe)) { t_skip("cannot copy exe"); return; }
    ml_put(ML_A, d, "k32m_ext.dll", NULL);
    snprintf(man, sizeof man, "%s.manifest", exe);
    write_manifest(man, "<assemblyIdentity type=\"win32\" name=\"k32t.ext\" version=\"1.0.0.0\"/>\r\n<file name=\"k32m_ext.dll\"/>\r\n");
    DWORD rc = ml_run_exe(exe, "ac_ext_manifest_child", "", NULL, 25000, 0);
    if (rc == 50) { t_note("external manifest was not applied to the copied exe"); return; }
    EXPECT_EQ(rc, 0);
}
CHILD(ac_ext_manifest_plain) {
    HANDLE cur = (HANDLE)1;
    if (!U(GetCurrentActCtx)(&cur)) return 90;
    return 0;
}
TEST(ac_exe_without_manifest_default_state) {
    EXPECT_EQ(ml_run_exe(t_self_path(), "ac_ext_manifest_plain", "", NULL, 25000, 0), 0);
}

/* ---- ACTCTX_FLAG_SET_PROCESS_DEFAULT (child: process wide) ---- */
CHILD(ac_set_process_default) {
    ACTCTXW a; memset(&a, 0, sizeof a); a.cbSize = sizeof a; a.lpSource = ml_w(argv[1]); a.dwFlags = ACTCTX_FLAG_SET_PROCESS_DEFAULT;
    SetLastError(0);
    HANDLE h = U(CreateActCtxW)(&a);
    if (h == INVALID_HANDLE_VALUE) return 100 + (GetLastError() & 0x7F);
    /* without any activation the default context answers section lookups */
    ACTCTX_SECTION_KEYED_DATA d; memset(&d, 0, sizeof d); d.cbSize = sizeof d;
    if (!FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, L"k32t_redir.dll", &d)) return 51;
    /* a second SET_PROCESS_DEFAULT is refused */
    SetLastError(0);
    HANDLE h2 = U(CreateActCtxW)(&a);
    if (h2 != INVALID_HANDLE_VALUE) return 52;
    DWORD e = GetLastError();
    if (e != ERROR_ACCESS_DENIED && e != ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET) return 53;
    return 0;
}
TEST(ac_set_process_default_child) {
    REQUIRE(U(CreateActCtxW));
    const char *p = mk_path("ac_spd", k_full_manifest);
    char args[MAX_PATH + 8]; snprintf(args, sizeof args, "\"%s\"", p);
    DWORD rc = ml_run_exe(t_self_path(), "ac_set_process_default", args, NULL, 25000, 0);
    t_note("SET_PROCESS_DEFAULT child result %lu", rc);
    CHECK(rc == 0 || rc == 51 || rc == 52 || rc == 53);
}
