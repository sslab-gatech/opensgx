        .global _start
        .text
_start:
        mov     $1, %rax
        mov     $0, %rdi
        div     %rdi, %rax
