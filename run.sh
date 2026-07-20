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

ui_mode="${AUTOPOIESIS_UI:-web}"
simulation_args=()
for argument in "$@"; do
  case "$argument" in
    --terminal) ui_mode="terminal" ;;
    --web) ui_mode="web" ;;
    --gui)
      printf 'L option --gui est obsolete ; lancement de l interface web.\n' >&2
      ui_mode="web"
      ;;
    *) simulation_args+=("$argument") ;;
  esac
done
if [[ "$ui_mode" == "gui" ]]; then
  printf 'AUTOPOIESIS_UI=gui est obsolete ; utilisation de web.\n' >&2
  ui_mode="web"
fi
if [[ "$ui_mode" != "web" && "$ui_mode" != "terminal" ]]; then
  printf 'AUTOPOIESIS_UI doit valoir web ou terminal.\n' >&2
  exit 2
fi

use_api="${USE_API:-1}"
if [[ "$use_api" != "0" && "$use_api" != "1" ]]; then
  printf 'USE_API doit valoir 0 ou 1.\n' >&2
  exit 2
fi

for tool in docker; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'Outil requis introuvable : %s\n' "$tool" >&2
    exit 127
  fi
done
if [[ "$ui_mode" == "web" ]]; then
  for tool in cmake bun; do
    if ! command -v "$tool" >/dev/null 2>&1; then
      printf 'Outil requis pour le mode web introuvable : %s\n' "$tool" >&2
      exit 127
    fi
  done
fi

# Le build Docker est la garde externe commune au lancement normal et au mode
# terminal. Il compile le moteur, exécute ses tests et construit le client web.
docker compose build

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

mkdir -p "$ROOT/data"
if [[ "$ui_mode" == "terminal" && "$evolution_autostart" == "1" ]]; then
  AUTOPOIESIS_DATA_DIR="$ROOT/data" "$ROOT/scripts/run-evolution-daemon-loop.sh" &
  daemon_pid=$!
fi

if [[ "$ui_mode" == "web" ]]; then
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build" --target autopoiesis_backend -j2
  bun install --cwd "$ROOT/web" --frozen-lockfile
  bun run --cwd "$ROOT/web" typecheck
  bun run --cwd "$ROOT/web" build

  export AUTOPOIESIS_BACKEND_PATH="$ROOT/build/autopoiesis_backend"
  export AUTOPOIESIS_DATA_DIR="$ROOT/data"
  web_command=(bun "$ROOT/web/server/index.ts")
  if [[ ${#simulation_args[@]} -gt 0 ]]; then
    web_command+=("${simulation_args[@]}")
  fi
  printf 'Interface web disponible sur http://localhost:%s\n' "${PORT:-3000}"
  if "${web_command[@]}"; then
    simulation_status=0
  else
    simulation_status=$?
  fi
else
  terminal_command=(docker compose run --rm --entrypoint /app/build/autopoiesis autopoiesis)
  if [[ "$use_api" == "0" ]]; then
    terminal_command+=(--no-api)
  fi
  if [[ ${#simulation_args[@]} -gt 0 ]]; then
    terminal_command+=("${simulation_args[@]}")
  fi
  if "${terminal_command[@]}"; then
    simulation_status=0
  else
    simulation_status=$?
  fi
fi

if [[ "$simulation_status" -ne 0 ]]; then
  exit "$simulation_status"
fi

printf '\nAutopoiesis termine proprement.\n'
