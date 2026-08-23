#!/bin/bash
# Fix CRLF contamination in repo shell scripts (one-shot).
cd /mnt/f/M4KK1 || exit 1
fixed=0
while IFS= read -r f; do
    if grep -q $'\r' "$f" 2>/dev/null; then
        tr -d '\r' < "$f" > "$f.tmp" && mv "$f.tmp" "$f"
        echo "fixed: $f"
        fixed=$((fixed + 1))
    fi
done < <(find tools -name '*.sh' -type f)
echo "total fixed: $fixed"
rem=$(grep -rl $'\r' tools --include='*.sh' 2>/dev/null | wc -l)
echo "crlf remaining: $rem"
