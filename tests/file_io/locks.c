#include "fio.h"

#define SHARE_ALL (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
static HANDLE open_rw(const char *p, DWORD extra) { return cfa(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, OPEN_EXISTING, extra); }
static BOOL lockex(HANDLE h, DWORD fl, DWORD64 off, DWORD64 len) {
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Offset = (DWORD)off; ov.OffsetHigh = (DWORD)(off >> 32);
    return U(LockFileEx)(h, fl, 0, (DWORD)len, (DWORD)(len >> 32), &ov);
}
static BOOL unlockex(HANDLE h, DWORD64 off, DWORD64 len) {
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Offset = (DWORD)off; ov.OffsetHigh = (DWORD)(off >> 32);
    return U(UnlockFileEx)(h, 0, (DWORD)len, (DWORD)(len >> 32), &ov);
}
#define LF(h, off, len) U(LockFile)(h, (DWORD)(off), (DWORD)((DWORD64)(off) >> 32), (DWORD)(len), (DWORD)((DWORD64)(len) >> 32))
#define UF(h, off, len) U(UnlockFile)(h, (DWORD)(off), (DWORD)((DWORD64)(off) >> 32), (DWORD)(len), (DWORD)((DWORD64)(len) >> 32))
#define FI (LOCKFILE_FAIL_IMMEDIATELY)
#define EX (LOCKFILE_EXCLUSIVE_LOCK)

