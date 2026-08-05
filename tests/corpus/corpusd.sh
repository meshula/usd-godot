#!/usr/bin/env bash
#
# corpusd — a tiny "tin cans and string" test daemon for the usd-godot fidelity corpus.
#
# WHY: the Godot editor binary crashes on startup inside Claude's sandbox (no graphics device), so
# Claude can build the extension but cannot RUN the corpus. This daemon runs on your machine (which
# has the graphics stack) and executes the tests Claude requests, bridging over two files on the
# shared filesystem — no socket required.
#
# USAGE: run this once in a normal terminal (NOT via Claude), leave it running:
#     ./tests/corpus/corpusd.sh
# Optional env: GODOT=/path/to/godot  USD_INSTALL_DIR=/path/to/usd/install
#
# PROTOCOL (files under <repo>/.corpusd/):
#   Claude writes  req : line1 = <token>, line2 = <command> [args...]
#   daemon writes  resp: line1 = <token>, remaining lines = command output (written atomically)
# Commands: ping | build | corpus | test <name.gd> | rebuild-run
#
# The daemon processes a request only when req's token changes, so it never re-runs a stale request.

set -uo pipefail

CORPUS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$CORPUS_DIR/../.." && pwd)"          # usd-godot/
GODOT_ROOT="$(cd "$REPO_ROOT/.." && pwd)"             # crucible/godot/
CH="$CORPUS_DIR/.corpusd"
mkdir -p "$CH"

USD_INSTALL_DIR="${USD_INSTALL_DIR:-/Users/nporcino/dev/crucible/Studiola/install}"
GODOT_BIN="${GODOT:-$GODOT_ROOT/godot-engine/bin/godot.macos.editor.arm64}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc)}"

export DYLD_FALLBACK_LIBRARY_PATH="$USD_INSTALL_DIR/lib:${DYLD_FALLBACK_LIBRARY_PATH:-}"
export PXR_PLUGINPATH_NAME="$USD_INSTALL_DIR/lib/usd:$USD_INSTALL_DIR/plugin/usd:${PXR_PLUGINPATH_NAME:-}"

log() { printf '[corpusd %s] %s\n' "$(date '+%H:%M:%S')" "$*"; }

# Stage the built extension into the corpus project (lib + gdextension + extension_list.cfg).
_stage() {
	local lib_src=""
	for cand in "$REPO_ROOT/lib" "$REPO_ROOT/bin"; do
		if compgen -G "$cand/libgodot-usd.*" >/dev/null 2>&1; then lib_src="$cand"; break; fi
	done
	[[ -z "$lib_src" ]] && { echo "stage: no built lib under $REPO_ROOT/{lib,bin}"; return 1; }
	cp "$REPO_ROOT/godot-usd.gdextension" "$CORPUS_DIR/godot-usd.gdextension" 2>/dev/null || true
	mkdir -p "$CORPUS_DIR/lib" "$CORPUS_DIR/.godot"
	rsync -a "$lib_src"/ "$CORPUS_DIR/lib/" >/dev/null 2>&1 || cp -f "$lib_src"/libgodot-usd.* "$CORPUS_DIR/lib/"
	echo "res://godot-usd.gdextension" > "$CORPUS_DIR/.godot/extension_list.cfg"
}

_build() {
	( cd "$REPO_ROOT" && cmake --build build -j"$JOBS" && cmake --install build --prefix . ) 2>&1
}

_corpus() { "$GODOT_BIN" --headless --path "$CORPUS_DIR" --script res://tests/run_corpus.gd 2>&1; }

_one_test() {  # <name.gd>
	local name="$1"
	[[ -z "$name" ]] && { echo "test: missing test name"; return 2; }
	"$GODOT_BIN" --headless --path "$CORPUS_DIR" --script "res://tests/$name" 2>&1
	echo "---exit=$?---"
}

run_command() {  # full command line
	local cmd="$1"; shift || true
	case "$cmd" in
		ping)         echo "pong  (godot=$GODOT_BIN, usd=$USD_INSTALL_DIR)";;
		build)        _build;;
		corpus)       _stage && _corpus;;
		test)         _stage && _one_test "$1";;
		rebuild-run)  _build && _stage && _corpus;;
		*)            echo "unknown command: '$cmd' (ping|build|corpus|test <name>|rebuild-run)";;
	esac
}

log "watching $CH/req  (godot=$GODOT_BIN)"
log "USD=$USD_INSTALL_DIR"
[[ -x "$GODOT_BIN" ]] || log "WARNING: godot binary not found/executable at $GODOT_BIN"

# Treat any request already present at startup as already-processed (don't re-run stale requests).
last_token=""
[[ -f "$CH/req" ]] && last_token="$(head -1 "$CH/req" 2>/dev/null)"

while true; do
	if [[ -f "$CH/req" ]]; then
		token="$(head -1 "$CH/req" 2>/dev/null)"
		if [[ -n "$token" && "$token" != "$last_token" ]]; then
			last_token="$token"
			cmdline="$(tail -n +2 "$CH/req" 2>/dev/null)"
			log "run: $cmdline"
			# shellcheck disable=SC2086
			output="$(run_command $cmdline)"
			# atomic response write
			{ printf '%s\n' "$token"; printf '%s\n' "$output"; } > "$CH/resp.tmp"
			mv -f "$CH/resp.tmp" "$CH/resp"
			log "done: $cmdline"
		fi
	fi
	sleep 0.5
done
