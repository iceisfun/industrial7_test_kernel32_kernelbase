#include "rk.h"

#ifndef REG_NOTIFY_THREAD_AGNOSTIC
#define REG_NOTIFY_THREAD_AGNOSTIC 0x10000000
#endif
#define NOTE_ALL (REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_ATTRIBUTES | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_SECURITY)

static HKEY open_notify(HKEY parent) { HKEY k = NULL; RegOpenKeyExW(parent, NULL, 0, KEY_NOTIFY | KEY_READ, &k); return k; }
static int signaled(HANDLE ev, DWORD ms) { return WaitForSingleObject(ev, ms) == WAIT_OBJECT_0; }

TEST(regkb_notify_filter_values) {
    EQ(REG_NOTIFY_CHANGE_NAME, 1); EQ(REG_NOTIFY_CHANGE_ATTRIBUTES, 2);
    EQ(REG_NOTIFY_CHANGE_LAST_SET, 4); EQ(REG_NOTIFY_CHANGE_SECURITY, 8);
    EQ(REG_NOTIFY_THREAD_AGNOSTIC, 0x10000000);
}
TEST(regkb_notify_async_value_change_signals_event) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h); REQUIRE(w);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    CHECK(!signaled(ev, 100));
    sc_dw(h, L"trigger", 1);
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_async_subkey_create_signals_name_filter) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_NAME, ev, TRUE), 0);
    sc_subc(h, L"newsub");
    CHECK(signaled(ev, 5000));
    ResetEvent(ev);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_NAME, ev, TRUE), 0);
    RegDeleteKeyW(h, L"newsub");
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_filter_excludes_other_changes) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_NAME, ev, TRUE), 0);
    sc_dw(h, L"value_only", 1);                 /* value changes are not name changes */
    CHECK(!signaled(ev, 300));
    sc_subc(h, L"now_a_name");
    CHECK(signaled(ev, 5000));
    ResetEvent(ev);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    sc_subc(h, L"another_name");                /* subkey creation is not a value change */
    CHECK(!signaled(ev, 300));
    sc_dw(h, L"value_two", 2);
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_value_delete_signals_last_set) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"going", 1); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    RegDeleteValueW(h, L"going");
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_one_shot_semantics) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    sc_dw(h, L"a", 1); CHECK(signaled(ev, 5000));
    ResetEvent(ev);
    sc_dw(h, L"b", 2);                          /* not re-armed: no further notification */
    CHECK(!signaled(ev, 300));
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    sc_dw(h, L"c", 3); CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_watch_subtree) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY deep = sc_sub(h, L"a\\b"); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    sc_dw(deep, L"x", 1);
    CHECK(!signaled(ev, 300));                  /* not watching the subtree */
    EQ(U(RegNotifyChangeKeyValue)(w, TRUE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    sc_dw(deep, L"y", 2);
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(deep); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_security_change) {
    REQUIRE(U(RegNotifyChangeKeyValue)); REQUIRE(U(RegSetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_SECURITY, ev, TRUE), 0);
    BYTE sd[2048]; DWORD cb = sizeof sd;
    EQ(RegGetKeySecurity(h, DACL_SECURITY_INFORMATION, sd, &cb), 0);
    EQ(U(RegSetKeySecurity)(h, DACL_SECURITY_INFORMATION, sd), 0);
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_all_filters_combined) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, TRUE, NOTE_ALL, ev, TRUE), 0);
    sc_subc(h, L"anything");
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
/* kernelbase RegNotifyChangeKeyValue: fAsynchronous && !hEvent -> 0x57 */
TEST(regkb_notify_async_requires_event) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, NULL, TRUE), ERROR_INVALID_PARAMETER);
    RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_requires_notify_access) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY q; EQ(RegOpenKeyExW(h, NULL, 0, KEY_QUERY_VALUE, &q), 0);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(q, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), ERROR_ACCESS_DENIED);
    CloseHandle(ev); RegCloseKey(q); RegCloseKey(h);
}
TEST(regkb_notify_invalid_handles) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    LSTATUS s = U(RegNotifyChangeKeyValue)(NULL, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE);
    CHECK(s != 0); t_note("notify on NULL key = %ld", (long)s);
    EQ(U(RegNotifyChangeKeyValue)(HKEY_PERFORMANCE_DATA, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), ERROR_INVALID_HANDLE);
    HKEY r = FRESH(); REQUIRE(r); HKEY k = sc_sub(r, L"x"); RegCloseKey(k);
    EQ(U(RegNotifyChangeKeyValue)(k, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), ERROR_INVALID_HANDLE);
    CloseHandle(ev); RegCloseKey(r);
}
TEST(regkb_notify_zero_filter_observed) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    LSTATUS s = U(RegNotifyChangeKeyValue)(w, FALSE, 0, ev, TRUE);
    t_note("filter 0 = %ld", (long)s);
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_closing_watched_handle_signals_event_observed) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    RegCloseKey(w);
    t_note("event signaled after closing the watched handle: %d", signaled(ev, 1000));
    CloseHandle(ev); RegCloseKey(h);
}
TEST(regkb_notify_deleting_watched_key_observed) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY k = sc_sub(h, L"victim"); HKEY w = open_notify(k);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(w, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    RegCloseKey(k);
    EQ(RegDeleteKeyW(h, L"victim"), 0);
    t_note("watcher signaled when its key was deleted: %d", signaled(ev, 2000));
    CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}

