#!/usr/bin/env bash
set -euo pipefail

docker compose build

if [[ "${USE_API:-0}" == "1" ]]; then
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

if [[ -t 0 && -t 1 ]] && python3 scripts/evolution-ui.py --has-work; then
  exec python3 scripts/evolution-ui.py
fi
