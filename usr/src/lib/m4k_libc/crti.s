/*
 * M4KK1 C Runtime Initialization
 * crti.s - Constructor initialization
 */

.section .init, "ax"
.globl _init
.type _init, @function
_init:
    pushl %ebp
    movl %esp, %ebp
    /* Constructor calls will be inserted here by linker */

.section .fini, "ax"
.globl _fini
.type _fini, @function
_fini:
    pushl %ebp
    movl %esp, %ebp
    /* Destructor calls will be inserted here by linker */
