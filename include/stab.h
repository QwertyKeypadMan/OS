/* Symbol table entries for STABS format.
   Bu dosya GDB, binutils ve TCC tarafindan ortak kullanilan, uzun
   suredir degismemis standart bir header'dir -- N_* makrolari STABS
   debug formatinin sembol turu kodlaridir. */

#ifndef __STAB_GNU_H__
#define __STAB_GNU_H__

#define N_GSYM  0x20    /* global symbol */
#define N_FNAME 0x22    /* F77 function name */
#define N_FUN   0x24    /* procedure name */
#define N_STSYM 0x26    /* data segment variable */
#define N_LCSYM 0x28    /* bss segment variable */
#define N_MAIN  0x2a    /* name of main routine */
#define N_ROSYM 0x2c    /* variable in .rodata section */
#define N_PC    0x30    /* global symbol (for Pascal) */
#define N_NSYMS 0x32    /* number of symbols (Ultrix V4.0) */
#define N_NOMAP 0x34    /* no data map for symbol (Ultrix V4.0) */
#define N_OBJ   0x38    /* object file (Solaris2) */
#define N_OPT   0x3c    /* options for the debugger (Solaris2) */
#define N_RSYM  0x40    /* register variable */
#define N_M2C   0x42    /* modula-2 compilation unit */
#define N_SLINE 0x44    /* line number in text segment */
#define N_DSLINE 0x46   /* line number in data segment */
#define N_BSLINE 0x48   /* line number in bss segment */
#define N_BROWS 0x48    /* path to .cb file for Sun source code browser */
#define N_DEFD  0x4a    /* GNU Modula2 definition module dependency */
#define N_EHDECL 0x50   /* GNU C++ exception variable */
#define N_MOD2  0x50    /* modula2 info "for imc" (according to Ultrix V4.0) */
#define N_CATCH 0x54    /* GNU C++ "catch" clause */
#define N_SSYM  0x60    /* structure or union element */
#define N_ENDM  0x62    /* last stab for module (Solaris2) */
#define N_SO    0x64    /* path and name of source file */
#define N_LSYM  0x80    /* stack variable */
#define N_BINCL 0x82    /* beginning of an include file (Sun only) */
#define N_SOL   0x84    /* name of include file */
#define N_PSYM  0xa0    /* parameter variable */
#define N_EINCL 0xa2    /* end of an include file */
#define N_ENTRY 0xa4    /* alternate entry point */
#define N_LBRAC 0xc0    /* beginning of a lexical block */
#define N_EXCL  0xc2    /* place holder for a deleted include file */
#define N_SCOPE 0xc4    /* modula2 scope information (Sun linker) */
#define N_RBRAC 0xe0    /* end of a lexical block */
#define N_BCOMM 0xe2    /* begin named common block */
#define N_ECOMM 0xe4    /* end named common block */
#define N_ECOML 0xe8    /* end common (local name) */

#define N_WITH  0xea    /* Pascal with statement: type,,0,0,offset */

#define N_NBTEXT 0xF0   /* Gould non-base registers */
#define N_NBDATA 0xF2
#define N_NBBSS  0xF4
#define N_NBSTS  0xF6
#define N_NBLCS  0xF8

/* Sun's ANSI C compiler uses these for STAB linking. */
#define N_LENG  0xfe    /* length of preceding entry */

#endif /* __STAB_GNU_H__ */