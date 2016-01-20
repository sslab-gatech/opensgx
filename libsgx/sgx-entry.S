/**
 * Architecture dependent entry points.
 *
 * Derived from entry.S in the Sandia Kitten microkernel
 * (http://github.com/ktpedre/kitten) As such, it is available
 * under Linus Torvald's Linux kernel license, a variant of the GNU
 * GPL v2.
 *
 * Modifications for usage for enclave entry by:
 *     2015, Patrick Bridges <patrickb314@gmail.com> for Two Sigma, Inc.
 */

#include <asm-linkage.h>

/* XXX - PGB
 * We need include files in OpenSGX to get the appropriate constants
 * from  -for now we just define them by hand.
 */
#define PAGE_SHIFT              12
#define PAGE_SIZE               (1 << PAGE_SHIFT)

/* 2 pages per SSA, last eventually write-protected */
#define SSA_FRAME_SHIFT     (PAGE_SHIFT + 1)
#define SSA_FRAME_SIZE      (1 << SSA_FRAME_SHIFT)
#define SSA_FRAME_MASK      (~((1 << SSA_FRAME_SHIFT) - 1))

/* Stack size */
#define STACK_FRAME_SIZE    (250 * PAGE_SIZE)

/**
 * Entry point for enclave calls.
 *
 * Upon entry we are still running with the user-level stack and the
 * x86_64 CPU control unit has stashed the user-level RIP in
 *
 * The first thing this function does is generate a partial stack frame
 * containing all caller-saved registers. The handler function
 * is responsible for saving all callee-saved registers. If it is a C
 * function, callee-saved registers are saved automatically by the compiler.
 *
 * Immediately before calling encalve main function, the enclave stack
 * looks like the following.  All fields represent the saved state of the
 * calling user-level task:
 *
 *            UNDEF      = Undefined, normally holds user-space SS
 *            UNDEF      = Undefined, normally holds user-space RSP
 *            UNDEF      = Undefined, normally holds user-space RFLAGS
 *            UNDEF      = Undefined, normally holds user-space CS
 *            RIP        = user-space RIP
 *            (junk)     = normally RAX, but RAX clobbered by EENTER
 *            RDI        = ARG0, passed from user-space
 *            RSI        = ARG1, passed from user-space
 *            RDX        = ARG2, passed from user-space
 *            (junk)     = normally RCX, but RCX is clobbered by EENTER
 *            RAX        = system call number, passed from user-space
 *            R8         = ARG4, passed from user-space
 *            R9         = ARG5, passed from user-space
 *            R10        = ARG3, passed from user-space
 *     RSP -> R11        = user-space RFLAGS
 *
 * And the registers are setup as follows:
 *
 *            RDI        = ARG0
 *            RSI        = ARG1
 *            RDX        = ARG2
 *            RCX        = ARG3 (was stored on R10 on entry)
 *            R8         = ARG4
 *            R9         = ARG5
 *
 * NOTE: RCX, RBX, and RAX are clobbered by system calls. This is due to
 *       the ENCLS using EAX to store EENTER, RBX to store the TCS address,
 *       and RCX to store the AEP before transfering control to the kernel.
 *       User-level will observe different values of RAX, RBX, and RCX after
 *       EENTER than before.
 *
 * C Prototype:
 *       int enclave_start(void);
 */
