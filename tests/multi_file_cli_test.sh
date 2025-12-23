#!/usr/bin/env bash

# Simple CLI-level sanity tests for --mmd-merge and --combine (GitBook SUMMARY.md)
# using the existing combine_summary fixtures.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APEX_BIN="$ROOT/build/apex"

if [[ ! -x "$APEX_BIN" ]]; then
	echo "Error: $APEX_BIN not found or not executable. Build Apex first (cmake --build build)." >&2
	exit 1
fi

FIXTURES="$ROOT/tests/fixtures/combine_summary"

TMPDIR="$(mktemp -d "${TMPDIR:-/tmp}/apex-multi-file-XXXXXX")"
trap 'rm -rf "$TMPDIR"' EXIT

echo "== Testing --mmd-merge with mmd_merge-style index =="

MMD_OUT="$TMPDIR/mmd_merge_output.md"
"$APEX_BIN" --mmd-merge "$FIXTURES/index.txt" >"$MMD_OUT"

# Expect Intro, Chapter 1, and Section 1.1 in order, with Section 1.1 headings shifted one level
grep -q '^# Intro' "$MMD_OUT" || {
	echo "mmd-merge: missing Intro heading"
	exit 1
}
grep -q '^# Chapter 1' "$MMD_OUT" || {
	echo "mmd-merge: missing Chapter 1 heading"
	exit 1
}
grep -q '^## Section 1\.1' "$MMD_OUT" || {
	echo "mmd-merge: Section 1.1 heading was not shifted to level 2"
	exit 1
}

echo "mmd-merge index test passed."

echo
echo "== Testing --combine with GitBook-style SUMMARY.md =="

COMBINE_OUT="$TMPDIR/summary_combine_output.md"
"$APEX_BIN" --combine "$FIXTURES/SUMMARY.md" >"$COMBINE_OUT"

grep -q '^# Intro' "$COMBINE_OUT" || {
	echo "combine: missing Intro heading from SUMMARY.md combine"
	exit 1
}
grep -q '^# Chapter 1' "$COMBINE_OUT" || {
	echo "combine: missing Chapter 1 heading from SUMMARY.md combine"
	exit 1
}
grep -q '^# Section 1\.1' "$COMBINE_OUT" || {
	echo "combine: missing Section 1.1 heading from SUMMARY.md combine"
	exit 1
}

echo "SUMMARY.md combine test passed."

echo
echo "All multi-file CLI tests passed."
