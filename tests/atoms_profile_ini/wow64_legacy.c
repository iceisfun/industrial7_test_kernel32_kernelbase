/* Legacy / WoW64 oriented entry points: SetHandleCount, Wow64 FS redirection, guest machine queries. */
#include "ap.h"

UDECL(HRESULT, GetMachineTypeAttributes, (USHORT Machine, DWORD *Attributes));

TEST(wowleg_sethandlecount_returns_argument) {
    REQUIRE(U(SetHandleCount));
    UINT vals[] = { 0, 1, 20, 255, 256, 1000, 0x10000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        SET_ERR(0xdeadbeef);
        UINT r = U(SetHandleCount)(vals[i]);
        CHECKF(r == vals[i], "SetHandleCount(%u) -> %u", vals[i], r);
        EXPECT_ERR(0xdeadbeef);
    }
}

TEST(legacy_sethandlecount_does_not_break_handles) {
    REQUIRE(U(SetHandleCount));
    U(SetHandleCount)(5);
    HANDLE e[40]; int ok = 1;
    for (int i = 0; i < 40; i++) { e[i] = CreateEventA(NULL, TRUE, FALSE, NULL); if (!e[i]) ok = 0; }
    CHECK(ok);
    for (int i = 0; i < 40; i++) if (e[i]) CloseHandle(e[i]);
}

TEST(legacy_sethandlecount_kernelbase_and_kernel32_agree) {
    REQUIRE(UK(SetHandleCount)); REQUIRE(UB(SetHandleCount));
    EXPECT_EQ(UK(SetHandleCount)(99), 99);
    EXPECT_EQ(UB(SetHandleCount)(98), 98);
}

CHILD(wow_redirection) {
    /* encode the observed results in the exit code */
    PVOID old = (PVOID)(ULONG_PTR)0x1234;
    int code = 0;
    SetLastError(0);
    BOOL d = U(Wow64DisableWow64FsRedirection)(&old);
    DWORD ed = GetLastError();
    if (d) code |= 1;
    SetLastError(0);
    BOOL r = d ? U(Wow64RevertWow64FsRedirection)(old) : U(Wow64RevertWow64FsRedirection)((PVOID)(ULONG_PTR)0x1234);
    DWORD er = GetLastError();
    if (r) code |= 2;
    SetLastError(0);
    BOOLEAN en = U(Wow64EnableWow64FsRedirection)(TRUE);
    DWORD ee = GetLastError();
    if (en) code |= 4;
    SetLastError(0);
    BOOLEAN dis = U(Wow64EnableWow64FsRedirection)(FALSE);
    if (dis) code |= 8;
    U(Wow64EnableWow64FsRedirection)(TRUE);
    /* revert-to-default: the invariant is that Disable/Revert pairs are consistent */
    if (d && !r) return 100;
    (void)ed; (void)er; (void)ee;
    return code;
}
TEST(wow64_fs_redirection_native_process_results_note) {
    REQUIRE(U(Wow64DisableWow64FsRedirection)); REQUIRE(U(Wow64RevertWow64FsRedirection)); REQUIRE(U(Wow64EnableWow64FsRedirection));
    DWORD r = t_run_child("wow_redirection", "", 20000);
    CHECK(r < 100);
    t_note("native x64 process: Disable=%lu Revert=%lu Enable(TRUE)=%lu Enable(FALSE)=%lu", r & 1, (r >> 1) & 1, (r >> 2) & 1, (r >> 3) & 1);
}

CHILD(wow_redirection_system32) {
    /* a file visible in System32 stays visible whatever the redirection state is for a native process */
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\kernel32.dll");
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) return 1;
    PVOID old = NULL;
    BOOL d = U(Wow64DisableWow64FsRedirection)(&old);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) return 2;
    if (d) U(Wow64RevertWow64FsRedirection)(old);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) return 3;
    return 0;
}
TEST(wow64_fs_redirection_does_not_hide_system32_for_native_process) {
    REQUIRE(U(Wow64DisableWow64FsRedirection));
    EXPECT_EQ(t_run_child("wow_redirection_system32", "", 20000), 0);
}

CHILD(wow_redirection_nested) {
    PVOID a = NULL, b = NULL;
    BOOL d1 = U(Wow64DisableWow64FsRedirection)(&a);
    BOOL d2 = U(Wow64DisableWow64FsRedirection)(&b);
    if (d1 != d2) return 1;
    if (d2) { if (!U(Wow64RevertWow64FsRedirection)(b)) return 2; }
    if (d1) { if (!U(Wow64RevertWow64FsRedirection)(a)) return 3; }
    return 0;
}
TEST(wow64_fs_redirection_nested_disable_revert) {
    REQUIRE(U(Wow64DisableWow64FsRedirection));
    EXPECT_EQ(t_run_child("wow_redirection_nested", "", 20000), 0);
}

