#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
if [[ -f "$ROOT/.env" ]]; then
  set -a
  source "$ROOT/.env"
  set +a
fi
if [[ "${VALIDATOR_MODE:-human}" == "human" ]]; then
  echo "Validator humain actif : utilisez ./scripts/approve-feature.sh <feature-request-id>" >&2
  exit 0
fi
if [[ "${VALIDATOR_MODE:-human}" != "codex" ]]; then
  echo "VALIDATOR_MODE doit valoir human ou codex" >&2
  exit 2
fi
exec "$ROOT/scripts/validator-agent.sh" "$@"
