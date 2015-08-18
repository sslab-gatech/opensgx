#include <stdio.h>
#include <stdint.h>
void main()
{

    uint64_t ret = 0;


//  Test case for ax, eax, rax
    asm("rdrand %ax\n\t");
    asm("movq %%rax, %0\n\t"
        :"=a"(ret));   
    printf("%x\n",ret);

    asm("rdrand %eax\n\t");
    asm("movq %%rax, %0\n\t"
        :"=a"(ret));   
    printf("%x\n",ret);
 
    asm("rdrand %rax\n\t");
    asm("movq %%rax, %0\n\t"
        :"=a"(ret));   
    printf("%lx\n",ret);
    

    
/*  Test case for cx, ecx, rcx
    asm("rdrand %cx\n\t");
    asm("movq %%rcx, %0\n\t"
        :"=a"(ret));   
    printf("%x\n",ret);
    asm("rdrand %ecx\n\t");
    asm("movq %%rcx, %0\n\t"
        :"=a"(ret));   
    printf("%x\n",ret);
    asm("rdrand %rcx\n\t");
    asm("movq %%rcx, %0\n\t"
        :"=a"(ret));   
    printf("%lx\n",ret);
*/
/*  Test case for dx, edx, rdx
    asm("rdrand %dx\n\t");
    asm("rdrand %edx\n\t");
    asm("rdrand %rdx\n\t");
*/
}

