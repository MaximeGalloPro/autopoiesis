#!/usr/bin/env bash
set -euo pipefail

# Lance l'instance Dieu pour une demande déjà approuvée.
ROOT="$(git rev-parse --show-toplevel)"
exec "$ROOT/scripts/god-agent.sh" "$@"
