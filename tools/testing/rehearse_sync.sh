#!/bin/bash
# End-to-end rehearsal of sync_upstream against a throwaway clone.
# Verifies: ahead push, FF main gate, legacy tag move — without
# touching the real repo's remotes.
set -e
S=/mnt/c/Users/LYH/AppData/Local/hermes/scripts/m4kk1_cron.sh
RM=/tmp/sync_rehearsal

rm -rf "$RM" /tmp/sync_bare
git clone -q --bare /mnt/f/M4KK1 /tmp/sync_bare
git clone -q /tmp/sync_bare "$RM"
cd "$RM"
git checkout -q ahead
git config user.email t@t
git config user.name t
git remote remove origin

echo test > rehearsal.txt
git add rehearsal.txt
git commit -qm "rehearsal: test commit"

# Extract functions from the real script (line ranges from grep anchors)
{
    echo '#!/bin/bash'
    echo 'REPO=/tmp/sync_rehearsal'
    echo 'LOGDIR=/mnt/f/M4KK1/logs'
    echo 'STATEDIR=$LOGDIR/state'
    echo 'ALERT_LOG=$LOGDIR/alerts.rehearsal'
    sed -n '17p'                "$S"   # log()
    sed -n '42,57p'             "$S"   # ensure_ahead()
    sed -n '/^push_retry()/,/^}/p'          "$S"   # push_retry()
    sed -n '/^verify_recent_green()/,/^}/p' "$S"   # verify_recent_green()
    sed -n '/^sync_upstream()/,/^}$/p'      "$S"   # sync_upstream()
    echo 'git remote add origin /tmp/sync_bare'
    echo 'sync_upstream'
} > /tmp/rehearse_inner.sh
bash /tmp/rehearse_inner.sh

echo "---- verification ----"
cd /tmp/sync_bare
echo "origin/ahead after:  $(git rev-parse --short ahead)"
echo "origin/main  after:  $(git rev-parse --short main)"
echo "legacy tag   after:  $(git rev-parse --short legacy 2>/dev/null)"
