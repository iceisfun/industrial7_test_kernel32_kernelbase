/* Assembly helpers shared by the thread tests. */
#include "tf.h"
__asm__(
    ".text\n"
    ".globl tf_regloop\n"
    "tf_regloop:\n"
    "    xorl %eax, %eax\n"
    "tf_regloop_spin:\n"
    "    testl %eax, %eax\n"
    "    jz tf_regloop_spin\n"
    "    ret\n"
    ".globl tf_regloop_end\n"
    "tf_regloop_end:\n"
    "    nop\n"
);
