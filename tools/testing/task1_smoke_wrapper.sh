#!/bin/bash
# task1 smoke runner: invoke canonical smoke script
set -u
cd /mnt/f/M4KK1 || exit 9
bash tools/testing/cron_task1_smoke.sh
