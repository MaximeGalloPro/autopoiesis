#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CODEX_BIN:-}" ]]; then
  if [[ -x "$CODEX_BIN" ]]; then
    printf '%s\n' "$CODEX_BIN"
    exit 0
  fi
  echo "CODEX_BIN n'est pas executable: $CODEX_BIN" >&2
  exit 127
fi

if command -v codex >/dev/null 2>&1; then
  command -v codex
  exit 0
fi

for candidate in "$HOME"/.vscode/extensions/openai.chatgpt-*/bin/*/codex; do
  if [[ -x "$candidate" ]]; then
    printf '%s\n' "$candidate"
    exit 0
  fi
done

echo "Codex introuvable. Definissez CODEX_BIN dans .env." >&2
exit 127
