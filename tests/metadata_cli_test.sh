#!/usr/bin/env bash

# CLI tests for --info, --extract-meta, and --extract-meta-value using tests/fixtures/metadata.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APEX_BIN="${APEX_BIN:-$ROOT/build/apex}"
FIXTURES="$ROOT/tests/fixtures/metadata"
YAML="$FIXTURES/yaml-frontmatter.md"
MMD="$FIXTURES/mmd-metadata.md"
PANDOC="$FIXTURES/pandoc-meta.md"

if [[ ! -x "$APEX_BIN" ]]; then
	echo "Error: $APEX_BIN not found or not executable. Build Apex first (cmake --build build --target apex_cli)." >&2
	exit 1
fi

for f in "$YAML" "$MMD" "$PANDOC"; do
	if [[ ! -f "$f" ]]; then
		echo "Error: missing fixture $f" >&2
		exit 1
	fi
done

TMPDIR="${TMPDIR:-/tmp}"
WORK="$(mktemp -d "${TMPDIR}/apex-metadata-cli-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

echo "== Testing -i / --info (no input files; info on stdout) =="

INFO_OUT="$WORK/info_stdout.txt"
# Close stdin so the CLI does not wait for interactive input in some environments.
"$APEX_BIN" -i </dev/null >"$INFO_OUT"
grep -q '^version:' "$INFO_OUT"
grep -q 'plugins:' "$INFO_OUT"
echo "-i without files: stdout contains version and plugins section."

echo
echo "== Testing -i with a file (info on stderr, HTML on stdout) =="

STDOUT="$WORK/out.html"
STDERR="$WORK/info.err"
"$APEX_BIN" -i "$YAML" >"$STDOUT" 2>"$STDERR"
grep -q '^version:' "$STDERR" || {
	echo "metadata_cli_test: expected version line on stderr when using -i with a file" >&2
	exit 1
}
grep -q '<p>' "$STDOUT" || {
	echo "metadata_cli_test: expected HTML fragment on stdout with -i and a file" >&2
	exit 1
}
echo "-i with file: version/config on stderr, conversion on stdout."

echo
echo "== Testing --extract-meta (YAML fixture) =="

META_OUT="$WORK/meta.yaml"
"$APEX_BIN" --extract-meta "$YAML" >"$META_OUT"
grep -q '^title: A YAML test$' "$META_OUT" || {
	echo "metadata_cli_test: --extract-meta missing expected title from YAML fixture" >&2
	exit 1
}
grep -q '^random_key: Flargle$' "$META_OUT" || {
	echo "metadata_cli_test: --extract-meta missing random_key from YAML fixture" >&2
	exit 1
}
echo "--extract-meta on YAML front matter: expected keys present."

echo
echo "== Testing -e / --extract-meta-value =="

val="$("$APEX_BIN" -e title "$YAML")"
if [[ "$val" != "A YAML test" ]]; then
	echo "metadata_cli_test: -e title from YAML expected 'A YAML test', got '$val'" >&2
	exit 1
fi

val="$("$APEX_BIN" --extract-meta-value random_key "$YAML")"
if [[ "$val" != "Flargle" ]]; then
	echo "metadata_cli_test: -e random_key expected 'Flargle', got '$val'" >&2
	exit 1
fi

val="$("$APEX_BIN" -e randomkey "$MMD")"
if [[ "$val" != "Bargle" ]]; then
	echo "metadata_cli_test: -e randomkey (MMD) expected 'Bargle', got '$val'" >&2
	exit 1
fi

val="$("$APEX_BIN" -e title "$PANDOC")"
if [[ "$val" != "Pandoc Metadata" ]]; then
	echo "metadata_cli_test: -e title (Pandoc) expected 'Pandoc Metadata', got '$val'" >&2
	exit 1
fi

if "$APEX_BIN" -e definitely_missing_key_xyz "$YAML" 2>"$WORK/miss.err"; then
	echo "metadata_cli_test: expected non-zero exit for missing metadata key" >&2
	exit 1
fi
grep -q "not found" "$WORK/miss.err" || {
	echo "metadata_cli_test: expected error message for missing key on stderr" >&2
	exit 1
}
echo "-e returns values and exit 1 when key is absent."

echo
echo "== Testing --combine --extract-meta (merge order, later file wins) =="

COMBINED="$WORK/combined_meta.yaml"
"$APEX_BIN" --combine --extract-meta "$MMD" "$YAML" >"$COMBINED"
grep -q '^title: A YAML test$' "$COMBINED" || {
	echo "metadata_cli_test: combined extract expected last file to win for title" >&2
	exit 1
}
echo "--combine --extract-meta: later file overrides title as expected."

echo
echo "All metadata CLI tests passed."
