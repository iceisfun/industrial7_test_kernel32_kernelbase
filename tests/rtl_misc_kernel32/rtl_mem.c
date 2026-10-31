/* Rtl memory helpers exported by kernel32: RtlMoveMemory/RtlCopyMemory/RtlFillMemory/RtlZeroMemory/RtlCompareMemory.
 * Declared locally (the SDK headers turn several of these into macros). */
#include "t.h"

typedef PVOID  (WINAPI *rm_move_t)(PVOID, const void *, SIZE_T);
typedef void   (WINAPI *rm_fill_t)(PVOID, SIZE_T, UCHAR);
typedef void   (WINAPI *rm_zero_t)(PVOID, SIZE_T);
typedef SIZE_T (WINAPI *rm_cmp_t)(const void *, const void *, SIZE_T);

#define RM_MOVE ((rm_move_t)t_proc("RtlMoveMemory"))
#define RM_COPY ((rm_move_t)t_proc("RtlCopyMemory"))
#define RM_FILL ((rm_fill_t)t_proc("RtlFillMemory"))
#define RM_ZERO ((rm_zero_t)t_proc("RtlZeroMemory"))
#define RM_CMP  ((rm_cmp_t)t_proc("RtlCompareMemory"))

static void fill_pat(unsigned char *p, size_t n, unsigned char seed) { for (size_t i = 0; i < n; i++) p[i] = (unsigned char)(seed + i * 7); }

TEST(rtlmem_exports_present_in_kernel32) {
    CHECK(t_proc_k32("RtlMoveMemory") != NULL);
    CHECK(t_proc_k32("RtlCopyMemory") != NULL);
    CHECK(t_proc_k32("RtlFillMemory") != NULL);
    CHECK(t_proc_k32("RtlZeroMemory") != NULL);
    CHECK(t_proc_k32("RtlCompareMemory") != NULL);
}

TEST(rtlmem_exports_present_in_ntdll) {
    CHECK(t_proc_nt("RtlMoveMemory") != NULL);
    CHECK(t_proc_nt("RtlCopyMemory") != NULL);
    CHECK(t_proc_nt("RtlFillMemory") != NULL);
    CHECK(t_proc_nt("RtlZeroMemory") != NULL);
    CHECK(t_proc_nt("RtlCompareMemory") != NULL);
}

TEST(rtlmem_copy_basic) {
    REQUIRE(RM_COPY);
    unsigned char src[64], dst[64];
    fill_pat(src, 64, 3); memset(dst, 0xEE, 64);
    RM_COPY(dst, src, 64);
    CHECK(!memcmp(src, dst, 64));
}

TEST(rtlmem_copy_all_sizes_0_to_300) {
    REQUIRE(RM_COPY);
    unsigned char src[320], dst[320];
    fill_pat(src, sizeof src, 11);
    for (size_t n = 0; n <= 300; n++) {
        memset(dst, 0xA5, sizeof dst);
        RM_COPY(dst + 1, src + 3, n);
        if (memcmp(dst + 1, src + 3, n) != 0) { CHECKF(0, "n=%zu body mismatch", n); break; }
        if (dst[0] != 0xA5 || dst[1 + n] != 0xA5) { CHECKF(0, "n=%zu guard byte clobbered", n); break; }
    }
    CHECK(1);
}

TEST(rtlmem_copy_zero_length_noop) {
    REQUIRE(RM_COPY);
    unsigned char a[8] = {1,2,3,4,5,6,7,8}, b[8] = {9,9,9,9,9,9,9,9};
    RM_COPY(a, b, 0);
    CHECK(a[0] == 1 && a[7] == 8);
}

TEST(rtlmem_copy_zero_length_null_pointers) {
    REQUIRE(RM_COPY);
    RM_COPY(NULL, NULL, 0);   /* memcpy of 0 bytes must not touch memory */
    CHECK(1);
}

TEST(rtlmem_copy_return_value_is_dest) {
    REQUIRE(RM_COPY);
    char a[8] = "abcdefg", b[8] = {0};
    PVOID r = RM_COPY(b, a, 8);
    EXPECT_EQ(r, b);
}

