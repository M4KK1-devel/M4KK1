#!/bin/bash
# Host-side unit test driver for the hardened m4k_libc allocator.
# Compiles usr/src/lib/m4k_libc/stdlib.c natively (its own headers,
# unistd include stripped, _exit stubbed) plus heap_host_test.c
# (glibc headers), links with --allow-multiple-definition so the
# allocator's malloc/free win, runs it, expects RESULT: 12/12.
# Dev/QA tool — NOT shipped in the ISO.
set -e
cd "$(dirname "$0")/../.."
OUT=/tmp/m4k_heap_host_test
mkdir -p "$OUT"

# 1) allocator: own headers, drop the unistd include (host header
#    clash: ssize_t int vs long) and the errno extern (glibc's errno
#    is TLS; the sed rewrites writes to a plain host global we link
#    in).  Copy lives next to the original so its relative
#    "include/*.h" picks resolve.
sed -e 's|#include "include/unistd.h"|void _exit(int c);|' \
    -e 's|#include "include/errno.h"|#include <errno.h>|' \
    usr/src/lib/m4k_libc/stdlib.c > usr/src/lib/m4k_libc/stdlib_host.c
gcc -O0 -g -c \
    -o "$OUT/stdlib_host.o" usr/src/lib/m4k_libc/stdlib_host.c
rm -f usr/src/lib/m4k_libc/stdlib_host.c

# 2) stubs: _exit + errno storage
cat > "$OUT/stubs.c" <<'EOF'
void _exit(int c) { (void)c; while (1) __asm__("pause"); }
int m4k_host_errno;
EOF
gcc -O0 -g -c -o "$OUT/stubs.o" "$OUT/stubs.c"

# 3) test body (glibc headers)
gcc -O0 -g -c -o "$OUT/test.o" tools/testing/heap_host_test.c

# 4) link: allocator first so its malloc/free/calloc/realloc
#    override glibc's
gcc -o "$OUT/t" -Wl,--allow-multiple-definition \
    "$OUT/stdlib_host.o" "$OUT/stubs.o" "$OUT/test.o"

"$OUT/t"
