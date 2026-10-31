/* FindFirstChangeNotification family and ReadDirectoryChangesW/ExW */
#include "dh.h"
#include <aclapi.h>

#ifndef FILE_ACTION_ADDED
#define FILE_ACTION_ADDED 1
#endif
UDECL(BOOL, ReadDirectoryChangesExW, (HANDLE, LPVOID, DWORD, BOOL, DWORD, LPDWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE, int));

static char *sub(char *buf, size_t cap, const char *dir, const char *leaf) { dh_path(buf, cap, dir, leaf); return buf; }

#define ALL_NAME (FILE_NOTIFY_CHANGE_FILE_NAME)
static HANDLE fcn(const char *dir, BOOL subtree, DWORD filter) { return U(FindFirstChangeNotificationA)(dir, subtree, filter); }
static int signaled(HANDLE h, DWORD ms) { return WaitForSingleObject(h, ms) == WAIT_OBJECT_0; }

/* ---- FindFirstChangeNotification ---- */
TEST(notify_fcn_signals_on_create) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindNextChangeNotification)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_create");
    HANDLE h = fcn(d, FALSE, ALL_NAME);
    CHECK(h != INVALID_HANDLE_VALUE && h != NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    CHECK(!signaled(h, 100));                       /* nothing happened yet */
    dh_mkfile(d, "new.txt", 1);
    CHECK(signaled(h, 5000));
    CHECK(signaled(h, 0));                          /* manual-reset style: stays signalled until FindNext */
    Sleep(100);
    CHECK(U(FindNextChangeNotification)(h));
    CHECK(!signaled(h, 300));
    CHECK(U(FindCloseChangeNotification)(h));
}
TEST(notify_fcn_w_signals_on_delete) {
    REQUIRE(U(FindFirstChangeNotificationW)); REQUIRE(U(FindNextChangeNotification)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_delete"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    dh_mkfile(d, "victim", 1); dh_w(w, MAX_PATH, d);
    HANDLE h = U(FindFirstChangeNotificationW)(w, FALSE, ALL_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CHECK(!signaled(h, 100));
    DeleteFileA(sub(p, sizeof p, d, "victim"));
    CHECK(signaled(h, 5000));
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_signals_on_rename) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_rename"), p[MAX_PATH], q[MAX_PATH];
    dh_mkfile(d, "old", 1);
    HANDLE h = fcn(d, FALSE, ALL_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    MoveFileA(sub(p, sizeof p, d, "old"), sub(q, sizeof q, d, "new"));
    CHECK(signaled(h, 5000));
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_file_name_filter_ignores_dirs_and_writes) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_filter1"), p[MAX_PATH];
    dh_mkfile(d, "existing", 1);
    HANDLE h = fcn(d, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    dh_mkdir(d, "newdir");                           /* directory name change: not in this filter */
    CHECK(!signaled(h, 400));
    sub(p, sizeof p, d, "existing");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    DWORD w; WriteFile(f, "more data", 9, &w, NULL); CloseHandle(f);
    CHECK(!signaled(h, 400));                        /* content change: not in this filter */
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_dir_name_filter) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_filter2");
    HANDLE h = fcn(d, FALSE, FILE_NOTIFY_CHANGE_DIR_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    dh_mkfile(d, "f", 1);
    CHECK(!signaled(h, 400));
    dh_mkdir(d, "sub");
    CHECK(signaled(h, 5000));
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_attributes_filter) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_attr"), p[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f");
    HANDLE h = fcn(d, FALSE, FILE_NOTIFY_CHANGE_ATTRIBUTES);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    CHECK(signaled(h, 5000));
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_last_write_filter) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_lw"), p[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f");
    HANDLE h = fcn(d, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    DWORD w; WriteFile(f, "zzz", 3, &w, NULL); FlushFileBuffers(f); CloseHandle(f);
    CHECK(signaled(h, 5000));
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_size_filter) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_size"), p[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f");
    HANDLE h = fcn(d, FALSE, FILE_NOTIFY_CHANGE_SIZE);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    char buf[8192] = {0}; DWORD w; WriteFile(f, buf, sizeof buf, &w, NULL); FlushFileBuffers(f); CloseHandle(f);
    int s = signaled(h, 5000);
    if (!s) t_note("FILE_NOTIFY_CHANGE_SIZE did not signal within 5s after write+flush+close");
    CHECK(s);
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_subtree_flag) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_subtree"), sd[MAX_PATH];
    dh_mkdir(d, "a"); sub(sd, sizeof sd, d, "a");
    HANDLE h1 = fcn(d, FALSE, ALL_NAME), h2 = fcn(d, TRUE, ALL_NAME);
    CHECK(h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE);
    if (h1 == INVALID_HANDLE_VALUE || h2 == INVALID_HANDLE_VALUE) return;
    dh_mkfile(sd, "deep.txt", 1);
    CHECK(signaled(h2, 5000));
    CHECK(!signaled(h1, 400));
    dh_mkfile(d, "top.txt", 1);
    CHECK(signaled(h1, 5000));
    U(FindCloseChangeNotification)(h1); U(FindCloseChangeNotification)(h2);
}
TEST(notify_fcn_subtree_deep_levels) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_deep"), a[MAX_PATH], b[MAX_PATH], c[MAX_PATH];
    dh_mkdir(d, "x"); sub(a, sizeof a, d, "x"); dh_mkdir(a, "y"); sub(b, sizeof b, a, "y"); dh_mkdir(b, "z"); sub(c, sizeof c, b, "z");
    HANDLE h = fcn(d, TRUE, ALL_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    dh_mkfile(c, "leaf", 1);
    CHECK(signaled(h, 5000));
    U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_missing_directory) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindFirstChangeNotificationW));
    char *d = dh_dir("nf_missing"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "nosuch"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(fcn(p, FALSE, ALL_NAME) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(FindFirstChangeNotificationW)(w, FALSE, ALL_NAME) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    sub(p, sizeof p, d, "nosuch\\deeper");
    CHECK(fcn(p, FALSE, ALL_NAME) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(notify_fcn_path_is_file) {
    REQUIRE(U(FindFirstChangeNotificationA));
    char *d = dh_dir("nf_isfile"), p[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f");
    SET_ERR(0);
    CHECK(fcn(p, FALSE, ALL_NAME) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_DIRECTORY);
}
TEST(notify_fcn_empty_and_null_name) {
    REQUIRE(U(FindFirstChangeNotificationW));
    SET_ERR(0);
    CHECK(U(FindFirstChangeNotificationW)(L"", FALSE, ALL_NAME) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0);
    HANDLE h = U(FindFirstChangeNotificationW)(NULL, FALSE, ALL_NAME);
    CHECK(h == INVALID_HANDLE_VALUE);
    t_note("FindFirstChangeNotificationW(NULL) err=%lu", GetLastError());
}
TEST(notify_fcn_invalid_filter) {
    REQUIRE(U(FindFirstChangeNotificationA));
    char *d = dh_dir("nf_badfilter");
    SET_ERR(0);
    CHECK(fcn(d, FALSE, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(fcn(d, FALSE, 0x80000000) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    HANDLE h = fcn(d, FALSE, 0xFFFFFFFF);
    CHECK(h == INVALID_HANDLE_VALUE);
    t_note("filter 0xFFFFFFFF: err=%lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(FindCloseChangeNotification)(h);
}
TEST(notify_fcn_all_documented_filters_accepted) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_allfilters");
    DWORD fl[] = { FILE_NOTIFY_CHANGE_FILE_NAME, FILE_NOTIFY_CHANGE_DIR_NAME, FILE_NOTIFY_CHANGE_ATTRIBUTES, FILE_NOTIFY_CHANGE_SIZE,
                   FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_NOTIFY_CHANGE_LAST_ACCESS, FILE_NOTIFY_CHANGE_CREATION, FILE_NOTIFY_CHANGE_SECURITY,
                   0x17F, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        HANDLE h = fcn(d, i & 1, fl[i]);
        CHECKF(h != INVALID_HANDLE_VALUE, "filter 0x%lx err %lu", fl[i], GetLastError());
        if (h != INVALID_HANDLE_VALUE) CHECK(U(FindCloseChangeNotification)(h));
    }
}
TEST(notify_fcn_double_close) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_dblclose");
    HANDLE h = fcn(d, FALSE, ALL_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CHECK(U(FindCloseChangeNotification)(h));
    SET_ERR(0);
    CHECK(!U(FindCloseChangeNotification)(h));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(notify_fcn_next_on_null) {
    REQUIRE(U(FindNextChangeNotification));
    SET_ERR(0);
    CHECK(!U(FindNextChangeNotification)(NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(notify_fcn_next_on_event_handle) {
    REQUIRE(U(FindNextChangeNotification));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    BOOL ok = U(FindNextChangeNotification)(ev);
    t_note("FindNextChangeNotification(event) ok=%d err=%lu", ok, GetLastError());
    CHECK(!ok);
    CloseHandle(ev);
}
TEST(notify_fcn_close_on_wrong_handle_type_closes_it) {
    REQUIRE(U(FindCloseChangeNotification));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(U(FindCloseChangeNotification)(ev));     /* plain NtClose: any handle type */
    SET_ERR(0);
    CHECK(!SetEvent(ev));
}
TEST(notify_fcn_wait_multiple) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d1 = dh_dir("nf_wm1"), *d2 = dh_dir("nf_wm2");
    HANDLE h[2] = { fcn(d1, FALSE, ALL_NAME), fcn(d2, FALSE, ALL_NAME) };
    CHECK(h[0] != INVALID_HANDLE_VALUE && h[1] != INVALID_HANDLE_VALUE);
    if (h[0] == INVALID_HANDLE_VALUE || h[1] == INVALID_HANDLE_VALUE) return;
    CHECK(WaitForMultipleObjects(2, h, FALSE, 150) == WAIT_TIMEOUT);
    dh_mkfile(d2, "x", 1);
    EXPECT_EQ(WaitForMultipleObjects(2, h, FALSE, 5000), WAIT_OBJECT_0 + 1);
    U(FindCloseChangeNotification)(h[0]); U(FindCloseChangeNotification)(h[1]);
}
TEST(notify_fcn_unicode_directory) {
    REQUIRE(U(FindFirstChangeNotificationW)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_uni"); WCHAR dw[MAX_PATH], sw[MAX_PATH];
    dh_w(dw, MAX_PATH, d);
    snwprintf(sw, MAX_PATH, L"%ls\\日本", dw);
    CreateDirectoryW(sw, NULL);
    HANDLE h = U(FindFirstChangeNotificationW)(sw, FALSE, ALL_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        dh_mkfileW(sw, L"café", 1);
        CHECK(signaled(h, 5000));
        U(FindCloseChangeNotification)(h);
    }
    dh_rmtreeW(sw);
}
TEST(notify_fcn_watched_directory_deleted) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_rmwatched"), w[MAX_PATH];
    dh_mkdir(d, "watched"); sub(w, sizeof w, d, "watched");
    HANDLE h = fcn(w, FALSE, ALL_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    SET_ERR(0);
    BOOL ok = RemoveDirectoryA(w);
    t_note("RemoveDirectory on a directory held by FindFirstChangeNotification: ok=%d err=%lu", ok, GetLastError());
    if (ok) t_note("watch signalled after delete: %d", signaled(h, 2000));
    U(FindCloseChangeNotification)(h);
    if (!ok) RemoveDirectoryA(w);
}
TEST(notify_fcn_many_handles) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_many"); HANDLE hs[64]; int i;
    for (i = 0; i < 64; i++) { hs[i] = fcn(d, FALSE, ALL_NAME); if (hs[i] == INVALID_HANDLE_VALUE) break; }
    EXPECT_EQ(i, 64);
    dh_mkfile(d, "boom", 1);
    for (int j = 0; j < i; j++) { CHECKF(signaled(hs[j], 5000), "handle %d", j); U(FindCloseChangeNotification)(hs[j]); }
}
TEST(notify_fcn_watch_prevents_nothing_else) {
    REQUIRE(U(FindFirstChangeNotificationA)); REQUIRE(U(FindCloseChangeNotification));
    char *d = dh_dir("nf_share"), p[MAX_PATH];
    HANDLE h = fcn(d, FALSE, ALL_NAME);
    CHECK(h != INVALID_HANDLE_VALUE);
    dh_mkfile(d, "f", 1);
    sub(p, sizeof p, d, "f");
    CHECK(DeleteFileA(p));
    HANDLE o = CreateFileA(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(o != INVALID_HANDLE_VALUE);
    if (o != INVALID_HANDLE_VALUE) CloseHandle(o);
    if (h != INVALID_HANDLE_VALUE) U(FindCloseChangeNotification)(h);
}

/* ---- ReadDirectoryChangesW ---- */
typedef struct { DWORD action; char name[300]; } ev_t;
typedef struct { HANDLE h; OVERLAPPED ov; DWORD buf[8192]; } watch_t;

static HANDLE open_dir(const char *dir, DWORD access, DWORD flags) {
    return CreateFileA(dir, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | flags, NULL);
}
static watch_t *watch_new(const char *dir) {
    watch_t *w = calloc(1, sizeof *w);
    w->h = open_dir(dir, FILE_LIST_DIRECTORY, FILE_FLAG_OVERLAPPED);
    w->ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    return w;
}
static void watch_free(watch_t *w) { if (w->h != INVALID_HANDLE_VALUE) CloseHandle(w->h); CloseHandle(w->ov.hEvent); free(w); }
static BOOL watch_start(watch_t *w, BOOL subtree, DWORD filter, DWORD size) {
    ResetEvent(w->ov.hEvent);
    w->ov.Internal = 0;
    return U(ReadDirectoryChangesW)(w->h, w->buf, size, subtree, filter, NULL, &w->ov, NULL);
}
/* wait up to ms; returns number of events, -1 on timeout, -2 on failure; *bytes receives byte count */
static int watch_get(watch_t *w, DWORD ms, ev_t *ev, int max, DWORD *bytes) {
    if (WaitForSingleObject(w->ov.hEvent, ms) != WAIT_OBJECT_0) return -1;
    DWORD n = 0;
    if (!GetOverlappedResult(w->h, &w->ov, &n, FALSE)) return -2;
    if (bytes) *bytes = n;
    int cnt = 0; BYTE *p = (BYTE *)w->buf;
    if (!n) return 0;
    for (;;) {
        FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION *)p;
        if (cnt < max) {
            ev[cnt].action = fni->Action;
            int l = WideCharToMultiByte(CP_UTF8, 0, fni->FileName, fni->FileNameLength / 2, ev[cnt].name, sizeof ev[cnt].name - 1, NULL, NULL);
            ev[cnt].name[l] = 0;
        }
        cnt++;
        if (!fni->NextEntryOffset) break;
        p += fni->NextEntryOffset;
    }
    return cnt;
}
static int has_ev(ev_t *e, int n, DWORD action, const char *name) {
    for (int i = 0; i < n && i < 32; i++) if (e[i].action == action && !strcmp(e[i].name, name)) return 1;
    return 0;
}
/* collect events until idle for quiet_ms; re-arms */
static int collect(watch_t *w, BOOL subtree, DWORD filter, DWORD quiet_ms, ev_t *out, int max) {
    int total = 0;
    for (;;) {
        ev_t e[64]; DWORD b;
        int n = watch_get(w, quiet_ms, e, 64, &b);
        if (n < 0) return n == -1 ? total : -2;
        for (int i = 0; i < n && i < 64; i++) if (total < max) out[total++] = e[i];
        if (!watch_start(w, subtree, filter, sizeof w->buf)) return -2;
    }
}

TEST(notify_rdc_create_delete_events) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_basic"), p[MAX_PATH];
    watch_t *w = watch_new(d);
    CHECK(w->h != INVALID_HANDLE_VALUE);
    if (w->h == INVALID_HANDLE_VALUE) { watch_free(w); return; }
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    CHECK(WaitForSingleObject(w->ov.hEvent, 150) == WAIT_TIMEOUT);
    dh_mkfile(d, "created.txt", 0);
    ev_t e[8]; DWORD b = 0;
    int n = watch_get(w, 5000, e, 8, &b);
    CHECK(n >= 1);
    if (n >= 1) { EXPECT_EQ(e[0].action, FILE_ACTION_ADDED); EXPECT_STR(e[0].name, "created.txt"); CHECK(b >= 12 + 2 * 11); }
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    DeleteFileA(sub(p, sizeof p, d, "created.txt"));
    n = watch_get(w, 5000, e, 8, &b);
    CHECK(n >= 1);
    if (n >= 1) { EXPECT_EQ(e[0].action, FILE_ACTION_REMOVED); EXPECT_STR(e[0].name, "created.txt"); }
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_rename_old_new_pair) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_rename"), p[MAX_PATH], q[MAX_PATH];
    dh_mkfile(d, "before", 1);
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    MoveFileA(sub(p, sizeof p, d, "before"), sub(q, sizeof q, d, "after"));
    ev_t e[8]; DWORD b;
    int n = watch_get(w, 5000, e, 8, &b);
    EXPECT_EQ(n, 2);
    if (n == 2) {
        EXPECT_EQ(e[0].action, FILE_ACTION_RENAMED_OLD_NAME); EXPECT_STR(e[0].name, "before");
        EXPECT_EQ(e[1].action, FILE_ACTION_RENAMED_NEW_NAME); EXPECT_STR(e[1].name, "after");
    }
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_modify_event) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_modify"), p[MAX_PATH];
    dh_mkfile(d, "m.txt", 1); sub(p, sizeof p, d, "m.txt");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE, sizeof w->buf));
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); DWORD wr;
    WriteFile(f, "abcdef", 6, &wr, NULL); FlushFileBuffers(f); CloseHandle(f);
    ev_t e[8]; DWORD b;
    int n = watch_get(w, 5000, e, 8, &b);
    CHECK(n >= 1);
    if (n >= 1) CHECK(has_ev(e, n, FILE_ACTION_MODIFIED, "m.txt"));
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_attribute_change_is_modified) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_attr"), p[MAX_PATH];
    dh_mkfile(d, "a.txt", 1); sub(p, sizeof p, d, "a.txt");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_ATTRIBUTES, sizeof w->buf));
    SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN);
    ev_t e[8]; DWORD b;
    int n = watch_get(w, 5000, e, 8, &b);
    CHECK(n >= 1);
    if (n >= 1) CHECK(has_ev(e, n, FILE_ACTION_MODIFIED, "a.txt"));
    CancelIoEx(w->h, NULL);
    watch_free(w);
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
}
TEST(notify_rdc_directory_events_need_dir_filter) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_dirs"), p[MAX_PATH];
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    dh_mkdir(d, "newdir");
    ev_t e[8]; DWORD b;
    CHECK(watch_get(w, 400, e, 8, &b) == -1);          /* file-name filter does not report directories */
    CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 1000);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_DIR_NAME, sizeof w->buf));
    RemoveDirectoryA(sub(p, sizeof p, d, "newdir"));
    int n = watch_get(w, 5000, e, 8, &b);
    CHECK(n >= 1);
    if (n >= 1) { EXPECT_EQ(e[0].action, FILE_ACTION_REMOVED); EXPECT_STR(e[0].name, "newdir"); }
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_subtree_names_are_relative) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_subtree"), sd[MAX_PATH];
    dh_mkdir(d, "s1"); sub(sd, sizeof sd, d, "s1"); dh_mkdir(sd, "s2"); sub(sd, sizeof sd, sd, "s2");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    dh_mkfile(sd, "leaf.txt", 0);
    ev_t e[8]; DWORD b;
    int n = watch_get(w, 5000, e, 8, &b);
    CHECK(n >= 1);
    if (n >= 1) { EXPECT_EQ(e[0].action, FILE_ACTION_ADDED); EXPECT_STR(e[0].name, "s1\\s2\\leaf.txt"); }
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_no_subtree_ignores_children) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_nosub"), sd[MAX_PATH];
    dh_mkdir(d, "kid"); sub(sd, sizeof sd, d, "kid");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    dh_mkfile(sd, "in_kid", 0);
    ev_t e[8]; DWORD b;
    CHECK(watch_get(w, 500, e, 8, &b) == -1);
    dh_mkfile(d, "top", 0);
    int n = watch_get(w, 5000, e, 8, &b);
    CHECK(n >= 1);
    if (n >= 1) EXPECT_STR(e[0].name, "top");
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_multiple_events_in_one_buffer) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_multi");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    for (int i = 0; i < 5; i++) { char n[16]; snprintf(n, sizeof n, "multi%d", i); dh_mkfile(d, n, 0); }
    Sleep(300);
    ev_t e[16]; DWORD b;
    int n = watch_get(w, 5000, e, 16, &b);
    CHECK(n >= 1);
    int total = n < 0 ? 0 : n;
    while (total < 5) {
        if (!watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf)) break;
        int m = watch_get(w, 2000, e + total, 16 - total, &b);
        if (m <= 0) break;
        total += m;
    }
    CHECKF(total == 5, "events %d", total);
    for (int i = 0; i < 5 && total >= 5; i++) { char nm[16]; snprintf(nm, sizeof nm, "multi%d", i); CHECKF(has_ev(e, 5, FILE_ACTION_ADDED, nm), "missing %s", nm); }
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_next_entry_offsets_aligned) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_align");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    dh_mkfile(d, "a", 0); dh_mkfile(d, "bb", 0); dh_mkfile(d, "ccc", 0);
    Sleep(300);
    DWORD b = 0;
    if (WaitForSingleObject(w->ov.hEvent, 5000) != WAIT_OBJECT_0) { CHECK(0); CancelIoEx(w->h, NULL); watch_free(w); return; }
    GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    BYTE *p = (BYTE *)w->buf; int n = 0;
    for (;;) {
        FILE_NOTIFY_INFORMATION *f = (FILE_NOTIFY_INFORMATION *)p; n++;
        CHECK(((ULONG_PTR)p & 3) == 0);
        CHECK(f->FileNameLength > 0 && (f->FileNameLength & 1) == 0);
        if (!f->NextEntryOffset) { CHECK((DWORD)(p - (BYTE *)w->buf) + 12 + f->FileNameLength <= b); break; }
        CHECK((f->NextEntryOffset & 3) == 0 && f->NextEntryOffset >= 12 + f->FileNameLength);
        p += f->NextEntryOffset;
    }
    CHECK(n >= 1);
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdc_overflow_small_buffer) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_overflow");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, 16));
    dh_mkfile(d, "a_file_name_that_is_far_too_long_for_the_buffer.txt", 0);
    if (WaitForSingleObject(w->ov.hEvent, 5000) != WAIT_OBJECT_0) { CHECK(0); CancelIoEx(w->h, NULL); watch_free(w); return; }
    DWORD b = 0xdead;
    SET_ERR(0);
    BOOL ok = GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    DWORD e = GetLastError();
    t_note("overflow: GetOverlappedResult ok=%d bytes=%lu err=%lu (ERROR_NOTIFY_ENUM_DIR=1022)", ok, b, e);
    if (!ok) t_note("overflow reported as failure");
    EXPECT_EQ(b, 0);
    watch_free(w);
}
TEST(notify_rdc_overflow_sync_return_value) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_overflow2");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, 16));
    dh_mkfile(d, "x_overflowing_entry_with_a_really_long_name_0123456789.txt", 0);
    CHECK(WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0);
    EXPECT_EQ(w->ov.InternalHigh, 0);
    t_note("overflow: Internal status = 0x%llx", (unsigned long long)w->ov.Internal);
    watch_free(w);
}
TEST(notify_rdc_pending_state) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_pending");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    CHECK(!HasOverlappedIoCompleted(&w->ov));
    DWORD b = 0;
    SET_ERR(0);
    CHECK(!GetOverlappedResult(w->h, &w->ov, &b, FALSE));
    EXPECT_ERR(ERROR_IO_INCOMPLETE);
    CancelIoEx(w->h, NULL);
    WaitForSingleObject(w->ov.hEvent, 2000);
    CHECK(HasOverlappedIoCompleted(&w->ov));
    watch_free(w);
}
TEST(notify_rdc_cancel_gives_operation_aborted) {
    REQUIRE(U(ReadDirectoryChangesW)); REQUIRE(U(CancelIoEx));
    char *d = dh_dir("rd_cancel");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    CHECK(U(CancelIoEx)(w->h, &w->ov));
    CHECK(WaitForSingleObject(w->ov.hEvent, 3000) == WAIT_OBJECT_0);
    DWORD b = 0;
    SET_ERR(0);
    CHECK(!GetOverlappedResult(w->h, &w->ov, &b, FALSE));
    EXPECT_ERR(ERROR_OPERATION_ABORTED);
    watch_free(w);
}
TEST(notify_rdc_close_handle_with_pending_request) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_closepending");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    CloseHandle(w->h); w->h = INVALID_HANDLE_VALUE;
    CHECK(WaitForSingleObject(w->ov.hEvent, 3000) == WAIT_OBJECT_0);
    t_note("pending request after handle close: Internal=0x%llx InternalHigh=%llu", (unsigned long long)w->ov.Internal, (unsigned long long)w->ov.InternalHigh);
    watch_free(w);
}
TEST(notify_rdc_invalid_handle) {
    REQUIRE(U(ReadDirectoryChangesW));
    static DWORD buf[256];
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(ReadDirectoryChangesW)(INVALID_HANDLE_VALUE, buf, sizeof buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(ReadDirectoryChangesW)(NULL, buf, sizeof buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ov.hEvent);
}
TEST(notify_rdc_invalid_filter) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_badfilter");
    watch_t *w = watch_new(d);
    SET_ERR(0);
    BOOL ok = watch_start(w, FALSE, 0, sizeof w->buf);
    DWORD e = GetLastError();
    if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    CHECK(!ok);
    EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    ok = watch_start(w, FALSE, 0x80000000, sizeof w->buf);
    e = GetLastError();
    if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    CHECK(!ok);
    EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    watch_free(w);
}
TEST(notify_rdc_handle_without_list_access) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_noaccess");
    watch_t *w = watch_new(d);
    CloseHandle(w->h);
    w->h = open_dir(d, FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_FLAG_OVERLAPPED);
    CHECK(w->h != INVALID_HANDLE_VALUE);
    if (w->h == INVALID_HANDLE_VALUE) { watch_free(w); return; }
    SET_ERR(0);
    BOOL ok = watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf);
    DWORD e = GetLastError();
    if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    if (!t_is_wine()) { CHECK(!ok); EXPECT_EQ(e, ERROR_ACCESS_DENIED); }
    watch_free(w);
}
TEST(notify_rdc_file_handle) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_filehandle"), p[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f");
    watch_t *w = watch_new(d);
    CloseHandle(w->h);
    w->h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    CHECK(w->h != INVALID_HANDLE_VALUE);
    if (w->h == INVALID_HANDLE_VALUE) { watch_free(w); return; }
    SET_ERR(0);
    BOOL ok = watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf);
    DWORD e = GetLastError();
    t_note("ReadDirectoryChangesW on a file handle: ok=%d err=%lu", ok, e);
    if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    CHECK(!ok);
    watch_free(w);
}
TEST(notify_rdc_non_directory_event_handle) {
    REQUIRE(U(ReadDirectoryChangesW));
    static DWORD buf[256];
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    BOOL ok = U(ReadDirectoryChangesW)(ev, buf, sizeof buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL);
    t_note("ReadDirectoryChangesW on an event handle: ok=%d err=%lu", ok, GetLastError());
    CHECK(!ok);
    CloseHandle(ev); CloseHandle(ov.hEvent);
}
TEST(notify_rdc_unaligned_buffer) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_unaligned");
    watch_t *w = watch_new(d);
    SET_ERR(0);
    BOOL ok = U(ReadDirectoryChangesW)(w->h, (BYTE *)w->buf + 1, 1024, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL);
    t_note("unaligned buffer: ok=%d err=%lu", ok, GetLastError());
    if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    watch_free(w);
}
TEST(notify_rdc_zero_length_buffer) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_zerolen");
    watch_t *w = watch_new(d);
    SET_ERR(0);
    BOOL ok = U(ReadDirectoryChangesW)(w->h, w->buf, 0, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL);
    t_note("zero-length buffer: ok=%d err=%lu", ok, GetLastError());
    if (ok) {
        dh_mkfile(d, "trigger", 0);
        DWORD r = WaitForSingleObject(w->ov.hEvent, 3000);
        DWORD b = 7; BOOL g = GetOverlappedResult(w->h, &w->ov, &b, FALSE);
        t_note("zero-length: wait=%lu GetOverlappedResult=%d bytes=%lu err=%lu", r, g, b, GetLastError());
        if (r != WAIT_OBJECT_0) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    }
    watch_free(w);
}
TEST(notify_rdc_not_directory_handle_flag_missing) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_syncwait");
    /* handle opened without FILE_FLAG_OVERLAPPED and an OVERLAPPED passed: still pending-async capable? note behaviour */
    HANDLE h = open_dir(d, FILE_LIST_DIRECTORY, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    static DWORD buf[1024];
    SET_ERR(0);
    BOOL ok = U(ReadDirectoryChangesW)(h, buf, sizeof buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL);
    t_note("non-overlapped handle + OVERLAPPED: ok=%d err=%lu internal=0x%llx", ok, GetLastError(), (unsigned long long)ov.Internal);
    if (ok) { dh_mkfile(d, "go", 0); WaitForSingleObject(ov.hEvent, 3000); }
    CloseHandle(h); CloseHandle(ov.hEvent);
}
typedef struct { char dir[MAX_PATH]; volatile LONG stop; } maker_t;
static DWORD WINAPI maker_thread(LPVOID p) {
    maker_t *m = p;
    Sleep(300);
    for (int i = 0; i < 20 && !m->stop; i++) { char n[32]; snprintf(n, sizeof n, "sync%d", i); dh_mkfile(m->dir, n, 0); Sleep(400); }
    return 0;
}
TEST(notify_rdc_synchronous_call) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_sync");
    HANDLE h = open_dir(d, FILE_LIST_DIRECTORY, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    static maker_t m; strcpy(m.dir, d); m.stop = 0;
    HANDLE th = CreateThread(NULL, 0, maker_thread, &m, 0, NULL);
    DWORD buf[1024], got = 0;
    BOOL ok = U(ReadDirectoryChangesW)(h, buf, sizeof buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, &got, NULL, NULL);
    InterlockedExchange(&m.stop, 1);
    CHECK(ok);
    CHECK(got >= 12);
    if (ok && got >= 12) {
        FILE_NOTIFY_INFORMATION *f = (FILE_NOTIFY_INFORMATION *)buf;
        EXPECT_EQ(f->Action, FILE_ACTION_ADDED);
        CHECK(f->FileNameLength >= 10 && !wcsncmp(f->FileName, L"sync", 4));
    }
    WaitForSingleObject(th, 15000); CloseHandle(th);
    CloseHandle(h);
}
static volatile LONG g_cr_calls; static volatile DWORD g_cr_err, g_cr_bytes; static volatile LPOVERLAPPED g_cr_ov;
static VOID WINAPI cr_routine(DWORD err, DWORD bytes, LPOVERLAPPED ov) { g_cr_calls++; g_cr_err = err; g_cr_bytes = bytes; g_cr_ov = ov; }
TEST(notify_rdc_completion_routine) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_cr");
    watch_t *w = watch_new(d);
    g_cr_calls = 0; g_cr_ov = NULL;
    SET_ERR(0);
    BOOL ok = U(ReadDirectoryChangesW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, cr_routine);
    CHECK(ok);
    if (!ok) { watch_free(w); return; }
    dh_mkfile(d, "crtrig", 0);
    DWORD r = SleepEx(5000, TRUE);
    EXPECT_EQ(r, WAIT_IO_COMPLETION);
    EXPECT_EQ(g_cr_calls, 1);
    EXPECT_EQ(g_cr_err, 0);
    CHECK(g_cr_bytes >= 12);
    CHECK(g_cr_ov == &w->ov);
    watch_free(w);
}
TEST(notify_rdc_completion_routine_not_run_without_alertable_wait) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_cr2");
    watch_t *w = watch_new(d);
    g_cr_calls = 0;
    BOOL ok = U(ReadDirectoryChangesW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, cr_routine);
    CHECK(ok);
    dh_mkfile(d, "crtrig", 0);
    Sleep(500);                              /* non-alertable */
    EXPECT_EQ(g_cr_calls, 0);
    DWORD sr = SleepEx(2000, TRUE);
    EXPECT_EQ(sr, WAIT_IO_COMPLETION);
    EXPECT_EQ(g_cr_calls, 1);
    watch_free(w);
}
TEST(notify_rdc_completion_routine_cancel) {
    REQUIRE(U(ReadDirectoryChangesW)); REQUIRE(U(CancelIoEx));
    char *d = dh_dir("rd_cr3");
    watch_t *w = watch_new(d);
    g_cr_calls = 0; g_cr_err = 0xffff;
    BOOL ok = U(ReadDirectoryChangesW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, cr_routine);
    CHECK(ok);
    CHECK(U(CancelIoEx)(w->h, NULL));
    SleepEx(2000, TRUE);
    EXPECT_EQ(g_cr_calls, 1);
    EXPECT_EQ(g_cr_err, ERROR_OPERATION_ABORTED);
    watch_free(w);
}
TEST(notify_rdc_iocp) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_iocp");
    watch_t *w = watch_new(d);
    HANDLE port = CreateIoCompletionPort(w->h, NULL, 0x1234, 0);
    CHECK(port != NULL);
    if (!port) { watch_free(w); return; }
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    dh_mkfile(d, "iocp_file", 0);
    DWORD b = 0; ULONG_PTR key = 0; LPOVERLAPPED ov = NULL;
    BOOL ok = GetQueuedCompletionStatus(port, &b, &key, &ov, 5000);
    CHECK(ok);
    EXPECT_EQ(key, 0x1234); CHECK(ov == &w->ov); CHECK(b >= 12);
    CloseHandle(port);
    watch_free(w);
}
TEST(notify_rdc_two_watchers_same_dir) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_two");
    watch_t *a = watch_new(d), *b = watch_new(d);
    CHECK(watch_start(a, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof a->buf));
    CHECK(watch_start(b, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof b->buf));
    dh_mkfile(d, "both", 0);
    ev_t e[4]; DWORD n;
    int na = watch_get(a, 5000, e, 4, &n); CHECK(na >= 1);
    int nb = watch_get(b, 5000, e, 4, &n); CHECK(nb >= 1);
    CancelIoEx(a->h, NULL); CancelIoEx(b->h, NULL);
    watch_free(a); watch_free(b);
}
TEST(notify_rdc_two_requests_same_handle) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_tworeq");
    watch_t *w = watch_new(d);
    OVERLAPPED ov2; memset(&ov2, 0, sizeof ov2); ov2.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    static DWORD buf2[4096];
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    BOOL ok = U(ReadDirectoryChangesW)(w->h, buf2, sizeof buf2, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov2, NULL);
    CHECK(ok);
    dh_mkfile(d, "q", 0);
    ev_t e[4]; DWORD n;
    int a = watch_get(w, 5000, e, 4, &n);
    CHECK(a >= 1);
    DWORD r = WaitForSingleObject(ov2.hEvent, 1000);
    t_note("second queued request completed too: %d (first gets the event)", r == WAIT_OBJECT_0);
    CancelIoEx(w->h, NULL); WaitForSingleObject(ov2.hEvent, 2000);
    CloseHandle(ov2.hEvent);
    watch_free(w);
}
TEST(notify_rdc_unicode_names) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_uni"); WCHAR dw[MAX_PATH];
    dh_w(dw, MAX_PATH, d);
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    dh_mkfileW(dw, L"café-日本-\U0001F600", 0);
    CHECK(WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0);
    DWORD b = 0; GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    FILE_NOTIFY_INFORMATION *f = (FILE_NOTIFY_INFORMATION *)w->buf;
    EXPECT_EQ(f->Action, FILE_ACTION_ADDED);
    CHECK(f->FileNameLength == wcslen(L"café-日本-\U0001F600") * 2 && !wcsncmp(f->FileName, L"café-日本-\U0001F600", f->FileNameLength / 2));
    CancelIoEx(w->h, NULL);
    watch_free(w);
    dh_rmtreeW(dw);
}
TEST(notify_rdc_long_name_255) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_long"); WCHAR dw[MAX_PATH], nm[256];
    dh_w(dw, MAX_PATH, d);
    for (int i = 0; i < 255; i++) nm[i] = 'a' + (i % 26);
    nm[255] = 0;
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    WCHAR full[1024]; snwprintf(full, 1024, L"\\\\?\\%ls\\%ls", dw, nm);
    HANDLE f = CreateFileW(full, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    CHECK(WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0);
    DWORD b = 0; GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION *)w->buf;
    EXPECT_EQ(fni->FileNameLength, 510);
    CancelIoEx(w->h, NULL);
    watch_free(w);
    DeleteFileW(full);
}
TEST(notify_rdc_bytes_returned_for_overlapped_null) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_bytesnull");
    watch_t *w = watch_new(d);
    BOOL ok = U(ReadDirectoryChangesW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL);
    CHECK(ok);        /* lpBytesReturned may be NULL in the asynchronous form */
    CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000);
    watch_free(w);
}
TEST(notify_rdc_all_filter_bits_accepted) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_filterbits");
    DWORD fl[] = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x17F, 0x1FF };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        watch_t *w = watch_new(d);
        SET_ERR(0);
        BOOL ok = watch_start(w, i & 1, fl[i], sizeof w->buf);
        if (fl[i] == 0x1FF || fl[i] == 0x100) { if (!ok) t_note("filter 0x%lx rejected err %lu", fl[i], GetLastError()); }
        else CHECKF(ok, "filter 0x%lx err %lu", fl[i], GetLastError());
        if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
        watch_free(w);
    }
}
TEST(notify_rdc_last_access_filter) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_access");
    watch_t *w = watch_new(d);
    BOOL ok = watch_start(w, FALSE, FILE_NOTIFY_CHANGE_LAST_ACCESS, sizeof w->buf);
    CHECK(ok);
    if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    watch_free(w);
}
TEST(notify_rdc_security_change_event) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_security"), p[MAX_PATH];
    dh_mkfile(d, "s.txt", 1); sub(p, sizeof p, d, "s.txt");
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_SECURITY, sizeof w->buf));
    PSECURITY_DESCRIPTOR sd = NULL; PACL dacl = NULL;
    DWORD r = GetNamedSecurityInfoA(p, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd);
    if (r == ERROR_SUCCESS) {
        /* re-apply the same DACL: a security descriptor write that changes nothing materially */
        r = SetNamedSecurityInfoA(p, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, dacl, NULL);
        t_note("SetNamedSecurityInfo(same DACL) = %lu", r);
        ev_t e[4]; DWORD b;
        int n = watch_get(w, 3000, e, 4, &b);
        t_note("security notification events: %d", n);
        LocalFree(sd);
    }
    CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000);
    watch_free(w);
}
TEST(notify_rdc_collect_burst) {
    REQUIRE(U(ReadDirectoryChangesW));
    char *d = dh_dir("rd_burst"), p[MAX_PATH];
    watch_t *w = watch_new(d);
    CHECK(watch_start(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, sizeof w->buf));
    for (int i = 0; i < 100; i++) { char n[16]; snprintf(n, sizeof n, "b%03d", i); dh_mkfile(d, n, 0); }
    for (int i = 0; i < 100; i++) { char n[16]; snprintf(n, sizeof n, "b%03d", i); DeleteFileA(sub(p, sizeof p, d, n)); }
    ev_t *e = malloc(sizeof(ev_t) * 400);
    int n = collect(w, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, 600, e, 400);
    CHECKF(n >= 0, "collect failed %d", n);
    int added = 0, removed = 0;
    for (int i = 0; i < n; i++) { if (e[i].action == FILE_ACTION_ADDED) added++; if (e[i].action == FILE_ACTION_REMOVED) removed++; }
    t_note("burst: added=%d removed=%d", added, removed);
    CHECKF(added >= 1 && removed >= 1 && added <= 100 && removed <= 100, "added %d removed %d", added, removed); /* events between re-arms may be lost: exact count not guaranteed */
    free(e);
    CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000);
    watch_free(w);
}

