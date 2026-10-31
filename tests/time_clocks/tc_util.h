#ifndef TC_UTIL_H
#define TC_UTIL_H
#include "t.h"
#define TC_EPOCH 116444736000000000ULL
#define TC_MAXFT 0x7FFFFFFFFFFFFFFFULL
static inline unsigned long long tc_u64(const FILETIME *f) { return ((unsigned long long)f->dwHighDateTime << 32) | f->dwLowDateTime; }
static inline FILETIME tc_ft(unsigned long long v) { FILETIME f; f.dwLowDateTime = (DWORD)v; f.dwHighDateTime = (DWORD)(v >> 32); return f; }
static inline int tc_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static inline int tc_dim(int y, int m) { static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31}; return m == 2 && tc_leap(y) ? 29 : d[m - 1]; }
/* days since 1970-01-01 (civil algorithm) */
static inline long long tc_days(long long y, int m, int d) {
    y -= m <= 2; long long era = (y >= 0 ? y : y - 399) / 400; long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
/* expected FILETIME for a civil time */
static inline unsigned long long tc_mkft(int y, int mo, int d, int h, int mi, int s, int ms) {
    long long secs = tc_days(y, mo, d) * 86400LL + h * 3600 + mi * 60 + s;
    return (unsigned long long)((secs * 1000LL + ms) * 10000LL) + TC_EPOCH;
}
/* day of week: 0=Sunday (Zeller-style, independent of tc_days) */
static inline int tc_dow(int y, int m, int d) {
    int q = d, mm = m, yy = y; if (mm < 3) { mm += 12; yy--; }
    int K = yy % 100, J = yy / 100;
    int h = (q + 13 * (mm + 1) / 5 + K + K / 4 + J / 4 + 5 * J) % 7; /* 0=Sat */
    return (h + 6) % 7;
}
static inline unsigned tc_lcg(unsigned long long *s) { *s = *s * 6364136223846793005ULL + 1442695040888963407ULL; return (unsigned)(*s >> 33); }
static inline unsigned long long tc_rand64(unsigned long long *s) { return ((unsigned long long)tc_lcg(s) << 40) ^ ((unsigned long long)tc_lcg(s) << 20) ^ tc_lcg(s); }
static inline unsigned long long tc_now100(void) { FILETIME f; GetSystemTimeAsFileTime(&f); return tc_u64(&f); }
#endif
