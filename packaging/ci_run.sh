#!/usr/bin/env bash
# Runs a CI command; if it fails, the last 40 output lines are posted as one GitHub error annotation,
# so the reason shows up on the run's summary page without opening the full log.
#   ci_run.sh <command> [args...]
log="$(mktemp)"
"$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
if [ "$status" -ne 0 ]; then
    msg="$(grep -v '^\s*$' "$log" | tail -n 40 | sed -e 's/%/%25/g' | awk 'BEGIN{ORS="%0A"} {print}')"
    echo "::error title=$1 failed (exit $status)::$msg"
fi
exit "$status"
