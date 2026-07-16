#!/usr/bin/env bash
set -euo pipefail

docker compose build

if [[ "${USE_API:-0}" == "1" ]]; then
  exec docker compose run --rm autopoiesis "$@"
fi

exec docker compose run --rm autopoiesis --no-api "$@"