TEST(rtlmem_move_basic) {
    REQUIRE(RM_MOVE);
    unsigned char src[100], dst[100];
    fill_pat(src, 100, 1); memset(dst, 0, 100);
    RM_MOVE(dst, src, 100);
    CHECK(!memcmp(src, dst, 100));
}

TEST(rtlmem_move_overlap_forward) {   /* dst > src: must behave like memmove */
    REQUIRE(RM_MOVE);
    for (size_t n = 1; n <= 200; n += 7) {
        unsigned char buf[512], ref[512];
        fill_pat(buf, sizeof buf, 5); memcpy(ref, buf, sizeof buf);
        memmove(ref + 20, ref, n);
        RM_MOVE(buf + 20, buf, n);
        if (memcmp(buf, ref, sizeof buf)) { CHECKF(0, "forward overlap n=%zu", n); return; }
    }
    CHECK(1);
}

TEST(rtlmem_move_overlap_backward) {  /* dst < src */
    REQUIRE(RM_MOVE);
    for (size_t n = 1; n <= 200; n += 7) {
        unsigned char buf[512], ref[512];
        fill_pat(buf, sizeof buf, 9); memcpy(ref, buf, sizeof buf);
        memmove(ref, ref + 13, n);
        RM_MOVE(buf, buf + 13, n);
        if (memcmp(buf, ref, sizeof buf)) { CHECKF(0, "backward overlap n=%zu", n); return; }
    }
    CHECK(1);
}

TEST(rtlmem_move_overlap_by_one) {
    REQUIRE(RM_MOVE);
    unsigned char buf[40], ref[40];
    fill_pat(buf, 40, 0); memcpy(ref, buf, 40);
    memmove(ref + 1, ref, 38);
    RM_MOVE(buf + 1, buf, 38);
    CHECK(!memcmp(buf, ref, 40));
    fill_pat(buf, 40, 0); memcpy(ref, buf, 40);
    memmove(ref, ref + 1, 38);
    RM_MOVE(buf, buf + 1, 38);
    CHECK(!memcmp(buf, ref, 40));
}

TEST(rtlmem_move_same_pointer) {
    REQUIRE(RM_MOVE);
    unsigned char buf[33], ref[33];
    fill_pat(buf, 33, 4); memcpy(ref, buf, 33);
    RM_MOVE(buf, buf, 33);
    CHECK(!memcmp(buf, ref, 33));
}

TEST(rtlmem_move_zero_length) {
    REQUIRE(RM_MOVE);
    unsigned char a[4] = {1,2,3,4};
    RM_MOVE(a, a + 1, 0);
    CHECK(a[0] == 1 && a[3] == 4);
}

