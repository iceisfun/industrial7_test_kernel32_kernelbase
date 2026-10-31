/* gap_11: make sure the private HKCU key never outlives the run, even if a test returned early. */
#include "g11.h"

static void final_cleanup(void) { g11_clean(); }
static void __attribute__((constructor)) g11_cleanup_reg(void) { atexit(final_cleanup); }
