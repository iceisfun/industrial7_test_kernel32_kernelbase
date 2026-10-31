#ifndef EED_ASM_H
#define EED_ASM_H
extern void eed_av_write(char *), eed_av_read(char *), eed_call_ptr(void *), eed_div0(void), eed_idiv_ovf(void),
    eed_ud2(void), eed_int3(void), eed_cli(void), eed_hlt(void), eed_movaps(void *), eed_tf(void),
    eed_write_dword(volatile DWORD *), eed_hw_exec(void);
extern char eed_av_write_after[], eed_av_read_after[], eed_call_ptr_after[], eed_div0_insn[], eed_div0_after[],
    eed_idiv_ovf_insn[], eed_idiv_ovf_after[], eed_ud2_insn[], eed_ud2_after[], eed_int3_insn[], eed_int3_after[],
    eed_cli_insn[], eed_cli_after[], eed_hlt_insn[], eed_hlt_after[], eed_movaps_insn[], eed_movaps_after[],
    eed_write_dword_after[];
#endif
