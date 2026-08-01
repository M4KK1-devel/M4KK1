#	$Id: Makefile.in,v 1.9 2012/09/25 11:17:17 plunky Exp $
#
# Makefile.in for the cc driver part of pcc.
#

srcdir=.
top_srcdir=../..
top_builddir=../..
prefix = /usr/local
exec_prefix = /usr/local
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
libexecdir = ${exec_prefix}/libexec
includedir = ${prefix}/include
datarootdir = ${prefix}/share
mandir = ${datarootdir}/man
CC = x86_64-pc-linux-gnu-gcc
EXEEXT = 
TARGOS = m4kk1
TARGOSVER = 0
TARGMACH = i386
TARGET = i386-pc-m4kk1
VERSION = 1.2.0.DEVEL
PCCLIBDIR = $(libdir)/pcc/$(TARGET)/$(VERSION)/lib
PCCINCDIR = $(libdir)/pcc/$(TARGET)/$(VERSION)/include
CFLAGS = -g -O2  -Wall -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wsign-compare
CPPFLAGS =  -DLIBEXECDIR=\"$(libexecdir)/\" \
	 -D_BSD_SOURCE -DGCC_COMPAT -DPCC_DEBUG -DINCLUDEDIR=\"$(includedir)/\" \
	-DPCCINCDIR=\"$(PCCINCDIR)/\" -DPCCLIBDIR=\"$(PCCLIBDIR)/\" \
	-DTARGOS=\"$(TARGOS)\" -DTARGOSVER=$(TARGOSVER) -Dos_$(TARGOS) \
	-DTARGMACH=\"$(TARGMACH)\" -Dmach_$(TARGMACH) \
	-I$(top_builddir) -I$(top_srcdir)/os/$(TARGOS) -I$(MIPDIR) -I$(MDIR)
LIBS = 
LDFLAGS = 
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} -s
INSTALL_DATA = ${INSTALL} -m 644

OBJS=driver.o platform.o strlist.o xalloc.o
DEST=i386-pc-m4kk1-pcc$(EXEEXT)

MIPDIR=$(top_srcdir)/mip
MDIR=$(top_srcdir)/arch/$(TARGMACH)

all: $(DEST)

$(DEST): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

install:
	test -z "$(DESTDIR)$(bindir)" || mkdir -p "$(DESTDIR)$(bindir)"
	$(INSTALL_PROGRAM) $(DEST) $(DESTDIR)$(bindir)

clean:
	rm -f  $(OBJS) $(DEST)

distclean: clean
	rm -f  Makefile