TEST(rtlmem_move_large_4mb) {
    REQUIRE(RM_MOVE);
    size_t n = 4u << 20;
    unsigned char *p = (unsigned char *)VirtualAlloc(NULL, n + 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    for (size_t i = 0; i < n + 4096; i++) p[i] = (unsigned char)(i * 31 + (i >> 8));
    unsigned char *ref = (unsigned char *)VirtualAlloc(NULL, n + 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(ref);
    memcpy(ref, p, n + 4096);
    memmove(ref + 777, ref, n);
    RM_MOVE(p + 777, p, n);
    CHECK(!memcmp(p, ref, n + 4096));
    VirtualFree(p, 0, MEM_RELEASE); VirtualFree(ref, 0, MEM_RELEASE);
}

TEST(rtlmem_fill_basic) {
    REQUIRE(RM_FILL);
    unsigned char b[32]; memset(b, 0, sizeof b);
    RM_FILL(b, 32, 0xAB);
    int ok = 1; for (int i = 0; i < 32; i++) if (b[i] != 0xAB) ok = 0;
    CHECK(ok);
}

TEST(rtlmem_fill_argument_order_dest_len_fill) {
    REQUIRE(RM_FILL);
    unsigned char b[16]; memset(b, 0, sizeof b);
    RM_FILL(b, 5, 0x7F);     /* length 5, fill 0x7F */
    for (int i = 0; i < 5; i++) CHECK(b[i] == 0x7F);
    for (int i = 5; i < 16; i++) CHECK(b[i] == 0);
}

TEST(rtlmem_fill_zero_length) {
    REQUIRE(RM_FILL);
    unsigned char b[4] = {1,2,3,4};
    RM_FILL(b, 0, 0xFF);
    CHECK(b[0] == 1 && b[1] == 2 && b[2] == 3 && b[3] == 4);
}

TEST(rtlmem_fill_all_sizes_with_guards) {
    REQUIRE(RM_FILL);
    unsigned char b[300];
    for (size_t n = 0; n <= 257; n++) {
        memset(b, 0x11, sizeof b);
        RM_FILL(b + 1, n, 0xC3);
        int ok = b[0] == 0x11 && b[1 + n] == 0x11;
        for (size_t i = 0; i < n; i++) if (b[1 + i] != 0xC3) ok = 0;
        if (!ok) { CHECKF(0, "n=%zu", n); return; }
    }
    CHECK(1);
}

TEST(rtlmem_fill_every_byte_value) {
    REQUIRE(RM_FILL);
    unsigned char b[40];
    for (int v = 0; v < 256; v++) {
        memset(b, ~v, sizeof b);
        RM_FILL(b, 40, (UCHAR)v);
        int ok = 1; for (int i = 0; i < 40; i++) if (b[i] != (unsigned char)v) ok = 0;
        if (!ok) { CHECKF(0, "value %d", v); return; }
    }
    CHECK(1);
}

TEST(rtlmem_fill_unaligned_starts) {
    REQUIRE(RM_FILL);
    unsigned char b[128];
    for (int off = 0; off < 17; off++) {
        memset(b, 0, sizeof b);
        RM_FILL(b + off, 70, 0x5A);
        int ok = 1;
        for (int i = 0; i < 128; i++) { int exp = (i >= off && i < off + 70) ? 0x5A : 0; if (b[i] != exp) ok = 0; }
        if (!ok) { CHECKF(0, "off=%d", off); return; }
    }
    CHECK(1);
}

TEST(rtlmem_fill_large_page_boundary) {
    REQUIRE(RM_FILL);
    unsigned char *p = (unsigned char *)VirtualAlloc(NULL, 3 * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    RM_FILL(p + 4090, 20, 0xEE);   /* spans a page boundary */
    CHECK(p[4089] == 0 && p[4090] == 0xEE && p[4109] == 0xEE && p[4110] == 0);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(rtlmem_zero_basic) {
    REQUIRE(RM_ZERO);
    unsigned char b[64]; memset(b, 0xFF, sizeof b);
    RM_ZERO(b, 64);
    int ok = 1; for (int i = 0; i < 64; i++) if (b[i]) ok = 0;
    CHECK(ok);
}

TEST(rtlmem_zero_partial_with_guards) {
    REQUIRE(RM_ZERO);
    unsigned char b[300];
    for (size_t n = 0; n <= 257; n += 1) {
        memset(b, 0xFF, sizeof b);
        RM_ZERO(b + 3, n);
        int ok = b[2] == 0xFF && b[3 + n] == 0xFF;
        for (size_t i = 0; i < n; i++) if (b[3 + i] != 0) ok = 0;
        if (!ok) { CHECKF(0, "n=%zu", n); return; }
    }
    CHECK(1);
}

TEST(rtlmem_zero_zero_length_null) {
    REQUIRE(RM_ZERO);
    RM_ZERO(NULL, 0);
    CHECK(1);
}

TEST(rtlmem_zero_large_1mb) {
    REQUIRE(RM_ZERO);
    size_t n = 1u << 20;
    unsigned char *p = (unsigned char *)VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    memset(p, 0x77, n);
    RM_ZERO(p, n);
    int ok = 1; for (size_t i = 0; i < n; i += 4093) if (p[i]) ok = 0;
    if (p[n - 1]) ok = 0;
    CHECK(ok);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(rtlmem_compare_equal_returns_length) {
    REQUIRE(RM_CMP);
    unsigned char a[100], b[100];
    fill_pat(a, 100, 2); memcpy(b, a, 100);
    EXPECT_EQ(RM_CMP(a, b, 100), 100);
}

TEST(rtlmem_compare_zero_length_returns_zero) {
    REQUIRE(RM_CMP);
    char a = 1, b = 2;
    EXPECT_EQ(RM_CMP(&a, &b, 0), 0);
}

TEST(rtlmem_compare_zero_length_null) {
    REQUIRE(RM_CMP);
    EXPECT_EQ(RM_CMP(NULL, NULL, 0), 0);
}

TEST(rtlmem_compare_first_byte_differs) {
    REQUIRE(RM_CMP);
    unsigned char a[16] = {1}, b[16] = {2};
    EXPECT_EQ(RM_CMP(a, b, 16), 0);
}

TEST(rtlmem_compare_mismatch_at_every_index) {
    REQUIRE(RM_CMP);
    unsigned char a[130], b[130];
    for (size_t n = 1; n <= 129; n++) {
        for (size_t k = 0; k < n; k++) {
            memset(a, 0x42, sizeof a); memset(b, 0x42, sizeof b);
            b[k] ^= 0x80;
            SIZE_T r = RM_CMP(a, b, n);
            if (r != k) { CHECKF(0, "n=%zu k=%zu got %zu", n, k, (size_t)r); return; }
        }
    }
    CHECK(1);
}

TEST(rtlmem_compare_mismatch_beyond_length_ignored) {
    REQUIRE(RM_CMP);
    unsigned char a[32], b[32];
    memset(a, 1, 32); memset(b, 1, 32); b[20] = 9;
    EXPECT_EQ(RM_CMP(a, b, 20), 20);
    EXPECT_EQ(RM_CMP(a, b, 21), 20);
}

TEST(rtlmem_compare_misaligned_relative) {
    REQUIRE(RM_CMP);
    unsigned char buf[200], buf2[200];
    memset(buf, 0x33, sizeof buf); memset(buf2, 0x33, sizeof buf2);
    for (int oa = 0; oa < 8; oa++) for (int ob = 0; ob < 8; ob++) {
        buf2[ob + 50] ^= 1;
        SIZE_T r = RM_CMP(buf + oa, buf2 + ob, 100);
        buf2[ob + 50] ^= 1;
        if (r != (SIZE_T)(50 - 0)) { CHECKF(0, "oa=%d ob=%d got %zu", oa, ob, (size_t)r); return; }
    }
    CHECK(1);
}

TEST(rtlmem_compare_same_pointer) {
    REQUIRE(RM_CMP);
    unsigned char a[50]; fill_pat(a, 50, 0);
    EXPECT_EQ(RM_CMP(a, a, 50), 50);
}

TEST(rtlmem_compare_is_unsigned_bytewise_count_not_sign) {
    REQUIRE(RM_CMP);
    unsigned char a[4] = {0x80, 0, 0, 0}, b[4] = {0x7F, 0, 0, 0};
    EXPECT_EQ(RM_CMP(a, b, 4), 0);
    EXPECT_EQ(RM_CMP(b, a, 4), 0);
}

TEST(rtlmem_compare_large_buffers) {
    REQUIRE(RM_CMP);
    size_t n = 1u << 20;
    unsigned char *a = (unsigned char *)VirtualAlloc(NULL, 2 * n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(a);
    unsigned char *b = a + n;
    memset(a, 0x5C, n); memset(b, 0x5C, n);
    EXPECT_EQ(RM_CMP(a, b, n), n);
    b[n - 1] = 0;
    EXPECT_EQ(RM_CMP(a, b, n), n - 1);
    b[12345] = 0;
    EXPECT_EQ(RM_CMP(a, b, n), 12345);
    VirtualFree(a, 0, MEM_RELEASE);
}

TEST(rtlmem_compare_stops_before_unreadable_page) {
    REQUIRE(RM_CMP);
    unsigned char *p = (unsigned char *)VirtualAlloc(NULL, 2 * 4096, MEM_RESERVE, PAGE_NOACCESS);
    REQUIRE(p);
    VirtualAlloc(p, 4096, MEM_COMMIT, PAGE_READWRITE);
    unsigned char *e = p + 4096 - 8;
    unsigned char ref[8] = {1,2,3,4,5,6,7,8};
    memcpy(e, ref, 8);
    EXPECT_EQ(RM_CMP(e, ref, 8), 8);   /* exactly up to the page end */
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(rtlmem_forward_matches_ntdll_behaviour) {
    FARPROC k = t_proc_k32("RtlMoveMemory"), n = t_proc_nt("RtlMoveMemory");
    REQUIRE(k); REQUIRE(n);
    unsigned char a[64], b[64], c[64]; fill_pat(a, 64, 1); memcpy(b, a, 64); memcpy(c, a, 64);
    ((rm_move_t)k)(b + 5, b, 40);
    ((rm_move_t)n)(c + 5, c, 40);
    CHECK(!memcmp(b, c, 64));
}

TEST(rtlmem_zero_vs_ntdll_zero) {
    FARPROC k = t_proc_k32("RtlZeroMemory"), n = t_proc_nt("RtlZeroMemory");
    REQUIRE(k); REQUIRE(n);
    unsigned char a[50], b[50]; memset(a, 9, 50); memset(b, 9, 50);
    ((rm_zero_t)k)(a, 49); ((rm_zero_t)n)(b, 49);
    CHECK(!memcmp(a, b, 50));
}

TEST(rtlmem_ntdll_fill_arg_order) {
    rm_fill_t f = (rm_fill_t)t_proc_nt("RtlFillMemory"); REQUIRE(f);
    unsigned char b[10] = {0};
    f(b, 4, 0x33);
    CHECK(b[3] == 0x33 && b[4] == 0);
}

TEST(rtlmem_ntdll_compare_matches_k32) {
    rm_cmp_t a = (rm_cmp_t)t_proc_nt("RtlCompareMemory"), b = (rm_cmp_t)t_proc_k32("RtlCompareMemory");
    REQUIRE(a); REQUIRE(b);
    unsigned char x[40], y[40]; memset(x, 7, 40); memset(y, 7, 40); y[17] = 8;
    EXPECT_EQ(a(x, y, 40), b(x, y, 40));
    EXPECT_EQ(b(x, y, 40), 17);
}

TEST(rtlmem_move_does_not_touch_adjacent) {
    REQUIRE(RM_MOVE);
    unsigned char b[64]; memset(b, 0xCC, sizeof b);
    unsigned char s[16]; fill_pat(s, 16, 0);
    RM_MOVE(b + 16, s, 16);
    int ok = 1;
    for (int i = 0; i < 16; i++) if (b[i] != 0xCC || b[32 + i] != 0xCC) ok = 0;
    CHECK(ok);
    CHECK(!memcmp(b + 16, s, 16));
}

TEST(rtlmem_copy_struct_roundtrip) {
    REQUIRE(RM_COPY);
    CONTEXT a, b;
    memset(&a, 0x5A, sizeof a); memset(&b, 0, sizeof b);
    RM_COPY(&b, &a, sizeof a);
    CHECK(!memcmp(&a, &b, sizeof a));
}

TEST(rtlmem_fill_pattern_via_compare_roundtrip) {
    REQUIRE(RM_FILL); REQUIRE(RM_CMP); REQUIRE(RM_ZERO);
    unsigned char a[200], b[200];
    RM_FILL(a, 200, 0x6D); RM_FILL(b, 200, 0x6D);
    EXPECT_EQ(RM_CMP(a, b, 200), 200);
    RM_ZERO(b + 100, 100);
    EXPECT_EQ(RM_CMP(a, b, 200), 100);
}
