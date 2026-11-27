#!/usr/bin/env bash
# Fetch the game's run log (and the ROM-import state) from a console over its
# file transfer service, for a console whose applications cannot see USB.
#
#   tools/ps4/fetch_log.sh CONSOLE_ADDRESS [OUT_FILE] [PORT]
#
# CONSOLE_ADDRESS is the console's network address; PORT defaults to 2121
# (the FTP service a jailbroken console runs).  The log is /data/eidos/EIDOS_LOG.txt;
# a log that does not end in an "exit (status N)" line is a crash or a hang.
set -euo pipefail
HOST=${1:?usage: fetch_log.sh CONSOLE_ADDRESS [OUT_FILE] [PORT]}
OUT=${2:-EIDOS_LOG.txt}
PORT=${3:-2121}
curl -fsS --max-time 30 "ftp://$HOST:$PORT/data/eidos/EIDOS_LOG.txt" -o "$OUT"
echo "wrote $OUT ($(wc -l < "$OUT") lines)"
tail -n 5 "$OUT"
