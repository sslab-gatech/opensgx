/**
 * Architecture dependent entry points.
 *
 * Derived from asm-linkage.h in the Sandia Kitten microkernel
 * (http://github.com/ktpedre/kitten) As such, it is available
 * under Linus Torvald's Linux kernel license, a variant of the GNU
 * GPL v2.
 */

#ifndef _ASM_LINKAGE_H
#define _ASM_LINKAGE_H

#ifdef __cplusplus
#define CPP_ASMLINKAGE extern "C"
#else
#define CPP_ASMLINKAGE
#endif

#ifndef asmlinkage
#define asmlinkage CPP_ASMLINKAGE
#endif

#ifndef prevent_tail_call
# define prevent_tail_call(ret) do { } while (0)
#endif

#ifndef __ALIGN
#define __ALIGN		.align 4,0x90
#define __ALIGN_STR	".align 4,0x90"
#endif

#ifdef __ASSEMBLY__

#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR

#ifndef ENTRY
#define ENTRY(name) \
  .globl name; \
  ALIGN; \
  name:
#endif

#define KPROBE_ENTRY(name) \
  .section .kprobes.text, "ax"; \
  ENTRY(name)

#ifndef END
#define END(name) \
  .size name, .-name
#endif

#ifndef ENDPROC
#define ENDPROC(name) \
  .type name, @function; \
  END(name)
#endif

#endif

#define NORET_TYPE    /**/
#define ATTRIB_NORET  __attribute__((noreturn))
#define NORET_AND     noreturn,

#ifndef FASTCALL
#define FASTCALL(x)	x
#define fastcall
#endif

#endif
