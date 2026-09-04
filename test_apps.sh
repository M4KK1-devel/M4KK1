#!/bin/bash
# M4KK1 4P1 - test_apps.sh
# Boot the latest ISO in QEMU and verify every desktop-suite app
# starts (serial banner evidence).  Usage:
#   wsl -d archlinux bash /mnt/f/M4KK1/test_apps.sh
set -u
cd "$(dirname "$0")"

ISO=$(ls -t output/m4kk1_*-full.iso | head -1)
echo "== test_apps: ISO=$ISO"

python3 tools/build/suite_probe.py
RC=$?

if [ $RC -eq 0 ]; then
    echo "test_apps: PASS (all suite apps verified)"
else
    echo "test_apps: FAIL (see probe output above)"
fi
exit $RC
