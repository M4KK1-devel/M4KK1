/*
 * M4KK1 C Runtime Startup Code
 * crt0.s - Program entry point
 */

.section .text
.globl _start
.type _start, @function

_start:
    /* Clear frame pointer for backtrace termination */
    xorl %ebp, %ebp
    
    /* Get argc and argv from stack */
    movl %esp, %eax
    movl (%eax), %ecx          /* argc */
    leal 4(%eax), %edx         /* argv */
    
    /* Align stack to 16 bytes (System V ABI requirement) */
    andl $0xFFFFFFF0, %esp
    
    /* Push padding to maintain alignment */
    pushl %esp
    
    /* Push argv and argc */
    pushl %edx
    pushl %ecx
    
    /* Call main */
    call main
    
    /* Exit with return value from main */
    movl %eax, %ebx
    movl $1, %eax
    int $0x80
    
    /* Should never reach here */
    hlt

.size _start, .-_start
