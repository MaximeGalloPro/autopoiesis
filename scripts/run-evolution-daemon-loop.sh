#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DATA_DIR="${AUTOPOIESIS_DATA_DIR:-${DATA_DIR:-$ROOT/data}}"
LOG_FILE="$DATA_DIR/evolution-daemon.log"

mkdir -p "$DATA_DIR"
request_offset=0
if [[ -f "$DATA_DIR/feature_requests.jsonl" ]]; then
  request_offset="$(wc -l < "$DATA_DIR/feature_requests.jsonl")"
fi
printf '%s\n' "$request_offset" > "$DATA_DIR/evolution-session-request-offset"
touch "$DATA_DIR/evolution-session-started"

while true; do
  DATA_DIR="$DATA_DIR" "$ROOT/scripts/evolution-daemon.sh" >> "$LOG_FILE" 2>&1 || true
  sleep 2
done
