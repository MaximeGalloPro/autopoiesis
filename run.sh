#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

docker compose build

configured_use_api="${USE_API:-}"
if [[ -z "$configured_use_api" && -f .env ]]; then
  configured_use_api="$(awk -F= '$1 == "USE_API" {gsub(/[[:space:]]/, "", $2); print $2; exit}' .env)"
fi
use_api="${configured_use_api:-1}"
if [[ "$use_api" != "0" && "$use_api" != "1" ]]; then
  printf 'USE_API doit valoir 0 ou 1.\n' >&2
  exit 2
fi

configured_autostart="${EVOLUTION_AUTOSTART:-}"
if [[ -z "$configured_autostart" && -f "$ROOT/.env" ]]; then
  configured_autostart="$(awk -F= '$1 == "EVOLUTION_AUTOSTART" {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$ROOT/.env")"
fi
evolution_autostart="${configured_autostart:-1}"
if [[ "$evolution_autostart" != "0" && "$evolution_autostart" != "1" ]]; then
  printf 'EVOLUTION_AUTOSTART doit valoir 0 ou 1.\n' >&2
  exit 2
fi

daemon_pid=""
cleanup() {
  if [[ -n "$daemon_pid" ]] && kill -0 "$daemon_pid" 2>/dev/null; then
    kill "$daemon_pid" 2>/dev/null || true
    wait "$daemon_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT

if [[ "$evolution_autostart" == "1" ]]; then
  mkdir -p "$ROOT/data"
  touch "$ROOT/data/evolution-session-started"
  (
    while true; do
      "$ROOT/scripts/evolution-daemon.sh" >> "$ROOT/data/evolution-daemon.log" 2>&1 || true
      sleep 2
    done
  ) &
  daemon_pid=$!
fi

if [[ "$use_api" == "1" ]]; then
  if docker compose run --rm autopoiesis "$@"; then
    simulation_status=0
  else
    simulation_status=$?
  fi
else
  if docker compose run --rm autopoiesis --no-api "$@"; then
    simulation_status=0
  else
    simulation_status=$?
  fi
fi

if [[ "$simulation_status" -ne 0 ]]; then
  exit "$simulation_status"
fi

printf '\nSimulation terminee proprement.\n'
