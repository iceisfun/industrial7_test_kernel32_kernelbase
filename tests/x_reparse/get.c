/* FSCTL_GET_REPARSE_POINT: round trip, non-reparse targets, output-buffer sizing. */
#include "xrp.h"

/* GET on base\j (as the link) with an output buffer of `cap` bytes; *n BytesReturned, returns GLE */
static DWORD get_cap(const WCHAR *p, BYTE *out, DWORD cap, DWORD *n) {
    memset(out, 0xCC, cap + 16);
    return get_rp(p, cap ? out : NULL, cap, n);
}

TEST(xrp_get_round_trip_bytes) {
    BEGIN(xrp_get_round_trip_bytes);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"j")));
    rbuf r; mp_build(&r, P(L"t"));
    NEED(set_rp(P(L"j"), &r) == 0);
    BYTE out[4096]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"j"), out, sizeof out, &n), 0);
    EXPECT_EQ(n, r.len);
    CHECK(n == r.len && !memcmp(out, r.b, r.len));
    xrp_end();
}
TEST(xrp_get_names_decoded) {
    BEGIN(xrp_get_names_decoded);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    BYTE out[4096]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"j"), out, sizeof out, &n), 0);
    EXPECT_EQ(rd32(out), TAG_MP);
    EXPECT_EQ(rd16(out + 4) + 8u, n);
    USHORT so = rd16(out + 8), sl = rd16(out + 10), po = rd16(out + 12), pl = rd16(out + 14);
    WCHAR sub[XP], prn[XP], want[XP];
    memcpy(sub, out + 16 + so, sl); sub[sl / 2] = 0;
    memcpy(prn, out + 16 + po, pl); prn[pl / 2] = 0;
    snwprintf(want, XP, L"\\??\\%ls", P(L"t"));
    EXPECT_WEQ(sub, want);
    EXPECT_WEQ(prn, P(L"t"));
    CHECK(!wcsncmp(sub, L"\\??\\", 4));
    /* names are NUL terminated inside the stored data */
    EXPECT_EQ(rd16(out + 16 + so + sl), 0);
    EXPECT_EQ(rd16(out + 16 + po + pl), 0);
    xrp_end();
}
TEST(xrp_get_plain_dir_not_reparse) {
    BEGIN(xrp_get_plain_dir_not_reparse);
    NEED(mkdirw(P(L"d")));
    BYTE out[512]; DWORD n = 0xDEADBEEF;
    EXPECT_EQ(get_rp(P(L"d"), out, sizeof out, &n), ERROR_NOT_A_REPARSE_POINT);
    EXPECT_EQ(n, 0xDEADBEEF);                          /* BytesReturned is not written on failure */
    xrp_end();
}
TEST(xrp_get_plain_file_not_reparse) {
    BEGIN(xrp_get_plain_file_not_reparse);
    NEED(mkfilew(P(L"f.txt"), "plain"));
    HANDLE h = CreateFileW(P(L"f.txt"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    OK_H(h);
    BYTE out[512]; DWORD n, e = 0;
    CHECK(!xio(h, XFSCTL_GET, NULL, 0, out, sizeof out, &n, &e));
    EXPECT_EQ(e, ERROR_NOT_A_REPARSE_POINT);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_get_through_junction_reads_target) {
    BEGIN(xrp_get_through_junction_reads_target);
    NEED(std_layout());
    HANDLE h = open_dir(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);   /* no OPEN_REPARSE_POINT: this is t */
    BYTE out[512]; DWORD n, e = 0;
    CHECK(!xio(h, XFSCTL_GET, NULL, 0, out, sizeof out, &n, &e));
    EXPECT_EQ(e, ERROR_NOT_A_REPARSE_POINT);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_get_through_junction_to_junction) {
    BEGIN(xrp_get_through_junction_to_junction);
    /* j1 -> j2 -> t: a following open of j1 lands on t, which is not a reparse point */
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j2"), P(L"t")) && mkjunc(P(L"j1"), P(L"j2")));
    BYTE out[2048]; DWORD n = 0, e = 0;
    HANDLE h = open_dir(P(L"j1"), FILE_READ_ATTRIBUTES); OK_H(h);
    CHECK(!xio(h, XFSCTL_GET, NULL, 0, out, sizeof out, &n, &e));
    EXPECT_EQ(e, ERROR_NOT_A_REPARSE_POINT);
    CLOSE(h);
    /* as the link, j1 reports its own target (j2), not the end of the chain */
    EXPECT_EQ(get_rp(P(L"j1"), out, sizeof out, &n), 0);
    WCHAR prn[XP]; USHORT po = rd16(out + 12), pl = rd16(out + 14);
    memcpy(prn, out + 16 + po, pl); prn[pl / 2] = 0;
    EXPECT_WEQ(prn, P(L"j2"));
    xrp_end();
}
/* Short-buffer rule: below sizeof(REPARSE_GUID_DATA_BUFFER) (28 bytes, whatever the tag) the call
 * fails with ERROR_INSUFFICIENT_BUFFER and writes nothing; from 28 bytes up a truncated copy comes
 * back with ERROR_MORE_DATA and BytesReturned = buffer size. */
#define XMINOUT 28
TEST(xrp_get_output_zero) {
    BEGIN(xrp_get_output_zero);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    BYTE out[64]; DWORD n = 0xDEADBEEF;
    EXPECT_EQ(get_cap(P(L"j"), out, 0, &n), ERROR_INVALID_USER_BUFFER);     /* NULL output */
    EXPECT_EQ(n, 0xDEADBEEF);
    n = 0xDEADBEEF;
    EXPECT_EQ(get_rp(P(L"j"), out, 0, &n), ERROR_INVALID_USER_BUFFER);       /* non-NULL, zero length */
    EXPECT_EQ(n, 0xDEADBEEF);
    xrp_end();
}
TEST(xrp_get_output_below_minimum) {
    BEGIN(xrp_get_output_below_minimum);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    BYTE out[64];
    /* the 8-byte REPARSE_DATA_BUFFER header and the 24-byte GUID header are both not enough */
    for (DWORD cap = 1; cap < XMINOUT; cap++) {
        DWORD n = 0xDEADBEEF, e = get_cap(P(L"j"), out, cap, &n);
        CHECKF(e == ERROR_INSUFFICIENT_BUFFER && n == 0xDEADBEEF, "cap=%lu gle=%lu n=%lu", cap, e, n);
        CHECKF(out[0] == 0xCC, "cap=%lu buffer touched", cap);
    }
    xrp_end();
}
TEST(xrp_get_output_at_minimum) {
    BEGIN(xrp_get_output_at_minimum);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    BYTE out[64]; DWORD n = 0xDEADBEEF;
    EXPECT_EQ(get_cap(P(L"j"), out, XMINOUT, &n), ERROR_MORE_DATA);
    EXPECT_EQ(n, XMINOUT);
    EXPECT_EQ(rd32(out), TAG_MP);
    EXPECT_EQ(rd16(out + 4), r.len - 8);          /* the full data length is reported */
    CHECK(!memcmp(out, r.b, XMINOUT));
    EXPECT_EQ(out[XMINOUT], 0xCC);
    xrp_end();
}
TEST(xrp_get_output_partial) {
    BEGIN(xrp_get_output_partial);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    static BYTE out[4096];
    DWORD caps[] = {XMINOUT + 1, 32, 64, r.len - 2, r.len - 1};
    for (int i = 0; i < 5; i++) {
        DWORD n = 0xDEADBEEF, e = get_cap(P(L"j"), out, caps[i], &n);
        CHECKF(e == ERROR_MORE_DATA, "cap=%lu gle=%lu", caps[i], e);
        CHECKF(n == caps[i], "cap=%lu n=%lu", caps[i], n);
        CHECKF(n == caps[i] && !memcmp(out, r.b, caps[i]), "cap=%lu prefix differs", caps[i]);
        CHECKF(out[caps[i]] == 0xCC, "cap=%lu wrote past the buffer", caps[i]);
    }
    xrp_end();
}
TEST(xrp_get_output_exact_and_large) {
    BEGIN(xrp_get_output_exact_and_large);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    static BYTE out[XMAXRP + 64];
    DWORD n = 0;
    EXPECT_EQ(get_cap(P(L"j"), out, r.len, &n), 0);
    EXPECT_EQ(n, r.len);
    CHECK(!memcmp(out, r.b, r.len));
    EXPECT_EQ(get_cap(P(L"j"), out, XMAXRP, &n), 0);
    EXPECT_EQ(n, r.len);
    EXPECT_EQ(out[r.len], 0xCC);
    xrp_end();
}
TEST(xrp_get_guid_tag_size_rule) {
    BEGIN(xrp_get_guid_tag_size_rule);
    NEED(mkdirw(P(L"d")));
    rbuf r; g3_build(&r, TAG_3P, &xrp_guid, 32);
    NEED(set_rp(P(L"d"), &r) == 0);
    BYTE out[256]; DWORD n, e;
    /* the same 28-byte minimum as for a Microsoft tag; the GUID header alone (24) is not enough */
    DWORD small[] = {XHDR, 16, XGHDR, XMINOUT - 1};
    for (int i = 0; i < 4; i++) {
        n = 0xDEADBEEF; e = get_cap(P(L"d"), out, small[i], &n);
        CHECKF(e == ERROR_INSUFFICIENT_BUFFER && n == 0xDEADBEEF, "cap=%lu gle=%lu n=%lu", small[i], e, n);
    }
    n = 0xDEADBEEF; e = get_cap(P(L"d"), out, XMINOUT, &n);
    EXPECT_EQ(e, ERROR_MORE_DATA);
    EXPECT_EQ(n, XMINOUT);
    CHECK(!memcmp(out, r.b, XMINOUT));
    n = 0; e = get_cap(P(L"d"), out, r.len, &n);
    EXPECT_EQ(e, 0);
    EXPECT_EQ(n, r.len);
    CHECK(!memcmp(out, r.b, r.len));
    EXPECT_EQ(del_rp_tag(P(L"d"), TAG_3P), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_get_overlapped_handle) {
    BEGIN(xrp_get_overlapped_handle);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    HANDLE h = CreateFileW(P(L"j"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OVERLAPPED, NULL);
    OK_H(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = ev;
    BYTE out[2048]; DWORD n = 0;
    BOOL ok = U(DeviceIoControl)(h, XFSCTL_GET, NULL, 0, out, sizeof out, NULL, &ov);
    DWORD e = ok ? 0 : GetLastError();
    CHECKF(ok || e == ERROR_IO_PENDING, "gle %lu", e);
    EXPECT_EQ(WaitForSingleObject(ev, 10000), WAIT_OBJECT_0);
    CHECK(GetOverlappedResult(h, &ov, &n, FALSE));
    EXPECT_EQ(n, r.len);
    EXPECT_EQ(ov.Internal, 0);
    CHECK(!memcmp(out, r.b, r.len));
    /* truncated copy on the overlapped path: the warning status and the count land in the OVERLAPPED */
    ResetEvent(ev); memset(&ov, 0, sizeof ov); ov.hEvent = ev;
    ok = U(DeviceIoControl)(h, XFSCTL_GET, NULL, 0, out, XMINOUT, NULL, &ov);
    e = ok ? 0 : GetLastError();
    CHECK(!ok);
    EXPECT_EQ(e, ERROR_MORE_DATA);
    EXPECT_EQ(ov.Internal, 0x80000005);            /* STATUS_BUFFER_OVERFLOW */
    EXPECT_EQ(ov.InternalHigh, XMINOUT);
    /* too small: refused before any I/O status is stored */
    ResetEvent(ev); memset(&ov, 0, sizeof ov); ov.hEvent = ev;
    ok = U(DeviceIoControl)(h, XFSCTL_GET, NULL, 0, out, XHDR, NULL, &ov);
    e = ok ? 0 : GetLastError();
    CHECK(!ok);
    EXPECT_EQ(e, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(ov.InternalHigh, 0);
    CloseHandle(ev);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_get_needs_no_access) {
    BEGIN(xrp_get_needs_no_access);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    BYTE out[2048]; DWORD n = 0, e = 0;
    HANDLE h = open_link(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    { BOOL ok_ = xio(h, XFSCTL_GET, NULL, 0, out, sizeof out, &n, &e); CHECKF(ok_, "gle %lu", e); }
    CLOSE(h);
    /* a handle with only SYNCHRONIZE still reads the reparse data */
    h = open_link(P(L"j"), SYNCHRONIZE); OK_H(h);
    n = 0;
    { BOOL ok_ = xio(h, XFSCTL_GET, NULL, 0, out, sizeof out, &n, &e); CHECKF(ok_, "gle %lu", e); }
    EXPECT_EQ(n, r.len);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_get_input_buffer_refused) {
    BEGIN(xrp_get_input_buffer_refused);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    BYTE junk[64]; memset(junk, 0x77, sizeof junk);
    BYTE out[2048]; DWORD n = 0xDEADBEEF, e = 0;
    HANDLE h = open_link(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    /* GET takes no input: a non-empty input buffer is a parameter error */
    CHECK(!xio(h, XFSCTL_GET, junk, sizeof junk, out, sizeof out, &n, &e));
    EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(n, 0xDEADBEEF);
    /* an input pointer with zero length is fine */
    n = 0;
    { BOOL ok_ = xio(h, XFSCTL_GET, junk, 0, out, sizeof out, &n, &e); CHECKF(ok_, "gle %lu", e); }
    EXPECT_EQ(n, r.len);
    CLOSE(h);
    xrp_end();
}
