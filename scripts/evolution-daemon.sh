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

# Une approbation humaine est prioritaire sur les demandes encore pending.
priority_approved_id="$(python3 - "$DATA_DIR/approved_feature_requests.jsonl" "$DATA_DIR/evolution_runs" "$DATA_DIR/evolution-session-started" <<'PY'
import json
import pathlib
import sys

approved_path = pathlib.Path(sys.argv[1])
runs_dir = pathlib.Path(sys.argv[2])
session_path = pathlib.Path(sys.argv[3])
session_started = session_path.stat().st_mtime if session_path.exists() else 0
if not approved_path.exists():
    raise SystemExit(0)
for line in reversed(approved_path.read_text(encoding="utf-8").splitlines()):
    if not line.strip():
        continue
    try:
        request = json.loads(line)
    except json.JSONDecodeError:
        continue
    if not isinstance(request, dict):
        continue
    request_id = request.get("id")
    run_dir = runs_dir / request_id if request_id else runs_dir / "__invalid__"
    failed = run_dir / "god-failed"
    retryable_failed = failed.exists() and failed.stat().st_mtime < session_started
    if request_id and not (run_dir / "worktree.path").exists() and (not failed.exists() or retryable_failed):
        print(request_id)
        break
PY
)"
if [[ -n "$priority_approved_id" ]]; then
  "$ROOT/scripts/codex-feature-hook.sh" "$priority_approved_id"
  exit 0
fi

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
def read_jsonl(path):
    items = []
    if not path.exists():
        return items
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        try:
            item = json.loads(line)
            if isinstance(item, dict):
                items.append(item)
        except json.JSONDecodeError:
            continue
    return items
approved = {item.get("id") for item in read_jsonl(approved_path)}
rejected = {item.get("id") for item in read_jsonl(rejected_path)}
for request in read_jsonl(requests_path):
    request_id = request.get("id")
    if request.get("status") == "pending" and request_id and request_id not in approved and request_id not in rejected and not (runs_dir / request_id / "validation-record.json").exists():
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

approved_id="$(python3 - "$DATA_DIR/approved_feature_requests.jsonl" "$DATA_DIR/evolution_runs" "$DATA_DIR/evolution-session-started" <<'PY'
import json
import pathlib
import sys

approved_path = pathlib.Path(sys.argv[1])
runs_dir = pathlib.Path(sys.argv[2])
session_path = pathlib.Path(sys.argv[3])
session_started = session_path.stat().st_mtime if session_path.exists() else 0
if not approved_path.exists():
    raise SystemExit(0)
for line in approved_path.read_text(encoding="utf-8").splitlines():
    if not line.strip():
        continue
    try:
        request_id = json.loads(line).get("id")
    except json.JSONDecodeError:
        continue
    run_dir = runs_dir / request_id
    failed = run_dir / "god-failed"
    retryable_failed = failed.exists() and failed.stat().st_mtime < session_started
    if request_id and not (run_dir / "worktree.path").exists() and (not failed.exists() or retryable_failed):
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