CHILD(wow_redirection_null) {
    /* NULL OldValue pointer: validated by the callee? observe only, in a child */
    SetLastError(0);
    BOOL r = U(Wow64RevertWow64FsRedirection)(NULL);
    return r ? 1 : 0;
}
TEST(wow64_revert_null_old_value_note) {
    REQUIRE(U(Wow64RevertWow64FsRedirection));
    DWORD r = t_run_child("wow_redirection_null", "", 20000);
    t_note("Wow64RevertWow64FsRedirection(NULL) child result: %lu (1 = TRUE)", r);
    CHECK(r <= 1);
}

TEST(wowleg_guest_machine_supported_i386) {
    REQUIRE(UB(IsWow64GuestMachineSupported));
    BOOL sup = 0xCC;
    HRESULT hr = UB(IsWow64GuestMachineSupported)(IMAGE_FILE_MACHINE_I386, &sup);
    EXPECT_EQ(hr, S_OK);
    CHECK(sup == TRUE || sup == FALSE);
    char wow[MAX_PATH]; UINT n = U(GetSystemWow64DirectoryA) ? U(GetSystemWow64DirectoryA)(wow, sizeof wow) : 0;
    t_note("i386 guest supported=%d; SysWOW64 dir present=%d", sup, n != 0);
    if (n) EXPECT_EQ(sup, TRUE);
}

