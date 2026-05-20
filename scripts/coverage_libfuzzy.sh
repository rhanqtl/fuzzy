#!/usr/bin/env bash
# Build with coverage, run all Catch2 test executables under build/test/, report line coverage for src/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-${ROOT}/build}"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DFUZZY_COVERAGE=ON
cmake --build "$BUILD" -j
if ctest --test-dir "$BUILD" --output-on-failure; then
  :
else
  echo "ctest failed; running test binaries directly (same pattern as CI without noexec)..." >&2
  shopt -s nullglob
  for exe in "$BUILD"/test/* "$BUILD"/test/planner/*; do
    [[ -f "$exe" && -x "$exe" ]] || continue
    "$exe" >/dev/null
  done
fi
exec gcovr -r "$ROOT/src" --object-directory "$BUILD" --fail-under-line 95 --print-summary "$@"
