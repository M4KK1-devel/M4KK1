# M4KK1 4P1 - Sprach window manager Makefile
#
# Three layout modes are built from the same core (sprach.c) plus one
# mode file.  The default WM image (sprach_stack) is what Copland
# spawns; swap the mode by installing a different ELF as /bin/sprach.
#
#   make          -> sprach_stack, sprach_tiling, sprach_scroll
#   make clean    -> remove built artifacts

CC      = gcc
LD      = ld
ROOT    = ../../..
CFLAGS  = -ffreestanding -nostdlib -nostdinc -m32 -mno-sse -std=gnu99 \
          -fno-stack-protector -fno-pic -fno-pie -O2 -g
INCS    = -I$(ROOT)/sys/src/include -I$(ROOT)/include \
          -I$(ROOT)/sys/src/arch/m4kk1/include -I$(ROOT)/m4sh
LDFLAGS = -m elf_i386 -T sprach.ld -nostdlib -z max-page-size=0x1000

MODES = stack tiling scroll

all: $(MODES:%=sprach_%)

sprach_%: sprach.o sprach_mode_%.o
	$(LD) $(LDFLAGS) -o $@ sprach.o sprach_mode_$*.o

sprach.o: sprach.c sprach.h ../lib/libcopland.h
	$(CC) $(CFLAGS) $(INCS) -c sprach.c -o sprach.o

sprach_mode_%.o: sprach_mode_%.c sprach.h ../lib/libcopland.h
	$(CC) $(CFLAGS) $(INCS) -c sprach_mode_$*.c -o sprach_mode_$*.o

clean:
	rm -f sprach.o sprach_mode_*.o sprach_stack sprach_tiling sprach_scroll