TEST(wow64_guest_machine_supported_table) {
    REQUIRE(UB(IsWow64GuestMachineSupported));
    struct { USHORT m; const char *n; } t[] = {
        { IMAGE_FILE_MACHINE_I386, "i386" }, { 0x8664, "amd64" }, { 0x1c4, "armnt" }, { 0xaa64, "arm64" },
        { 0x200, "ia64" }, { 0, "unknown(0)" }, { 0x1234, "bogus" }, { 0xFFFF, "ffff" }, { 0x1c0, "arm" }, { 0x01c2, "thumb" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BOOL sup = 0xCC;
        HRESULT hr = UB(IsWow64GuestMachineSupported)(t[i].m, &sup);
        t_note("guest %-10s hr=0x%08lx supported=%d", t[i].n, hr, sup);
        if (hr == S_OK) CHECKF(sup == TRUE || sup == FALSE, "%s: out value not BOOL (%d)", t[i].n, sup);
        else CHECKF(FAILED(hr), "%s: odd hr", t[i].n);
    }
}

TEST(wow64_guest_machine_supported_failure_leaves_output) {
    REQUIRE(UB(IsWow64GuestMachineSupported));
    BOOL sup = 0x5A5A;
    HRESULT hr = UB(IsWow64GuestMachineSupported)(0x1234, &sup);
    if (FAILED(hr)) EXPECT_EQ(sup, 0x5A5A);       /* output only written on success */
    else t_note("bogus machine yields S_OK, supported=%d", sup);
}

TEST(wow64_guest_machine_supported_k32_forwards_to_kernelbase) {
    REQUIRE(UK(IsWow64GuestMachineSupported)); REQUIRE(UB(IsWow64GuestMachineSupported));
    BOOL a = 0xCC, b = 0xCC;
    HRESULT h1 = UK(IsWow64GuestMachineSupported)(IMAGE_FILE_MACHINE_I386, &a);
    HRESULT h2 = UB(IsWow64GuestMachineSupported)(IMAGE_FILE_MACHINE_I386, &b);
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(a, b);
    t_note("kernel32 export address %s kernelbase export", (void *)UK(IsWow64GuestMachineSupported) == (void *)UB(IsWow64GuestMachineSupported) ? "equals" : "differs from");
}

TEST(wow64_machine_type_attributes_when_present) {
    GetMachineTypeAttributes_pfn f = UN(GetMachineTypeAttributes);
    if (!f) { t_skip("GetMachineTypeAttributes not exported on this build"); return; }
    DWORD a = 0xCCCCCCCC;
    HRESULT hr = f(0x8664, &a);
    EXPECT_EQ(hr, S_OK);
    CHECKF((a & 3) == 3, "AMD64 attributes 0x%lx", a);      /* UserEnabled | KernelEnabled */
    EXPECT_EQ(a & ~7u, 0);
    a = 0xCC;
    hr = f(IMAGE_FILE_MACHINE_I386, &a);
    t_note("I386 attributes: hr=0x%lx a=0x%lx", hr, a);
    a = 0xCC;
    hr = f(0x1234, &a);
    t_note("bogus machine: hr=0x%lx a=0x%lx", hr, a);
}

CHILD(wow_default_guest) {
    UDECL(USHORT, Wow64SetThreadDefaultGuestMachine, (USHORT));
    Wow64SetThreadDefaultGuestMachine_pfn f = UN(Wow64SetThreadDefaultGuestMachine);
    if (!f) return 1;
    USHORT a = f(IMAGE_FILE_MACHINE_I386);
    USHORT b = f(0);
    USHORT c = f(0x8664);
    /* in a native process the call only reports the machine type and must be stable */
    if (a != b || b != c) return 2;
    return a == 0x8664 ? 0 : 3;
}
TEST(wow64_set_thread_default_guest_machine_native) {
    REQUIRE(UB(Wow64SetThreadDefaultGuestMachine));
    DWORD r = t_run_child("wow_default_guest", "", 20000);
    t_note("Wow64SetThreadDefaultGuestMachine child result %lu (0 = stable and AMD64)", r);
    CHECK(r == 0 || r == 3);
}

TEST(wow64_selector_entry_invalid_handle) {
    REQUIRE(U(Wow64GetThreadSelectorEntry));
    WOW64_LDT_ENTRY e; memset(&e, 0x55, sizeof e);
    SET_ERR(0);
    /* selector 0x23 (index 0x20) is synthesised by ntdll without touching the handle and succeeds even for NULL;
       only the TEB selector (0x53, index 0x50) queries the thread, so it is the one that validates the handle */
    EXPECT_EQ(U(Wow64GetThreadSelectorEntry)(NULL, 0x53, &e), FALSE);
    CHECK(GetLastError() != 0);
    t_note("Wow64GetThreadSelectorEntry(NULL) err=%lu", GetLastError());
    SET_ERR(0);
    EXPECT_EQ(U(Wow64GetThreadSelectorEntry)(INVALID_HANDLE_VALUE, 0x53, &e), FALSE);
    t_note("Wow64GetThreadSelectorEntry(-1) err=%lu", GetLastError());
}

TEST(wow64_selector_entry_current_thread_native_note) {
    REQUIRE(U(Wow64GetThreadSelectorEntry));
    WOW64_LDT_ENTRY e; memset(&e, 0, sizeof e);
    SET_ERR(0);
    BOOL r = U(Wow64GetThreadSelectorEntry)(GetCurrentThread(), 0x23, &e);
    t_note("selector 0x23 on native thread -> %d err=%lu", r, GetLastError());
}

TEST(wow64_thread_context_invalid_handles) {
    REQUIRE(U(Wow64GetThreadContext)); REQUIRE(U(Wow64SetThreadContext)); REQUIRE(U(Wow64SuspendThread));
    WOW64_CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = WOW64_CONTEXT_CONTROL;
    SET_ERR(0);
    EXPECT_EQ(U(Wow64GetThreadContext)(NULL, &c), FALSE);
    t_note("Wow64GetThreadContext(NULL) err=%lu", GetLastError());
    SET_ERR(0);
    EXPECT_EQ(U(Wow64SetThreadContext)(NULL, &c), FALSE);
    t_note("Wow64SetThreadContext(NULL) err=%lu", GetLastError());
    SET_ERR(0);
    EXPECT_EQ(U(Wow64SuspendThread)(NULL), (DWORD)-1);
    t_note("Wow64SuspendThread(NULL) err=%lu", GetLastError());
    SET_ERR(0);
    EXPECT_EQ(U(Wow64SuspendThread)(INVALID_HANDLE_VALUE), (DWORD)-1);
}

TEST(wow64_thread_context_on_native_thread_of_own_child_note) {
    REQUIRE(U(Wow64GetThreadContext)); REQUIRE(U(Wow64SuspendThread));
    /* a thread created by this test, suspended at creation; never touches foreign threads */
    HANDLE t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ExitThread, NULL, CREATE_SUSPENDED, NULL);
    CHECK(t != NULL);
    if (!t) return;
    WOW64_CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = WOW64_CONTEXT_CONTROL;
    SET_ERR(0);
    BOOL r = U(Wow64GetThreadContext)(t, &c);
    t_note("Wow64GetThreadContext on native thread -> %d err=%lu", r, GetLastError());
    SET_ERR(0);
    DWORD s = U(Wow64SuspendThread)(t);
    t_note("Wow64SuspendThread on native thread -> %lu err=%lu", s, GetLastError());
    ResumeThread(t);
    if (s != (DWORD)-1) ResumeThread(t);
    WaitForSingleObject(t, 5000);
    CloseHandle(t);
}

TEST(wow64_system_directory_queries_consistent) {
    REQUIRE(U(GetSystemWow64DirectoryA)); REQUIRE(U(GetSystemWow64DirectoryW));
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    SET_ERR(0);
    UINT ra = U(GetSystemWow64DirectoryA)(a, sizeof a), rw = U(GetSystemWow64DirectoryW)(w, MAX_PATH);
    t_note("GetSystemWow64Directory A=%u W=%u err=%lu", ra, rw, GetLastError());
    EXPECT_EQ(ra, rw);
    if (ra) {
        EXPECT_EQ(ra, strlen(a));
        CHECK(GetFileAttributesA(a) != INVALID_FILE_ATTRIBUTES);
        UINT need = U(GetSystemWow64DirectoryA)(NULL, 0);
        EXPECT_EQ(need, ra + 1);
        char small[4]; EXPECT_EQ(U(GetSystemWow64DirectoryA)(small, 4), ra + 1);
    }
}
