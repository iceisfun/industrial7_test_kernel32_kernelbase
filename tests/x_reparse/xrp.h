/* Shared helpers for the x_reparse area (all static).
 * Every test works in its own directory under the per-process scratch dir
 * (xrp_begin), every junction points inside that directory, and xrp_end
 * removes each reparse point with RemoveDirectoryW before deleting the
 * rest of the tree (it never descends through a reparse point). */
#ifndef XRP_H
#define XRP_H
#include "t.h"
#include <stdlib.h>
#include <wchar.h>
#include <winioctl.h>

/* Single-evaluation versions (the harness macros evaluate their arguments several times). */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#undef EXPECT_ERR
#define EXPECT_ERR(err) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #err, "GetLastError got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define EXPECT_WEQ(a, b) do { const WCHAR *a_ = (a), *b_ = (b); \
    t_check(!wcscmp(a_, b_), __FILE__, __LINE__, #a " == " #b, "wide string mismatch"); } while (0)
#define EXPECT_WIEQ(a, b) do { const WCHAR *a_ = (a), *b_ = (b); \
    t_check(!_wcsicmp(a_, b_), __FILE__, __LINE__, #a " ~= " #b, "wide string mismatch (case-insensitive)"); } while (0)

#define XSH (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
#define XP 1024                 /* wide path buffer size used throughout */
#define TAG_MP  0xA0000003u     /* IO_REPARSE_TAG_MOUNT_POINT */
#define TAG_SL  0xA000000Cu     /* IO_REPARSE_TAG_SYMLINK */
#define TAG_3P  0x00001234u     /* non-Microsoft tag (needs a GUID header) */
#define XFSCTL_SET 0x000900A4u  /* FSCTL_SET_REPARSE_POINT */
#define XFSCTL_GET 0x000900A8u  /* FSCTL_GET_REPARSE_POINT */
#define XFSCTL_DEL 0x000900ACu  /* FSCTL_DELETE_REPARSE_POINT */
#define XMAXRP (16 * 1024)      /* MAXIMUM_REPARSE_DATA_BUFFER_SIZE */
#define XHDR 8                  /* REPARSE_DATA_BUFFER_HEADER_SIZE */
#define XGHDR 24                /* REPARSE_GUID_DATA_BUFFER_HEADER_SIZE */

/* GUID used for the non-Microsoft tag buffers. */
static const GUID xrp_guid = {0x6b3f1c2e, 0x51a7, 0x4d0e, {0x9a, 0x51, 0x3c, 0x27, 0x8e, 0x10, 0x44, 0xd2}};
static const GUID xrp_guid2 = {0x0c1d2e3f, 0x4a5b, 0x4c6d, {0x8e, 0x9f, 0xa0, 0xb1, 0xc2, 0xd3, 0xe4, 0xf5}};

/* ---------- paths ---------- */
static WCHAR g_xbase[XP];
/* Fresh per-test directory: <scratch>\<name> (removed first if a previous run left it). */
static void xrp_rmtree(const WCHAR *dir);
static const WCHAR *xrp_begin(const char *name) {
    WCHAR t[XP];
    MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, t, XP);
    snwprintf(g_xbase, XP, L"%ls%hs", t, name);
    if (GetFileAttributesW(g_xbase) != INVALID_FILE_ATTRIBUTES) xrp_rmtree(g_xbase);
    CreateDirectoryW(g_xbase, NULL);
    return g_xbase;
}
static void xrp_end(void) { if (g_xbase[0]) xrp_rmtree(g_xbase); g_xbase[0] = 0; }
/* base\rel into a ring of buffers so several can be live in one expression */
static WCHAR *P(const WCHAR *rel) {
    static WCHAR ring[16][XP]; static int idx;
    WCHAR *o = ring[idx++ & 15];
    snwprintf(o, XP, L"%ls\\%ls", g_xbase, rel);
    return o;
}
static void xrp_acp(char *out, size_t cap, const WCHAR *w) { WideCharToMultiByte(CP_ACP, 0, w, -1, out, (int)cap, NULL, NULL); }

/* ---------- scaffolding (direct APIs) ---------- */
static int mkdirw(const WCHAR *p) { return CreateDirectoryW(p, NULL) != 0; }
static int mkfilew(const WCHAR *p, const char *body) {
    HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n = 0, len = body ? (DWORD)strlen(body) : 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (len) WriteFile(h, body, len, &n, NULL);
    CloseHandle(h);
    return n == len;
}
static int existsw(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
/* content of a file as a NUL-terminated string ("" on error, max 255 chars) */
static const char *readw(const WCHAR *p) {
    static char ring[4][256]; static int idx;
    char *o = ring[idx++ & 3]; DWORD n = 0;
    o[0] = 0;
    HANDLE h = CreateFileW(p, GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return "<open failed>";
    ReadFile(h, o, 255, &n, NULL); o[n] = 0;
    CloseHandle(h);
    return o;
}
#define EXPECT_BODY(p, s) do { const char *b_ = readw(p); \
    t_check(!strcmp(b_, (s)), __FILE__, __LINE__, "content of " #p, "got \"%s\" want \"%s\"", b_, (s)); } while (0)

/* Delete a tree without ever following a reparse point. */
static void xrp_rmtree(const WCHAR *dir) {
    WCHAR pat[XP], sub[XP]; WIN32_FIND_DATAW fd;
    DWORD a = GetFileAttributesW(dir);
    if (a == INVALID_FILE_ATTRIBUTES) return;
    if (a & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(dir, a & ~FILE_ATTRIBUTE_READONLY);
    if (a & FILE_ATTRIBUTE_REPARSE_POINT) {
        if (a & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(dir); else DeleteFileW(dir);
        return;
    }
    if (!(a & FILE_ATTRIBUTE_DIRECTORY)) { DeleteFileW(dir); return; }
    snwprintf(pat, XP, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            snwprintf(sub, XP, L"%ls\\%ls", dir, fd.cFileName);
            xrp_rmtree(sub);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir);
}

/* ---------- reparse buffers ---------- */
typedef struct { BYTE b[XMAXRP + 256]; DWORD len; } rbuf;
static USHORT rd16(const BYTE *p) { return (USHORT)(p[0] | (p[1] << 8)); }
static void wr16(BYTE *p, USHORT v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); }
static DWORD rd32(const BYTE *p) { return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24); }
static void wr32(BYTE *p, DWORD v) { wr16(p, (USHORT)v); wr16(p + 2, (USHORT)(v >> 16)); }

/* Mount-point buffer: header(8) + SubstOff SubstLen PrintOff PrintLen (8) + PathBuffer
 * laid out as Subst NUL Print NUL (the layout mklink /J uses). */
static DWORD mp_build_raw(rbuf *r, const WCHAR *subst, const WCHAR *print, int print_first) {
    USHORT S = (USHORT)(wcslen(subst) * 2), Pn = (USHORT)(wcslen(print) * 2);
    memset(r, 0, sizeof *r);
    BYTE *pb = r->b + 16;
    USHORT so, po;
    if (print_first) { po = 0; so = Pn + 2; } else { so = 0; po = S + 2; }
    memcpy(pb + so, subst, S); memcpy(pb + po, print, Pn);
    USHORT rdl = (USHORT)(8 + S + 2 + Pn + 2);
    wr32(r->b, TAG_MP); wr16(r->b + 4, rdl); wr16(r->b + 6, 0);
    wr16(r->b + 8, so); wr16(r->b + 10, S); wr16(r->b + 12, po); wr16(r->b + 14, Pn);
    r->len = 8u + rdl;
    return r->len;
}
/* Standard junction buffer for an absolute DOS target path: Subst "\??\<target>", Print "<target>". */
static DWORD mp_build(rbuf *r, const WCHAR *target) {
    WCHAR s[XP]; snwprintf(s, XP, L"\\??\\%ls", target);
    return mp_build_raw(r, s, target, 0);
}
/* Non-Microsoft tag buffer with GUID header and `n` data bytes. */
static DWORD g3_build(rbuf *r, DWORD tag, const GUID *g, DWORD n) {
    memset(r, 0, sizeof *r);
    wr32(r->b, tag); wr16(r->b + 4, (USHORT)n); wr16(r->b + 6, 0);
    memcpy(r->b + 8, g, sizeof *g);
    for (DWORD i = 0; i < n; i++) r->b[XGHDR + i] = (BYTE)(0x40 + i);
    r->len = XGHDR + n;
    return r->len;
}

/* ---------- handles ---------- */
static HANDLE open_link(const WCHAR *p, DWORD acc) {   /* the reparse point itself */
    return CreateFileW(p, acc, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
}
static HANDLE open_dir(const WCHAR *p, DWORD acc) {    /* follows reparse points */
    return CreateFileW(p, acc, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
}
#define CLOSE(h) do { if ((h) != INVALID_HANDLE_VALUE && (h) != NULL) CloseHandle(h); (h) = INVALID_HANDLE_VALUE; } while (0)
#define OK_H(h) t_check((h) != INVALID_HANDLE_VALUE, __FILE__, __LINE__, #h " valid", "GLE=%lu", GetLastError())

/* DeviceIoControl under test; returns BOOL, *err = GLE (0 on success), *ret = BytesReturned. */
static BOOL xio(HANDLE h, DWORD code, const void *in, DWORD inl, void *out, DWORD outl, DWORD *ret, DWORD *err) {
    DWORD r = 0xDEADBEEF;
    SetLastError(0xBEEF);
    BOOL ok = U(DeviceIoControl)(h, code, (void *)in, inl, out, outl, &r, NULL);
    if (err) *err = ok ? 0 : GetLastError();
    if (ret) *ret = r;
    return ok;
}
/* Set a buffer on path (opened as the link itself with GENERIC_WRITE); returns GLE (0 = success). */
static DWORD set_rp_len(const WCHAR *p, const void *buf, DWORD len) {
    HANDLE h = open_link(p, GENERIC_READ | GENERIC_WRITE);
    if (h == INVALID_HANDLE_VALUE) return 0xFFFF0000u | GetLastError();
    DWORD e = 0, r;
    xio(h, XFSCTL_SET, buf, len, NULL, 0, &r, &e);
    CloseHandle(h);
    return e;
}
static DWORD set_rp(const WCHAR *p, const rbuf *r) { return set_rp_len(p, r->b, r->len); }
/* Read the stored buffer through the link; returns GLE, fills out (cap bytes) and *n. */
static DWORD get_rp(const WCHAR *p, void *out, DWORD cap, DWORD *n) {
    HANDLE h = open_link(p, FILE_READ_ATTRIBUTES);
    if (h == INVALID_HANDLE_VALUE) return 0xFFFF0000u | GetLastError();
    DWORD e = 0;
    xio(h, XFSCTL_GET, NULL, 0, out, cap, n, &e);
    CloseHandle(h);
    return e;
}
static DWORD del_rp_tag(const WCHAR *p, DWORD tag) {
    BYTE hdr[XHDR]; memset(hdr, 0, sizeof hdr); wr32(hdr, tag);
    HANDLE h = open_link(p, GENERIC_READ | GENERIC_WRITE);
    if (h == INVALID_HANDLE_VALUE) return 0xFFFF0000u | GetLastError();
    DWORD e = 0, r;
    xio(h, XFSCTL_DEL, hdr, XHDR, NULL, 0, &r, &e);
    CloseHandle(h);
    return e;
}

/* Scaffolding junction: create empty dir `link`, point it at absolute `target`.
 * Uses the direct DeviceIoControl so that setup does not depend on the export under test. */
static int mkjunc(const WCHAR *link, const WCHAR *target) {
    rbuf r; mp_build(&r, target);
    if (!CreateDirectoryW(link, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    HANDLE h = CreateFileW(link, GENERIC_WRITE, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD n;
    BOOL ok = DeviceIoControl(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, NULL);
    CloseHandle(h);
    return ok;
}
/* Usual layout: base\t (target dir with file f.txt "target body" and dir sub), base\j -> base\t. */
static int std_layout(void) {
    if (!mkdirw(P(L"t")) || !mkfilew(P(L"t\\f.txt"), "target body") || !mkdirw(P(L"t\\sub"))) return 0;
    return mkjunc(P(L"j"), P(L"t"));
}
#define NEED(c) do { if (!(c)) { t_skip("setup failed: " #c " (gle %lu)", GetLastError()); xrp_end(); return; } } while (0)
#define BEGIN(name) xrp_begin(#name)

/* FILE_ID of a path: index (OPEN_REPARSE_POINT if link != 0). Returns 0 on failure. */
static ULONGLONG file_index(const WCHAR *p, int link) {
    HANDLE h = link ? open_link(p, FILE_READ_ATTRIBUTES) : open_dir(p, FILE_READ_ATTRIBUTES);
    BY_HANDLE_FILE_INFORMATION bi;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ULONGLONG v = 0;
    if (GetFileInformationByHandle(h, &bi)) v = ((ULONGLONG)bi.nFileIndexHigh << 32) | bi.nFileIndexLow;
    CloseHandle(h);
    return v;
}
/* Final path (normalized, DOS) of a path opened as link or followed. */
static int final_of(const WCHAR *p, int link, WCHAR *out, DWORD cap) {
    HANDLE h = link ? open_link(p, FILE_READ_ATTRIBUTES) : open_dir(p, FILE_READ_ATTRIBUTES);
    out[0] = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD n = GetFinalPathNameByHandleW(h, out, cap, 0);
    CloseHandle(h);
    return n > 0 && n < cap;
}
#endif
