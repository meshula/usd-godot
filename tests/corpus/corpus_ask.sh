#!/usr/bin/env bash
#
# corpus_ask.sh <command...> — client for corpusd (used by Claude from the sandbox).
# Writes a tokened request and blocks until the daemon returns the matching response.
# Commands: ping | build | corpus | test <name.gd> | rebuild-run
#
# Env: CORPUS_TIMEOUT (seconds, default 240).

set -uo pipefail

CH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.corpusd"
mkdir -p "$CH"

TOKEN="t$(date +%s)-$$-${RANDOM:-0}"
CMD="$*"
[[ -z "$CMD" ]] && { echo "usage: corpus_ask.sh <ping|build|corpus|test NAME|rebuild-run>"; exit 2; }

# Publish request atomically so the daemon never reads a half-written token line.
{ printf '%s\n%s\n' "$TOKEN" "$CMD"; } > "$CH/req.tmp"
mv -f "$CH/req.tmp" "$CH/req"

TIMEOUT="${CORPUS_TIMEOUT:-240}"
deadline=$(( $(date +%s) + TIMEOUT ))
while [ "$(date +%s)" -lt "$deadline" ]; do
	if [[ -f "$CH/resp" && "$(head -1 "$CH/resp" 2>/dev/null)" == "$TOKEN" ]]; then
		tail -n +2 "$CH/resp"
		exit 0
	fi
	sleep 0.5
done
echo "corpus_ask: TIMEOUT after ${TIMEOUT}s — is corpusd.sh running in a host terminal?" >&2
exit 1
