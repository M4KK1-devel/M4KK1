/*
 * M4KK1 C Runtime Begin
 * crtbegin.s - Constructor initialization support
 */

.section .ctors, "aw"
.globl __CTOR_LIST__
__CTOR_LIST__:
    .long -1

.section .dtors, "aw"
.globl __DTOR_LIST__
__DTOR_LIST__:
    .long -1

.section .init_array, "aw"
.section .fini_array, "aw"