/* ---- ReadDirectoryChangesExW ---- */
typedef struct {
    DWORD NextEntryOffset, Action;
    LARGE_INTEGER CreationTime, LastModificationTime, LastChangeTime, LastAccessTime, AllocatedLength, FileSize;
    DWORD FileAttributes, ReparsePointTag;
    LARGE_INTEGER FileId, ParentFileId;
    DWORD FileNameLength; WCHAR FileName[1];
} fni_ext;

TEST(notify_rdcex_invalid_info_class) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    char *d = dh_dir("rx_badclass");
    watch_t *w = watch_new(d);
    int bad[] = { 0, -1, 99 };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        BOOL ok = UN(ReadDirectoryChangesExW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL, bad[i]);
        CHECKF(!ok, "class %d", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    watch_free(w);
}
TEST(notify_rdcex_class_1_matches_w) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    char *d = dh_dir("rx_class1");
    watch_t *w = watch_new(d);
    BOOL ok = UN(ReadDirectoryChangesExW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL, 1);
    CHECK(ok);
    dh_mkfile(d, "c1", 0);
    CHECK(WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0);
    DWORD b = 0; GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    FILE_NOTIFY_INFORMATION *f = (FILE_NOTIFY_INFORMATION *)w->buf;
    EXPECT_EQ(f->Action, FILE_ACTION_ADDED); EXPECT_EQ(f->FileNameLength, 4);
    EXPECT_EQ(b, 12 + 4);
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdcex_extended_information) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    char *d = dh_dir("rx_ext");
    watch_t *w = watch_new(d);
    SET_ERR(0);
    BOOL ok = UN(ReadDirectoryChangesExW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL, 2);
    CHECKF(ok, "err %lu", GetLastError());
    if (!ok) { watch_free(w); return; }
    dh_mkfile(d, "ext.txt", 100);
    CHECK(WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0);
    DWORD b = 0; GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    fni_ext *f = (fni_ext *)w->buf;
    CHECK(b >= 84 + 14);
    EXPECT_EQ(f->Action, FILE_ACTION_ADDED);
    EXPECT_EQ(f->FileNameLength, 14);
    CHECK(f->FileNameLength == 14 && !wcsncmp(f->FileName, L"ext.txt", 7));
    CHECK(f->FileAttributes & FILE_ATTRIBUTE_ARCHIVE);
    CHECK(!(f->FileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    CHECK(f->CreationTime.QuadPart != 0 && f->LastModificationTime.QuadPart != 0);
    CHECK(f->FileId.QuadPart != 0);
    t_note("extended info ParentFileId=0x%llx", (unsigned long long)f->ParentFileId.QuadPart);
    t_note("extended info: FileSize=%lld AllocatedLength=%lld (size at creation time; unspecified)", f->FileSize.QuadPart, f->AllocatedLength.QuadPart);
    CHECK(((ULONG_PTR)f & 7) == 0);
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdcex_extended_directory_attribute) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    char *d = dh_dir("rx_extdir");
    watch_t *w = watch_new(d);
    BOOL ok = UN(ReadDirectoryChangesExW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_DIR_NAME, NULL, &w->ov, NULL, 2);
    CHECK(ok);
    if (!ok) { watch_free(w); return; }
    dh_mkdir(d, "extsub");
    CHECK(WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0);
    DWORD b = 0; GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    fni_ext *f = (fni_ext *)w->buf;
    CHECK(f->FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdcex_extended_rename_pair) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    char *d = dh_dir("rx_extren"), p[MAX_PATH], q[MAX_PATH];
    dh_mkfile(d, "r_old", 5);
    watch_t *w = watch_new(d);
    BOOL ok = UN(ReadDirectoryChangesExW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL, 2);
    CHECK(ok);
    if (!ok) { watch_free(w); return; }
    MoveFileA(sub(p, sizeof p, d, "r_old"), sub(q, sizeof q, d, "r_new"));
    CHECK(WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0);
    DWORD b = 0; GetOverlappedResult(w->h, &w->ov, &b, FALSE);
    fni_ext *f = (fni_ext *)w->buf;
    EXPECT_EQ(f->Action, FILE_ACTION_RENAMED_OLD_NAME);
    CHECK(f->NextEntryOffset != 0);
    if (f->NextEntryOffset) {
        fni_ext *g = (fni_ext *)((BYTE *)f + f->NextEntryOffset);
        EXPECT_EQ(g->Action, FILE_ACTION_RENAMED_NEW_NAME);
        t_note("rename pair FileIds equal: %d", f->FileId.QuadPart == g->FileId.QuadPart);
        CHECK((f->NextEntryOffset & 7) == 0);
    }
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdcex_extended_file_size_on_modify) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    char *d = dh_dir("rx_extsize"), p[MAX_PATH];
    dh_mkfile(d, "g.bin", 1); sub(p, sizeof p, d, "g.bin");
    watch_t *w = watch_new(d);
    BOOL ok = UN(ReadDirectoryChangesExW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, NULL, &w->ov, NULL, 2);
    CHECK(ok);
    if (!ok) { watch_free(w); return; }
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); DWORD wr; char buf[3000] = {0};
    WriteFile(f, buf, sizeof buf, &wr, NULL); FlushFileBuffers(f); CloseHandle(f);
    if (WaitForSingleObject(w->ov.hEvent, 5000) == WAIT_OBJECT_0) {
        DWORD b = 0; GetOverlappedResult(w->h, &w->ov, &b, FALSE);
        fni_ext *e = (fni_ext *)w->buf;
        EXPECT_EQ(e->Action, FILE_ACTION_MODIFIED);
        CHECK(e->FileSize.QuadPart == 3000 || e->FileSize.QuadPart == 1);
        t_note("modify notification FileSize=%lld", e->FileSize.QuadPart);
    } else CHECK(0);
    CancelIoEx(w->h, NULL);
    watch_free(w);
}
TEST(notify_rdcex_invalid_handle) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    static DWORD buf[256]; OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    SET_ERR(0);
    CHECK(!UN(ReadDirectoryChangesExW)(INVALID_HANDLE_VALUE, buf, sizeof buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL, 1));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(notify_rdcex_class_3_support) {
    REQUIRE(UN(ReadDirectoryChangesExW));
    char *d = dh_dir("rx_class3");
    watch_t *w = watch_new(d);
    SET_ERR(0);
    BOOL ok = UN(ReadDirectoryChangesExW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &w->ov, NULL, 3);
    t_note("ReadDirectoryNotifyFullInformation (class 3): ok=%d err=%lu", ok, GetLastError());
    if (ok) { CancelIoEx(w->h, NULL); WaitForSingleObject(w->ov.hEvent, 2000); }
    watch_free(w);
}
