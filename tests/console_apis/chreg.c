/* console_apis: child dispatcher. The harness limits the number of registered children, so every console
 * test body registers here and is reached through the single "con_dispatch" child. */
#include "t.h"
#include "ch.h"

#define CH_MAX 2048
static struct { const char *name; int (*fn)(int, char **); } g_tab[CH_MAX];
static int g_ntab;

void ch_register(const char *name, int (*fn)(int, char **)) {
    if (g_ntab < CH_MAX) { g_tab[g_ntab].name = name; g_tab[g_ntab].fn = fn; g_ntab++; }
}

CHILD(con_dispatch) {
    if (argc < 3) return 0xC0DE0003;
    for (int i = 0; i < g_ntab; i++)
        if (!strcmp(g_tab[i].name, argv[1])) return g_tab[i].fn(argc - 1, argv + 1);
    return 0xC0DE0002;
}