struct sync_arg { HKEY key; LSTATUS res; HANDLE started; DWORD flags; BOOL async; HANDLE ev; };
static DWORD WINAPI sync_notify_thread(LPVOID p) {
    struct sync_arg *a = p;
    SetEvent(a->started);
    a->res = U(RegNotifyChangeKeyValue)(a->key, FALSE, a->flags, a->ev, a->async);
    return 0;
}
TEST(regkb_notify_synchronous_blocks_until_change) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    struct sync_arg a = { w, 99, CreateEventW(NULL, TRUE, FALSE, NULL), REG_NOTIFY_CHANGE_LAST_SET, FALSE, NULL };
    HANDLE th = CreateThread(NULL, 0, sync_notify_thread, &a, 0, NULL); REQUIRE(th);
    WaitForSingleObject(a.started, 5000);
    CHECK(WaitForSingleObject(th, 400) == WAIT_TIMEOUT);   /* still blocked */
    sc_dw(h, L"wake", 1);
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    EQ(a.res, 0);
    CloseHandle(th); CloseHandle(a.started); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_thread_agnostic_survives_thread_exit) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    struct sync_arg a = { w, 99, CreateEventW(NULL, TRUE, FALSE, NULL), REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC, TRUE, ev };
    HANDLE th = CreateThread(NULL, 0, sync_notify_thread, &a, 0, NULL); REQUIRE(th);
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    EQ(a.res, 0);
    CHECK(!signaled(ev, 400));                 /* registering thread is gone, event must not have fired */
    sc_dw(h, L"later", 1);
    CHECK(signaled(ev, 5000));
    CloseHandle(th); CloseHandle(a.started); CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_without_agnostic_thread_exit_observed) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY w = open_notify(h);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    struct sync_arg a = { w, 99, CreateEventW(NULL, TRUE, FALSE, NULL), REG_NOTIFY_CHANGE_LAST_SET, TRUE, ev };
    HANDLE th = CreateThread(NULL, 0, sync_notify_thread, &a, 0, NULL); REQUIRE(th);
    WaitForSingleObject(th, 5000);
    EQ(a.res, 0);
    t_note("event signaled when the registering thread exits (no THREAD_AGNOSTIC): %d", signaled(ev, 500));
    CloseHandle(th); CloseHandle(a.started); CloseHandle(ev); RegCloseKey(w); RegCloseKey(h);
}
TEST(regkb_notify_two_keys_one_event) {
    REQUIRE(U(RegNotifyChangeKeyValue));
    HKEY h = FRESH(); REQUIRE(h); HKEY a = sc_sub(h, L"a"), b = sc_sub(h, L"b");
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    EQ(U(RegNotifyChangeKeyValue)(a, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    EQ(U(RegNotifyChangeKeyValue)(b, FALSE, REG_NOTIFY_CHANGE_LAST_SET, ev, TRUE), 0);
    sc_dw(b, L"x", 1);
    CHECK(signaled(ev, 5000));
    CloseHandle(ev); RegCloseKey(a); RegCloseKey(b); RegCloseKey(h);
}
