#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

load_env_file() {
  local path="$1" key value
  [[ -f "$path" ]] || return 0
  while IFS='=' read -r key value; do
    key="${key#"${key%%[![:space:]]*}"}"
    key="${key%"${key##*[![:space:]]}"}"
    [[ "$key" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] || continue
    [[ -z "${!key+x}" ]] || continue
    value="${value#"${value%%[![:space:]]*}"}"
    value="${value%"${value##*[![:space:]]}"}"
    if [[ ${#value} -ge 2 && ( "${value:0:1}" == '"' && "${value: -1}" == '"' || "${value:0:1}" == "'" && "${value: -1}" == "'" ) ]]; then
      value="${value:1:${#value}-2}"
    fi
    printf -v "$key" '%s' "$value"
    export "$key"
  done < "$path"
}

load_env_file "$ROOT/.env"

ui_mode="${AUTOPOIESIS_UI:-gui}"
simulation_args=()
for argument in "$@"; do
  case "$argument" in
    --terminal) ui_mode="terminal" ;;
    --gui) ui_mode="gui" ;;
    *) simulation_args+=("$argument") ;;
  esac
done
if [[ "$ui_mode" != "gui" && "$ui_mode" != "terminal" ]]; then
  printf 'AUTOPOIESIS_UI doit valoir gui ou terminal.\n' >&2
  exit 2
fi

docker compose build

use_api="${USE_API:-1}"
if [[ "$use_api" != "0" && "$use_api" != "1" ]]; then
  printf 'USE_API doit valoir 0 ou 1.\n' >&2
  exit 2
fi

evolution_autostart="${EVOLUTION_AUTOSTART:-1}"
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
  request_offset=0
  if [[ -f "$ROOT/data/feature_requests.jsonl" ]]; then
    request_offset="$(wc -l < "$ROOT/data/feature_requests.jsonl")"
  fi
  printf '%s\n' "$request_offset" > "$ROOT/data/evolution-session-request-offset"
  touch "$ROOT/data/evolution-session-started"
  (
    while true; do
      "$ROOT/scripts/evolution-daemon.sh" >> "$ROOT/data/evolution-daemon.log" 2>&1 || true
      sleep 2
    done
  ) &
  daemon_pid=$!
fi

if [[ "$ui_mode" == "gui" ]]; then
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release -DAUTOPOIESIS_BUILD_GUI=ON
  cmake --build "$ROOT/build" --target autopoiesis_gui -j2
  export AUTOPOIESIS_DATA_DIR="$ROOT/data"
  simulation_command=("$ROOT/build/autopoiesis_gui")
  if [[ "$use_api" == "0" ]]; then
    simulation_command+=(--no-api)
  fi
  if [[ ${#simulation_args[@]} -gt 0 ]]; then
    simulation_command+=("${simulation_args[@]}")
  fi
  if "${simulation_command[@]}"; then
    simulation_status=0
  else
    simulation_status=$?
  fi
else
  if [[ "$use_api" == "1" ]]; then
    terminal_command=(docker compose run --rm autopoiesis)
    if [[ ${#simulation_args[@]} -gt 0 ]]; then
      terminal_command+=("${simulation_args[@]}")
    fi
    if "${terminal_command[@]}"; then
      simulation_status=0
    else
      simulation_status=$?
    fi
  else
    terminal_command=(docker compose run --rm autopoiesis --no-api)
    if [[ ${#simulation_args[@]} -gt 0 ]]; then
      terminal_command+=("${simulation_args[@]}")
    fi
    if "${terminal_command[@]}"; then
      simulation_status=0
    else
      simulation_status=$?
    fi
  fi
fi

if [[ "$simulation_status" -ne 0 ]]; then
  exit "$simulation_status"
fi

printf '\nSimulation terminee proprement.\n'
