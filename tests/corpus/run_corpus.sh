#!/usr/bin/env bash
#
# Run the usd-godot fidelity corpus (harvested from godot-usd) and print a scoreboard.
#
# Prerequisites:
#   - A Godot binary. Set $GODOT, or have `godot` on PATH.
#   - A built usd-godot extension (repo `lib/` + `godot-usd.gdextension`). Build it first
#     (see CMakeLists.txt / build.sh). This script stages those artifacts into the corpus
#     project because a .gdextension must live inside the project's res:// tree.
#
# Usage:
#   GODOT=/path/to/godot ./tests/corpus/run_corpus.sh
#
# Exit code mirrors the harness: 0 unless there is a REGRESSION (a MUST_PASS test that failed).

set -euo pipefail

CORPUS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$CORPUS_DIR/../.." && pwd)"

GODOT_BIN="${GODOT:-$(command -v godot || true)}"
if [[ -z "$GODOT_BIN" ]]; then
	echo "error: no Godot binary. Set \$GODOT or put 'godot' on PATH." >&2
	exit 2
fi

# --- Stage the built extension into the corpus project -----------------------
GDEXT_SRC="$REPO_ROOT/godot-usd.gdextension"
if [[ ! -f "$GDEXT_SRC" ]]; then
	echo "error: $GDEXT_SRC not found." >&2
	exit 2
fi

# Locate the built library directory (CMake installs to lib/, some flows use bin/).
LIB_SRC=""
for cand in "$REPO_ROOT/lib" "$REPO_ROOT/bin"; do
	if compgen -G "$cand/libgodot-usd.*" > /dev/null 2>&1 || compgen -G "$cand/godot-usd.*" > /dev/null 2>&1; then
		LIB_SRC="$cand"
		break
	fi
done
if [[ -z "$LIB_SRC" ]]; then
	echo "error: no built usd-godot library found under $REPO_ROOT/lib or $REPO_ROOT/bin." >&2
	echo "       Build the extension first (see CMakeLists.txt / build.sh)." >&2
	exit 2
fi

echo "staging extension: $GDEXT_SRC + $LIB_SRC/ -> $CORPUS_DIR"
cp "$GDEXT_SRC" "$CORPUS_DIR/godot-usd.gdextension"
mkdir -p "$CORPUS_DIR/lib"
# Copy the library plus any USD plugin resources (usdz/plugInfo etc.) the runtime needs.
rsync -a "$LIB_SRC"/ "$CORPUS_DIR/lib/"

# Godot loads GDExtensions listed in res://.godot/extension_list.cfg. That file is normally written
# by an editor project scan; since this corpus project is only ever run headless with --script, we
# author it directly so the extension actually loads (otherwise load() finds no USD loader and every
# test is a false XFAIL).
mkdir -p "$CORPUS_DIR/.godot"
echo "res://godot-usd.gdextension" > "$CORPUS_DIR/.godot/extension_list.cfg"

# --- Run the scoreboard harness ---------------------------------------------
echo "running corpus with: $GODOT_BIN"
exec "$GODOT_BIN" --headless --path "$CORPUS_DIR" --script res://tests/run_corpus.gd
