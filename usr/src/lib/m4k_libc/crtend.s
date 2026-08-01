/*
 * M4KK1 C Runtime End
 * crtend.s - Destructor finalization support
 */

.section .ctors, "aw"
.globl __CTOR_END__
__CTOR_END__:
    .long 0

.section .dtors, "aw"
.globl __DTOR_END__
__DTOR_END__:
    .long 0
