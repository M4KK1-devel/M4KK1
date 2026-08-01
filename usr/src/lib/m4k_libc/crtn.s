/*
 * M4KK1 C Runtime Finalization
 * crtn.s - Destructor finalization
 */

.section .init, "ax"
    /* End of constructor initialization */
    popl %ebp
    ret

.section .fini, "ax"
    /* End of destructor finalization */
    popl %ebp
    ret
