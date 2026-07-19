#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <data-directory>" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DATA_DIR="$1"
RUNS_DIR="$DATA_DIR/evolution_runs"
SESSION_FILE="$DATA_DIR/evolution-session-started"

if [[ ! -d "$RUNS_DIR" || ! -f "$SESSION_FILE" ]]; then
  exit 0
fi

notify_once() {
  local event="$1"
  local run_dir="$2"
  local evidence="$3"
  local sent="$run_dir/notification-$event-sent"
  if [[ -f "$evidence" && "$evidence" -nt "$SESSION_FILE" && ! -f "$sent" ]]; then
    "$ROOT/scripts/notify-macos.sh" "$event" "$(basename "$run_dir")"
    date -u +%Y-%m-%dT%H:%M:%SZ > "$sent"
  fi
}

for run_dir in "$RUNS_DIR"/*; do
  [[ -d "$run_dir" ]] || continue
  notify_once success "$run_dir" "$run_dir/activation.json"
  notify_once timeout "$run_dir" "$run_dir/ui-queue-timeout"
  notify_once timeout "$run_dir" "$run_dir/ui-work-timeout"
  for failure in god-failed god-correction-failed activation-failed corrections-exhausted; do
    if [[ -f "$run_dir/$failure" ]]; then
      notify_once failure "$run_dir" "$run_dir/$failure"
      break
    fi
  done
done