TEST(fio_lock_exclusive_conflicts_between_handles) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_ex.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(LF(h1, 0, 10));
    EXPECT_FAIL_B(LF(h2, 0, 10), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h2, 5, 10), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h2, 9, 1), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h2, 0, 1), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h2, 0, 1000), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h2, 3, 2), ERROR_LOCK_VIOLATION);            /* nested */
    CHECK(LF(h2, 10, 10));                                       /* adjacent is fine */
    CHECK(UF(h2, 10, 10));
    CHECK(LF(h2, 20, 5));
    CHECK(UF(h1, 0, 10));
    CHECK(LF(h2, 0, 10));                                       /* free again */
    CHECK(UF(h2, 0, 10)); CHECK(UF(h2, 20, 5));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_same_handle_overlap_fails) {
    REQUIRE(U(LockFile));
    TP(p, "lk_same.bin"); mkfile_pat(p, 100);
    HANDLE h = open_rw(p, 0); OK_H(h);
    CHECK(LF(h, 10, 10));
    EXPECT_FAIL_B(LF(h, 10, 10), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h, 15, 10), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h, 5, 10), ERROR_LOCK_VIOLATION);
    CHECK(LF(h, 20, 10));
    CHECK(LF(h, 0, 10));
    CHECK(UF(h, 0, 10)); CHECK(UF(h, 10, 10)); CHECK(UF(h, 20, 10));
    CLOSE(h); rmfile(p);
}
TEST(fio_unlock_requires_exact_range) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_unl.bin"); mkfile_pat(p, 100);
    HANDLE h = open_rw(p, 0); OK_H(h);
    CHECK(LF(h, 10, 20));
    EXPECT_FAIL_B(UF(h, 10, 10), ERROR_NOT_LOCKED);          /* partial */
    EXPECT_FAIL_B(UF(h, 10, 21), ERROR_NOT_LOCKED);
    EXPECT_FAIL_B(UF(h, 11, 19), ERROR_NOT_LOCKED);
    EXPECT_FAIL_B(UF(h, 0, 100), ERROR_NOT_LOCKED);
    EXPECT_FAIL_B(UF(h, 0, 10), ERROR_NOT_LOCKED);
    CHECK(UF(h, 10, 20));
    EXPECT_FAIL_B(UF(h, 10, 20), ERROR_NOT_LOCKED);          /* already released */
    EXPECT_FAIL_B(UF(h, 500, 3), ERROR_NOT_LOCKED);          /* never locked */
    CLOSE(h); rmfile(p);
}
TEST(fio_unlock_adjacent_locks_not_merged) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_adj.bin"); mkfile_pat(p, 100);
    HANDLE h = open_rw(p, 0); OK_H(h);
    CHECK(LF(h, 0, 10)); CHECK(LF(h, 10, 10));
    EXPECT_FAIL_B(UF(h, 0, 20), ERROR_NOT_LOCKED);
    CHECK(UF(h, 10, 10)); CHECK(UF(h, 0, 10));
    CLOSE(h); rmfile(p);
}
TEST(fio_unlock_by_other_handle_fails) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_oth.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(LF(h1, 0, 10));
    EXPECT_FAIL_B(UF(h2, 0, 10), ERROR_NOT_LOCKED);
    EXPECT_FAIL_B(LF(h2, 0, 10), ERROR_LOCK_VIOLATION);
    CHECK(UF(h1, 0, 10));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_shared_locks_coexist) {
    REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx));
    TP(p, "lk_sh.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0), h3 = open_rw(p, 0); OK_H(h1); OK_H(h2); OK_H(h3);
    CHECK(lockex(h1, FI, 0, 10));
    CHECK(lockex(h2, FI, 0, 10));
    CHECK(lockex(h2, FI, 5, 10));         /* same handle may overlap its own shared locks */
    SET_ERR(0);
    EXPECT_FAIL_B(lockex(h3, FI | EX, 0, 10), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(lockex(h3, FI | EX, 14, 1), ERROR_LOCK_VIOLATION);
    CHECK(lockex(h3, FI | EX, 15, 1));
    CHECK(unlockex(h3, 15, 1));
    CHECK(unlockex(h1, 0, 10));
    EXPECT_FAIL_B(lockex(h3, FI | EX, 0, 10), ERROR_LOCK_VIOLATION);   /* h2 still holds 0..10 and 5..15 */
    CHECK(unlockex(h2, 0, 10));
    EXPECT_FAIL_B(lockex(h3, FI | EX, 5, 5), ERROR_LOCK_VIOLATION);
    CHECK(unlockex(h2, 5, 10));
    CHECK(lockex(h3, FI | EX, 0, 20));
    CHECK(unlockex(h3, 0, 20));
    CLOSE(h1); CLOSE(h2); CLOSE(h3); rmfile(p);
}
TEST(fio_lock_shared_twice_needs_two_unlocks) {
    REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx));
    TP(p, "lk_sh2.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(lockex(h1, FI, 0, 10)); CHECK(lockex(h1, FI, 0, 10));
    CHECK(unlockex(h1, 0, 10));
    EXPECT_FAIL_B(lockex(h2, FI | EX, 0, 10), ERROR_LOCK_VIOLATION);
    CHECK(unlockex(h1, 0, 10));
    CHECK(lockex(h2, FI | EX, 0, 10));
    EXPECT_FAIL_B(unlockex(h1, 0, 10), ERROR_NOT_LOCKED);
    CHECK(unlockex(h2, 0, 10));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_exclusive_vs_shared_ordering) {
    REQUIRE(U(LockFileEx));
    TP(p, "lk_exsh.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(lockex(h1, FI | EX, 0, 10));
    EXPECT_FAIL_B(lockex(h2, FI, 0, 10), ERROR_LOCK_VIOLATION);        /* shared vs exclusive */
    /* a lock held by the same owner (file object, pid, key) never conflicts: own shared request is granted */
    CHECK(lockex(h1, FI, 5, 10)); CHECK(unlockex(h1, 5, 10));
    CHECK(unlockex(h1, 0, 10));
    CHECK(lockex(h1, FI, 0, 10));
    EXPECT_FAIL_B(lockex(h1, FI | EX, 0, 10), ERROR_LOCK_VIOLATION);   /* no upgrade */
    CHECK(unlockex(h1, 0, 10));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_io_checks_exclusive) {
    REQUIRE(U(LockFile)); REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    TP(p, "lk_io.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(LF(h1, 10, 10));
    char b[64]; DWORD n = 77;
    hsetpos(h2, 0);
    CHECK(U(ReadFile)(h2, b, 10, &n, NULL)); EXPECT_EQ(n, 10);           /* before the region */
    EXPECT_EQ(fpos(h2), 10);
    n = 77; EXPECT_FAIL_B(U(ReadFile)(h2, b, 1, &n, NULL), ERROR_LOCK_VIOLATION); EXPECT_EQ(n, 0);
    EXPECT_EQ(fpos(h2), 10);                                              /* no movement on failure */
    hsetpos(h2, 5);
    n = 77; EXPECT_FAIL_B(U(ReadFile)(h2, b, 10, &n, NULL), ERROR_LOCK_VIOLATION); EXPECT_EQ(n, 0);  /* straddling */
    EXPECT_EQ(fpos(h2), 5);
    hsetpos(h2, 19);
    n = 77; EXPECT_FAIL_B(U(ReadFile)(h2, b, 2, &n, NULL), ERROR_LOCK_VIOLATION);
    hsetpos(h2, 20);
    CHECK(U(ReadFile)(h2, b, 10, &n, NULL)); EXPECT_EQ(n, 10);
    hsetpos(h2, 12);
    n = 77; EXPECT_FAIL_B(U(WriteFile)(h2, "zz", 2, &n, NULL), ERROR_LOCK_VIOLATION); EXPECT_EQ(n, 0);
    hsetpos(h2, 30);
    CHECK(U(WriteFile)(h2, "zz", 2, &n, NULL)); EXPECT_EQ(n, 2);
    /* the lock owner is unaffected */
    hsetpos(h1, 10);
    CHECK(U(ReadFile)(h1, b, 10, &n, NULL)); EXPECT_EQ(n, 10);
    hsetpos(h1, 10);
    CHECK(U(WriteFile)(h1, "OWNER", 5, &n, NULL)); EXPECT_EQ(n, 5);
    /* zero length IO inside a locked range */
    hsetpos(h2, 12);
    n = 77; BOOL ok = U(ReadFile)(h2, b, 0, &n, NULL);
    t_note("zero-length read in locked range: ok=%d gle=%lu", ok, GetLastError());
    CHECK(UF(h1, 10, 10));
    hsetpos(h2, 12);
    CHECK(U(ReadFile)(h2, b, 4, &n, NULL)); EXPECT_EQ(n, 4);
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_io_checks_shared) {
    REQUIRE(U(LockFileEx)); REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    TP(p, "lk_iosh.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(lockex(h1, FI, 10, 10));
    char b[64]; DWORD n = 77;
    hsetpos(h2, 10);
    CHECK(U(ReadFile)(h2, b, 10, &n, NULL)); EXPECT_EQ(n, 10);           /* readers allowed through shared lock */
    hsetpos(h2, 10);
    n = 77; EXPECT_FAIL_B(U(WriteFile)(h2, "q", 1, &n, NULL), ERROR_LOCK_VIOLATION);
    hsetpos(h1, 10);
    n = 77; BOOL ok = U(WriteFile)(h1, "q", 1, &n, NULL);
    t_note("owner write into own shared lock: ok=%d gle=%lu", ok, GetLastError());
    CHECK(unlockex(h1, 10, 10));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_beyond_eof_and_no_extension) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_eof.bin"); mkfile_pat(p, 10);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(LF(h1, 1000, 100));
    EXPECT_EQ(hsize(h1), 10);                                        /* lock does not extend the file */
    EXPECT_FAIL_B(LF(h2, 1050, 1), ERROR_LOCK_VIOLATION);
    CHECK(LF(h2, 1100, 1));
    CHECK(LF(h1, 0x100000000ULL, 5));                               /* beyond 4 GiB */
    EXPECT_FAIL_B(LF(h2, 0x100000002ULL, 5), ERROR_LOCK_VIOLATION);
    CHECK(UF(h1, 1000, 100)); CHECK(UF(h2, 1100, 1)); CHECK(UF(h1, 0x100000000ULL, 5));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_whole_file_range) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_all.bin"); mkfile_pat(p, 10);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(U(LockFile)(h1, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF));
    EXPECT_FAIL_B(LF(h2, 0, 1), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(LF(h2, 0x7FFFFFFFFFFFFFFFULL, 1), ERROR_LOCK_VIOLATION);
    CHECK(U(UnlockFile)(h1, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF));
    CHECK(LF(h2, 0, 1)); CHECK(UF(h2, 0, 1));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_range_overflow) {
    REQUIRE(U(LockFile));
    TP(p, "lk_ovf.bin"); mkfile_pat(p, 10);
    HANDLE h = open_rw(p, 0); OK_H(h);
    SET_ERR(0);
    /* FsRtlPrivateLock rejects a range only when last byte (offset+len-1) wraps: offset 1, len 2^64-1 ends
       exactly at 2^64-1 and is accepted; offset 2 with the same length wraps */
    BOOL ok = U(LockFile)(h, 1, 0, 0xFFFFFFFF, 0xFFFFFFFF);
    t_note("LockFile(offset=1,len=2^64-1) ok=%d gle=%lu", ok, GetLastError());
    CHECK(ok);
    if (ok) U(UnlockFile)(h, 1, 0, 0xFFFFFFFF, 0xFFFFFFFF);
    SET_ERR(0);
    ok = U(LockFile)(h, 2, 0, 0xFFFFFFFF, 0xFFFFFFFF);
    DWORD e = GetLastError();
    CHECK(!ok);
    t_note("LockFile(offset=2,len=2^64-1) gle=%lu", e);
    CHECK(e == ERROR_INVALID_LOCK_RANGE || e == ERROR_INVALID_PARAMETER || e == ERROR_LOCK_VIOLATION);
    if (ok) U(UnlockFile)(h, 2, 0, 0xFFFFFFFF, 0xFFFFFFFF);
    SET_ERR(0);
    ok = U(LockFile)(h, 0xFFFFFFFF, 0xFFFFFFFF, 2, 0);
    t_note("LockFile(offset=2^64-1,len=2) ok=%d gle=%lu", ok, GetLastError());
    CHECK(!ok);
    if (ok) U(UnlockFile)(h, 0xFFFFFFFF, 0xFFFFFFFF, 2, 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_lock_zero_length_range) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_zero.bin"); mkfile_pat(p, 10);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    SET_ERR(0);
    BOOL ok = LF(h1, 5, 0);
    t_note("LockFile(len 0) ok=%d gle=%lu", ok, GetLastError());
    if (ok) {
        BOOL ok2 = LF(h2, 5, 1);
        t_note("lock inside zero-length lock ok=%d", ok2);
        if (ok2) UF(h2, 5, 1);
        SET_ERR(0); ok = UF(h1, 5, 0);
        t_note("UnlockFile(len 0) ok=%d gle=%lu", ok, GetLastError());
    }
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lock_released_on_close) {
    REQUIRE(U(LockFile));
    TP(p, "lk_close.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(LF(h1, 0, 50));
    EXPECT_FAIL_B(LF(h2, 10, 5), ERROR_LOCK_VIOLATION);
    CLOSE(h1);
    CHECK(LF(h2, 10, 5));
    CHECK(UF(h2, 10, 5));
    CLOSE(h2); rmfile(p);
}
TEST(fio_lock_duplicate_handle_shares_lock_owner) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_dup.bin"); mkfile_pat(p, 100);
    HANDLE h = open_rw(p, 0), d = NULL; OK_H(h);
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(LF(h, 0, 10));
    EXPECT_FAIL_B(LF(d, 0, 10), ERROR_LOCK_VIOLATION);          /* same file object: own exclusive lock conflicts */
    CHECK(UF(d, 0, 10));                                         /* but the duplicate may release it */
    CHECK(LF(d, 0, 10));
    CloseHandle(h);
    /* closing one handle does not release locks while the file object lives on */
    HANDLE o = open_rw(p, 0); OK_H(o);
    EXPECT_FAIL_B(LF(o, 0, 10), ERROR_LOCK_VIOLATION);
    CLOSE(o);
    CHECK(UF(d, 0, 10));
    CloseHandle(d); rmfile(p);
}
TEST(fio_lock_reopened_handle_is_different_owner) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile)); REQUIRE(U(ReOpenFile));
    TP(p, "lk_reopen.bin"); mkfile_pat(p, 100);
    HANDLE h = open_rw(p, 0); OK_H(h);
    HANDLE r = U(ReOpenFile)(h, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, 0); OK_H(r);
    CHECK(LF(h, 0, 10));
    EXPECT_FAIL_B(LF(r, 0, 10), ERROR_LOCK_VIOLATION);
    EXPECT_FAIL_B(UF(r, 0, 10), ERROR_NOT_LOCKED);
    CHECK(UF(h, 0, 10));
    CLOSE(r); CLOSE(h); rmfile(p);
}
TEST(fio_lock_invalid_handles) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile)); REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx));
    EXPECT_FAIL_B(LF(INVALID_HANDLE_VALUE, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(LF(NULL, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(UF(INVALID_HANDLE_VALUE, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(UF(NULL, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(lockex(INVALID_HANDLE_VALUE, FI, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(lockex(NULL, FI, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(unlockex(INVALID_HANDLE_VALUE, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(unlockex(NULL, 0, 1), ERROR_INVALID_HANDLE);
    TP(p, "lk_closed.bin"); mkfile_pat(p, 4);
    HANDLE h = open_rw(p, 0); CloseHandle(h);
    EXPECT_FAIL_B(LF(h, 0, 1), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(UF(h, 0, 1), ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_lockex_reserved_must_be_zero) {
    REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx));
    TP(p, "lk_rsv.bin"); mkfile_pat(p, 10);
    HANDLE h = open_rw(p, 0); OK_H(h);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    static const DWORD rsv[] = {1, 2, 0x80000000, 0xFFFFFFFF};
    for (size_t i = 0; i < sizeof rsv / sizeof *rsv; i++) {
        SET_ERR(0);
        BOOL ok = U(LockFileEx)(h, FI, rsv[i], 1, 0, &ov);
        DWORD e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_PARAMETER, "LockFileEx reserved=0x%lx ok=%d gle=%lu", rsv[i], ok, e);
        SET_ERR(0);
        ok = U(UnlockFileEx)(h, rsv[i], 1, 0, &ov);
        e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_PARAMETER, "UnlockFileEx reserved=0x%lx ok=%d gle=%lu", rsv[i], ok, e);
    }
    CHECK(!exists("") );
    /* nothing was locked by the failed calls */
    EXPECT_FAIL_B(unlockex(h, 0, 1), ERROR_NOT_LOCKED);
    CLOSE(h); rmfile(p);
}
TEST(fio_lockex_flag_bits) {
    REQUIRE(U(LockFileEx));
    TP(p, "lk_flags.bin"); mkfile_pat(p, 10);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    /* unknown flag bits are ignored: only bit0 (fail immediately) and bit1 (exclusive) matter */
    CHECK(lockex(h1, FI | EX | 0xFFFFFFFC, 0, 5));
    EXPECT_FAIL_B(lockex(h2, FI | 0xFFFFFFFC, 0, 5), ERROR_LOCK_VIOLATION);
    CHECK(unlockex(h1, 0, 5));
    CHECK(lockex(h1, 0, 20, 5));              /* flags 0: blocking shared, uncontended */
    CHECK(unlockex(h1, 20, 5));
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_lockex_overlapped_handle_pending_then_granted) {
    REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx)); REQUIRE(U(GetOverlappedResult));
    TP(p, "lk_pend.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, FILE_FLAG_OVERLAPPED); OK_H(h1); OK_H(h2);
    CHECK(lockex(h1, FI | EX, 0, 10));
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    BOOL ok = U(LockFileEx)(h2, EX, 0, 10, 0, &ov);
    DWORD e = GetLastError();
    CHECK(!ok); EXPECT_EQ(e, ERROR_IO_PENDING);
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 150), WAIT_TIMEOUT);
    CHECK(unlockex(h1, 0, 10));
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 5000), WAIT_OBJECT_0);
    DWORD n = 0; CHECK(U(GetOverlappedResult)(h2, &ov, &n, FALSE));
    CHECK(unlockex(h2, 0, 10));
    CloseHandle(ov.hEvent); CLOSE(h1); CLOSE(h2); rmfile(p);
}
typedef struct { HANDLE h; DWORD ok; DWORD err; } LKT;
static DWORD WINAPI lock_thread(LPVOID a) {
    LKT *t = (LKT *)a; OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    t->ok = U(LockFileEx)(t->h, EX, 0, 10, 0, &ov);      /* blocks on a synchronous handle */
    t->err = GetLastError();
    return 0;
}
TEST(fio_lockex_blocking_sync_handle_waits_for_unlock) {
    REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx));
    TP(p, "lk_block.bin"); mkfile_pat(p, 100);
    HANDLE h1 = open_rw(p, 0), h2 = open_rw(p, 0); OK_H(h1); OK_H(h2);
    CHECK(lockex(h1, FI | EX, 0, 10));
    LKT t = {h2, 0xAAAA, 0};
    HANDLE th = CreateThread(NULL, 0, lock_thread, &t, 0, NULL);
    CHECK(th != NULL);
    EXPECT_EQ(WaitForSingleObject(th, 250), WAIT_TIMEOUT);       /* still blocked */
    CHECK(unlockex(h1, 0, 10));
    EXPECT_EQ(WaitForSingleObject(th, 10000), WAIT_OBJECT_0);
    EXPECT_EQ(t.ok, 1);
    CHECK(unlockex(h2, 0, 10));
    CloseHandle(th); CLOSE(h1); CLOSE(h2); rmfile(p);
}

/* ---------- cross process ---------- */
CHILD(fio_lockchild_excl) {
    if (argc < 2 || !U(LockFile)) return 90;
    HANDLE h = U(CreateFileA)(argv[1], GENERIC_READ | GENERIC_WRITE, SHARE_ALL, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 91;
    int r = 0; char b[16]; DWORD n;
    SetLastError(0);
    if (LF(h, 0, 10) || GetLastError() != ERROR_LOCK_VIOLATION) r |= 1;
    if (!LF(h, 10, 10)) r |= 2; else if (!UF(h, 10, 10)) r |= 4;
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    SetLastError(0);
    if (U(ReadFile)(h, b, 4, &n, NULL) || GetLastError() != ERROR_LOCK_VIOLATION) r |= 8;
    SetFilePointer(h, 5, NULL, FILE_BEGIN);
    SetLastError(0);
    if (U(WriteFile)(h, "x", 1, &n, NULL) || GetLastError() != ERROR_LOCK_VIOLATION) r |= 16;
    SetLastError(0);
    if (UF(h, 0, 10) || GetLastError() != ERROR_NOT_LOCKED) r |= 32;
    SetFilePointer(h, 20, NULL, FILE_BEGIN);
    if (!U(ReadFile)(h, b, 4, &n, NULL) || n != 4) r |= 64;
    CloseHandle(h);
    return r;
}
CHILD(fio_lockchild_shared) {
    if (argc < 2 || !U(LockFileEx)) return 90;
    HANDLE h = U(CreateFileA)(argv[1], GENERIC_READ | GENERIC_WRITE, SHARE_ALL, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 91;
    int r = 0; char b[16]; DWORD n; OVERLAPPED ov;
    memset(&ov, 0, sizeof ov);
    if (!U(LockFileEx)(h, FI, 0, 10, 0, &ov)) r |= 1; else { if (!U(UnlockFileEx)(h, 0, 10, 0, &ov)) r |= 2; }
    SetLastError(0);
    if (U(LockFileEx)(h, FI | EX, 0, 10, 0, &ov) || GetLastError() != ERROR_LOCK_VIOLATION) r |= 4;
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    if (!U(ReadFile)(h, b, 10, &n, NULL) || n != 10) r |= 8;
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    SetLastError(0);
    if (U(WriteFile)(h, "x", 1, &n, NULL) || GetLastError() != ERROR_LOCK_VIOLATION) r |= 16;
    CloseHandle(h);
    return r;
}
CHILD(fio_lockchild_holder) {   /* locks and exits without unlocking */
    if (argc < 2 || !U(LockFile)) return 90;
    HANDLE h = U(CreateFileA)(argv[1], GENERIC_READ | GENERIC_WRITE, SHARE_ALL, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 91;
    if (!LF(h, 0, 20)) return 92;
    return 0;
}
TEST(fio_lock_cross_process_exclusive) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_xp1.bin"); mkfile_pat(p, 100);
    HANDLE h = open_rw(p, 0); OK_H(h);
    CHECK(LF(h, 0, 10));
    char arg[MAX_PATH * 2 + 8]; snprintf(arg, sizeof arg, "\"%s\"", p);
    DWORD rc = t_run_child("fio_lockchild_excl", arg, 25000);
    CHECKF(rc == 0, "child result bitmask 0x%lx", rc);
    CHECK(UF(h, 0, 10));
    CLOSE(h); rmfile(p);
}
TEST(fio_lock_cross_process_shared) {
    REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx));
    TP(p, "lk_xp2.bin"); mkfile_pat(p, 100);
    HANDLE h = open_rw(p, 0); OK_H(h);
    CHECK(lockex(h, FI, 0, 10));
    char arg[MAX_PATH * 2 + 8]; snprintf(arg, sizeof arg, "\"%s\"", p);
    DWORD rc = t_run_child("fio_lockchild_shared", arg, 25000);
    CHECKF(rc == 0, "child result bitmask 0x%lx", rc);
    CHECK(unlockex(h, 0, 10));
    CLOSE(h); rmfile(p);
}
TEST(fio_lock_released_when_process_exits) {
    REQUIRE(U(LockFile)); REQUIRE(U(UnlockFile));
    TP(p, "lk_xp3.bin"); mkfile_pat(p, 100);
    char arg[MAX_PATH * 2 + 8]; snprintf(arg, sizeof arg, "\"%s\"", p);
    EXPECT_EQ(t_run_child("fio_lockchild_holder", arg, 25000), 0);
    HANDLE h = open_rw(p, 0); OK_H(h);
    CHECK(LF(h, 0, 20));
    CHECK(UF(h, 0, 20));
    CLOSE(h); rmfile(p);
}
TEST(fio_lock_inherited_handle_shares_owner_with_parent) {
    REQUIRE(U(LockFile));
    /* An inheritable handle duplicated into a child refers to the same file object, but the lock key is the process:
       the child's attempt to lock the parent's locked range conflicts. Verified through a child using the inherited handle. */
    TP(p, "lk_inh.bin"); mkfile_pat(p, 100);
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE h = U(CreateFileA)(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, &sa, OPEN_EXISTING, 0, NULL); OK_H(h);
    CHECK(LF(h, 0, 10));
    char arg[64]; snprintf(arg, sizeof arg, "%llu", (unsigned long long)(ULONG_PTR)h);
    PROCESS_INFORMATION pi;
    HANDLE ph = t_spawn_child("fio_lockchild_inh", arg, 0, NULL, NULL, NULL, TRUE, NULL, &pi);
    CHECK(ph != NULL);
    if (ph) {
        DWORD code = 0xDEAD;
        if (WaitForSingleObject(ph, 20000) != WAIT_OBJECT_0) { TerminateProcess(ph, 1); WaitForSingleObject(ph, 3000); code = 0xDEAD0001; }
        else GetExitCodeProcess(ph, &code);
        CHECKF(code == 0, "child code 0x%lx", code);
        CloseHandle(pi.hThread); CloseHandle(ph);
    }
    CHECK(UF(h, 0, 10));
    CLOSE(h); rmfile(p);
}
CHILD(fio_lockchild_inh) {
    if (argc < 2 || !U(LockFile)) return 90;
    HANDLE h = (HANDLE)(ULONG_PTR)strtoull(argv[1], NULL, 10);
    int r = 0;
    SetLastError(0);
    /* Different process, same file object: exclusive overlapping lock conflicts; unlocking someone else's lock is refused or succeeds. */
    if (LF(h, 0, 10)) r |= 1;
    else if (GetLastError() != ERROR_LOCK_VIOLATION) r |= 2;
    if (!LF(h, 50, 10)) r |= 4; else if (!UF(h, 50, 10)) r |= 8;
    return r;
}
