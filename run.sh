#!/usr/bin/env bash
set -euo pipefail

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
