#!/usr/bin/env bash
# Document metadata is printed for -t terminal. Config and --meta are not.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APEX_BIN="${APEX_BIN:-$ROOT/build/apex}"

if [[ ! -x "$APEX_BIN" ]]; then
	echo "Error: $APEX_BIN not found. Build with: cmake --build build --target apex_cli" >&2
	exit 1
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/apex-term-meta-cli-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/xdg/apex"
cat > "$WORK/xdg/apex/config.yml" <<'EOF'
cfgkey: from-config
EOF

cat > "$WORK/doc.md" <<'EOF'
---
title: Hello
---

See [%cfgkey] and [%extra].
EOF

OUT="$WORK/out.txt"
(
	cd "$WORK"
	XDG_CONFIG_HOME="$WORK/xdg" "$APEX_BIN" -t terminal --meta extra=from-meta "$WORK/doc.md" >"$OUT"
)

# Dim styling inserts SGR codes between "title:" and " Hello".
sed -E $'s/\033\\[[0-9;]*m//g' "$OUT" >"$WORK/plain.txt"
mv "$WORK/plain.txt" "$OUT"

grep -q 'title: Hello' "$OUT" || { echo "missing document key"; exit 1; }
grep -q 'from-config' "$OUT" || { echo "config value was not substituted"; exit 1; }
grep -q 'from-meta' "$OUT" || { echo "--meta value was not substituted"; exit 1; }
if grep -q 'cfgkey:' "$OUT"; then echo "config key was printed in the header"; exit 1; fi
if grep -q 'extra:' "$OUT"; then echo "--meta key was printed in the header"; exit 1; fi

# A config mode key replaces apex_options. The header must stay document-only.
mkdir -p "$WORK/xdg-mode/apex"
cat > "$WORK/xdg-mode/apex/config.yml" <<'EOF'
mode: unified
cfgkey: from-config
EOF

cat > "$WORK/doc-mode.md" <<'EOF'
---
title: Hello
---

See [%cfgkey].
EOF

OUT_MODE="$WORK/out-mode.txt"
(
	cd "$WORK"
	XDG_CONFIG_HOME="$WORK/xdg-mode" "$APEX_BIN" -t terminal "$WORK/doc-mode.md" >"$OUT_MODE"
)

sed -E $'s/\033\\[[0-9;]*m//g' "$OUT_MODE" >"$WORK/plain-mode.txt"
mv "$WORK/plain-mode.txt" "$OUT_MODE"

grep -q 'title: Hello' "$OUT_MODE" || { echo "missing document key after mode reset"; exit 1; }
grep -q 'from-config' "$OUT_MODE" || { echo "config value was not substituted after mode reset"; exit 1; }
if grep -q 'cfgkey:' "$OUT_MODE"; then echo "config key was printed in the header after mode reset"; exit 1; fi

echo "terminal metadata CLI: document key shown, config and --meta substituted but not printed"