ENTRY(enclave_start)
    /* XXX PGB
     * Enter from user-space - entry RSP/RBP are saved in the SSA by EENTER
     * Just need to switch to a trusted stack, building up %rsp to point to
     * the right place in the SSA given the info we have. The real trick is
     * that we can't actually read what's in the TCS on real hardware, the
     * code/data is between us and the SSA in the GT/KAIST TCS/code/SSA/Heap
     * layout. If each SSA area is placed next to it's SSA, on the other hand,
     * it would be easier to find the stack (and the heap!).
     *
     * Right now what we do assumes the TCS is readable, which it is in
     * simulator. As a result we can find the enclave base using the entry
     * offset in the TCS, and then the SSA from there. The other alternative
     * is to rely on linker symbols to find the end of the code thus the
     * start of the SSA areas. The problem there is knowning how many TCSes
     * there are...
     */
    /* First, find the base of the enclave */
    lea enclave_start(%rip), %rsp
    subq 32(%rbx), %rsp
    /* Now find the base of the SSA for this TCS */
    addq 16(%rbx), %rsp   /* Get to start of SSA */
    /* And then get to the end of the CSSA */
    shlq $(SSA_FRAME_SHIFT), %rax
    addq %rax, %rsp
    addq $(SSA_FRAME_SIZE), %rsp    /* Stack at end of curr ssa */

    addq $(STACK_FRAME_SIZE), %rsp

    /*
     * Save registers to trusted stack
     */

    subq $512, %rsp       /* Room on stack for FP state        */
    fxsave (%rsp)
    subq  $15*8, %rsp     /* Room on the stack for pt_regs     */
    movq %rcx, 10*8(%rsp) /* Save user-space RIP               */
                          /* RAX contained EENTER leaf         */
    movq %rdi,  8*8(%rsp) /* Save user-space RDI (ARG0)        */
    movq %rsi,  7*8(%rsp) /* Save user-space RSI (ARG1)        */
    movq %rdx,  6*8(%rsp) /* Save user-space RDX (ARG2)        */
                          /* RCX is clobbered                  */
                          /* Save user-space RAX (syscall #)   */
    movq %r8,   3*8(%rsp) /* Save user-space R8  (ARG4)        */
    movq %r9,   2*8(%rsp) /* Save user-space R9  (ARG5)        */
    movq %r10,  1*8(%rsp) /* Save user-space R10 (ARG3)        */
    movq %r11,     (%rsp) /* Save user-space RFLAGS            */

    /* Now determine where to go. If this is a regular TCS call,
     * CSSA will be zero and go to enclave_main. If this is an exception
     * call, go to enclave_exception with a pointer to the previous
     * SSA instead. This means that all recursing into the enclave
     * happens via enclave_exception, which needs to be careful of that. */

#if 0
    /* XXX PGB This won't work until emulator/runtime CSSA setup and
     * handling is actually right. It's wrong in the emulator because it
     * doesn't set cssa/etc. in the the destination, but even fixing it
     * in the CSSA passed in the TCS breaks things for reasons
     * I don't yet understand. */
    test %rax, %rax
    jz 1f
    /* The exception case - arg0 should be base of the previous CSSA    */
    lea 15*8(%rsp), %rdi
    sub $(PAGE_SIZE + SSA_FRAME_SIZE), %rdi
    call enclave_exception
    ja 2f               /* Return to user space, which should*/
                        /* eresume the previous entry        */
#endif
    1:
    /*
     * Call the enclave call handler
     */
    movq %r10, %rcx         /* Per x86_64 C ABI, RCX holds ARG3  */
    call enclave_main       /* Call the main enclave entry point */
    2:
    /*
     * Return to user-space - stack needs to be on the entry f
     */
    /* Wipe x64 registers that won't be restored */
    movq         $0, %r12
    movq         $0, %r13
    movq         $0, %r14
    movq         $0, %r15

    /* Restore callee-save registers from stack frame */
    movq     (%rsp), %r11       /* Restore RFLAGS for SYSRET         */
    movq  1*8(%rsp), %r10       /* Restore user-space R10 (ARG3)     */
    movq  2*8(%rsp), %r9        /* Restore user-space R9  (ARG5)     */
    movq  3*8(%rsp), %r8        /* Restore user-space R8  (ARG4)     */
                                /* RAX will contain EEXIT on exit    */
                                /* RCX will contain TCS.AEP on exit  */
    movq  6*8(%rsp), %rdx       /* Restore user-space RDX (ARG2)     */
    movq  7*8(%rsp), %rsi       /* Restore user-space RSI (ARG1)     */
    movq  8*8(%rsp), %rdi       /* Restore user-space RDI (ARG0)     */

    addq  $15*8, %rsp
    fxrstor (%rsp)              /* Floating point state              */

    movl  $0x04, %eax           /* EEXIT                             */
    movq  $0x0, %rbx            /* Should be 10*8(%rsp) before addq  */
                                /* for fp restore, but we're using   */
                                /* OpenSGX eexit semantics for now   */
    .byte 0x0F                  /* ENCLU                             */
    .byte 0x01
    .byte 0xd7
    retq
END(enclave_start)
