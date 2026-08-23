#!/bin/bash
# Run sync_upstream (extracted live from the real cron script) inside
# the rehearsal clone that already has origin/main fetched.
S=/mnt/c/Users/LYH/AppData/Local/hermes/scripts/m4kk1_cron.sh
cd /tmp/sync_rehearsal || exit 1
{
    echo '#!/bin/bash'
    echo 'REPO=/tmp/sync_rehearsal'
    echo 'LOGDIR=/mnt/f/M4KK1/logs'
    echo 'STATEDIR=$LOGDIR/state'
    echo 'ALERT_LOG=$LOGDIR/alerts.rehearsal'
    sed -n '17p'                      "$S"  # log()
    sed -n '/^ensure_ahead()/,/^}/p'  "$S"
    sed -n '/^push_retry()/,/^}/p'    "$S"
    sed -n '/^verify_recent_green()/,/^}/p' "$S"
    sed -n '/^sync_upstream()/,/^}$/p' "$S"
    echo 'sync_upstream'
} > /tmp/ri3.sh
bash /tmp/ri3.sh
echo "EXIT=$?"
cd /tmp/sync_bare
echo "main now:   $(git rev-parse --short main)"
echo "legacy now: $(git rev-parse --short legacy)"
echo "ahead now:  $(git rev-parse --short ahead)"
