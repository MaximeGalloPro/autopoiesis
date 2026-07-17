#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
if [[ -f "$ROOT/.env" ]]; then
  set -a
  source "$ROOT/.env"
  set +a
fi
DATA_DIR="${DATA_DIR:-$ROOT/data}"
LOCK_DIR="$DATA_DIR/evolution-daemon.lock"
VALIDATOR_MODE="${VALIDATOR_MODE:-human}"
VALIDATOR_MAX_REFORMULATIONS="${VALIDATOR_MAX_REFORMULATIONS:-3}"
if [[ "$VALIDATOR_MODE" != "human" && "$VALIDATOR_MODE" != "codex" ]]; then
  echo "VALIDATOR_MODE doit valoir human ou codex" >&2
  exit 2
fi
if ! [[ "$VALIDATOR_MAX_REFORMULATIONS" =~ ^[0-9]+$ ]]; then
  echo "VALIDATOR_MAX_REFORMULATIONS doit etre un entier positif ou nul" >&2
  exit 2
fi

mkdir -p "$DATA_DIR"
if ! mkdir "$LOCK_DIR" 2>/dev/null; then
  exit 0
fi
trap 'rmdir "$LOCK_DIR"' EXIT

python3 "$ROOT/scripts/reformulate-feature.py" "$DATA_DIR" "$VALIDATOR_MAX_REFORMULATIONS"

pending_id="$(python3 - "$DATA_DIR/feature_requests.jsonl" "$DATA_DIR/evolution_runs" "$DATA_DIR/approved_feature_requests.jsonl" "$DATA_DIR/rejected_feature_requests.jsonl" <<'PY'
import json
import pathlib
import sys

requests_path = pathlib.Path(sys.argv[1])
runs_dir = pathlib.Path(sys.argv[2])
approved_path = pathlib.Path(sys.argv[3])
rejected_path = pathlib.Path(sys.argv[4])
if not requests_path.exists():
    raise SystemExit(0)
approved = set()
rejected = set()
if approved_path.exists():
    approved = {json.loads(line)["id"] for line in approved_path.read_text(encoding="utf-8").splitlines() if line.strip()}
if rejected_path.exists():
    rejected = {json.loads(line)["id"] for line in rejected_path.read_text(encoding="utf-8").splitlines() if line.strip()}
for line in requests_path.read_text(encoding="utf-8").splitlines():
    if not line.strip():
        continue
    request = json.loads(line)
    request_id = request.get("id")
    if request.get("status") == "pending" and request_id not in approved and request_id not in rejected and not (runs_dir / request_id / "validation-record.json").exists():
        print(request_id)
        break
PY
)"
if [[ -n "$pending_id" ]]; then
  if [[ "$VALIDATOR_MODE" == "codex" ]]; then
    "$ROOT/scripts/validator-feature-hook.sh" "$pending_id"
  else
    echo "Demande $pending_id en attente de validation humaine" >&2
  fi
  exit 0
fi

approved_id="$(python3 - "$DATA_DIR/approved_feature_requests.jsonl" "$DATA_DIR/evolution_runs" <<'PY'
import json
import pathlib
import sys

approved_path = pathlib.Path(sys.argv[1])
runs_dir = pathlib.Path(sys.argv[2])
if not approved_path.exists():
    raise SystemExit(0)
for line in approved_path.read_text(encoding="utf-8").splitlines():
    if not line.strip():
        continue
    request_id = json.loads(line).get("id")
    run_dir = runs_dir / request_id
    if request_id and not (run_dir / "worktree.path").exists():
        print(request_id)
        break
PY
)"
if [[ -n "$approved_id" ]]; then
  "$ROOT/scripts/codex-feature-hook.sh" "$approved_id"
  exit 0
fi

verification_id="$(python3 - "$DATA_DIR/evolution_runs" <<'PY'
import pathlib
import sys

runs_dir = pathlib.Path(sys.argv[1])
if not runs_dir.exists():
    raise SystemExit(0)
for run_dir in sorted(runs_dir.iterdir()):
    if (run_dir / "worktree.path").exists() and (run_dir / "god-result.txt").exists() and not (run_dir / "verification.json").exists():
        print(run_dir.name)
        break
PY
)"
if [[ -n "$verification_id" ]]; then
  "$ROOT/scripts/verify-evolution.sh" "$verification_id"
fi
