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
SESSION_OFFSET_FILE="$DATA_DIR/evolution-session-request-offset"
VALIDATOR_MODE="${VALIDATOR_MODE:-human}"
VALIDATOR_MAX_REFORMULATIONS="${VALIDATOR_MAX_REFORMULATIONS:-3}"
GOD_MAX_CORRECTIONS="${GOD_MAX_CORRECTIONS:-2}"
log() {
  printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"
}
run_stage() {
  local label="$1"
  local request_id="$2"
  shift 2
  log "$label demarre | demande=$request_id"
  if "$@"; then
    log "$label termine | demande=$request_id"
    return 0
  else
    local status=$?
    log "$label en echec | demande=$request_id | code=$status"
    return "$status"
  fi
}
if [[ "$VALIDATOR_MODE" != "human" && "$VALIDATOR_MODE" != "codex" ]]; then
  echo "VALIDATOR_MODE doit valoir human ou codex" >&2
  exit 2
fi
if ! [[ "$VALIDATOR_MAX_REFORMULATIONS" =~ ^[0-9]+$ ]]; then
  echo "VALIDATOR_MAX_REFORMULATIONS doit etre un entier positif ou nul" >&2
  exit 2
fi
if ! [[ "$GOD_MAX_CORRECTIONS" =~ ^[0-9]+$ ]]; then
  echo "GOD_MAX_CORRECTIONS doit etre un entier positif ou nul" >&2
  exit 2
fi

mkdir -p "$DATA_DIR"
if ! mkdir "$LOCK_DIR" 2>/dev/null; then
  exit 0
fi
trap 'rmdir "$LOCK_DIR"' EXIT

if [[ ! -f "$SESSION_OFFSET_FILE" && -f "$DATA_DIR/evolution-session-started" ]]; then
  request_offset=0
  if [[ -f "$DATA_DIR/feature_requests.jsonl" ]]; then
    request_offset="$(wc -l < "$DATA_DIR/feature_requests.jsonl")"
  fi
  printf '%s\n' "$request_offset" > "$SESSION_OFFSET_FILE"
  log "File historique ignoree | lignes=$request_offset"
fi

"$ROOT/scripts/notify-evolution-events.sh" "$DATA_DIR"

python3 "$ROOT/scripts/reformulate-feature.py" "$DATA_DIR" "$VALIDATOR_MAX_REFORMULATIONS" "$SESSION_OFFSET_FILE"

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
  run_stage "Dieu" "$priority_approved_id" "$ROOT/scripts/codex-feature-hook.sh" "$priority_approved_id"
  exit 0
fi

pending_id="$(python3 - "$DATA_DIR/feature_requests.jsonl" "$DATA_DIR/evolution_runs" "$DATA_DIR/approved_feature_requests.jsonl" "$DATA_DIR/rejected_feature_requests.jsonl" "$SESSION_OFFSET_FILE" <<'PY'
import json
import pathlib
import sys

requests_path = pathlib.Path(sys.argv[1])
runs_dir = pathlib.Path(sys.argv[2])
approved_path = pathlib.Path(sys.argv[3])
rejected_path = pathlib.Path(sys.argv[4])
offset_path = pathlib.Path(sys.argv[5])
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
try:
    offset = max(0, int(offset_path.read_text(encoding="utf-8").strip()))
except (FileNotFoundError, ValueError):
    offset = 0
current_lines = requests_path.read_text(encoding="utf-8").splitlines()[offset:]
for line in current_lines:
    try:
        request = json.loads(line)
    except json.JSONDecodeError:
        continue
    if not isinstance(request, dict):
        continue
    request_id = request.get("id")
    if request.get("status") == "pending" and request_id and request_id not in approved and request_id not in rejected and not (runs_dir / request_id / "validation-record.json").exists():
        print(request_id)
        break
PY
)"
if [[ -n "$pending_id" ]]; then
  if [[ "$VALIDATOR_MODE" == "codex" ]]; then
    run_stage "Validator" "$pending_id" "$ROOT/scripts/validator-feature-hook.sh" "$pending_id"
  else
    log "Validation humaine attendue | demande=$pending_id"
  fi
  exit 0
fi

correction_id="$(python3 - "$DATA_DIR/evolution_runs" "$GOD_MAX_CORRECTIONS" "$DATA_DIR/evolution-session-started" <<'PY'
import json
import pathlib
import sys

runs_dir = pathlib.Path(sys.argv[1])
maximum = int(sys.argv[2])
session_path = pathlib.Path(sys.argv[3])
session_started = session_path.stat().st_mtime if session_path.exists() else 0
if not runs_dir.exists():
    raise SystemExit(0)
for run_dir in sorted(runs_dir.iterdir(), reverse=True):
    verification_path = run_dir / "verification.json"
    count_path = run_dir / "correction-count"
    if not verification_path.exists() or not count_path.exists():
        continue
    try:
        verification = json.loads(verification_path.read_text(encoding="utf-8"))
        count = int(count_path.read_text(encoding="utf-8").strip())
    except (ValueError, json.JSONDecodeError):
        continue
    correction_failed = run_dir / "god-correction-failed"
    failed_this_session = correction_failed.exists() and correction_failed.stat().st_mtime >= session_started
    if verification.get("status") == "rejected" and count < maximum and (run_dir / "worktree.path").exists() and not failed_this_session:
        print(run_dir.name)
        break
PY
)"
if [[ -n "$correction_id" ]]; then
  run_stage "Correction de Dieu" "$correction_id" "$ROOT/scripts/god-correction-agent.sh" "$correction_id"
  exit 0
fi

python3 - "$DATA_DIR/evolution_runs" "$GOD_MAX_CORRECTIONS" "$DATA_DIR/evolution-session-started" <<'PY'
import json
import pathlib
import sys

runs_dir = pathlib.Path(sys.argv[1])
maximum = int(sys.argv[2])
session_path = pathlib.Path(sys.argv[3])
session_started = session_path.stat().st_mtime if session_path.exists() else 0
if runs_dir.exists():
    for run_dir in runs_dir.iterdir():
        verification_path = run_dir / "verification.json"
        count_path = run_dir / "correction-count"
        if not verification_path.exists() or not count_path.exists():
            continue
        try:
            verification = json.loads(verification_path.read_text(encoding="utf-8"))
            count = int(count_path.read_text(encoding="utf-8").strip())
        except (ValueError, json.JSONDecodeError):
            continue
        if verification_path.stat().st_mtime >= session_started and verification.get("status") == "rejected" and count >= maximum:
            (run_dir / "corrections-exhausted").touch()
PY

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
  run_stage "Dieu" "$approved_id" "$ROOT/scripts/codex-feature-hook.sh" "$approved_id"
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
  run_stage "Verification" "$verification_id" "$ROOT/scripts/verify-evolution.sh" "$verification_id"
  exit 0
fi

activation_id="$(python3 - "$DATA_DIR/evolution_runs" "$DATA_DIR/evolution-session-started" <<'PY'
import json
import pathlib
import sys

runs_dir = pathlib.Path(sys.argv[1])
session_path = pathlib.Path(sys.argv[2])
session_started = session_path.stat().st_mtime if session_path.exists() else 0
if not runs_dir.exists():
    raise SystemExit(0)
for run_dir in sorted(runs_dir.iterdir(), reverse=True):
    verification_path = run_dir / "verification.json"
    activation_started = run_dir / "activation-started"
    if not verification_path.exists() or (run_dir / "activation.json").exists() or not (run_dir / "activation-eligible").exists():
        continue
    if activation_started.exists() and activation_started.stat().st_mtime >= session_started:
        continue
    try:
        verification = json.loads(verification_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        continue
    if verification.get("status") == "verified" and (run_dir / "worktree.path").exists():
        print(run_dir.name)
        break
PY
)"
if [[ -n "$activation_id" ]]; then
  run_stage "Activation" "$activation_id" "$ROOT/scripts/activate-evolution.sh" "$activation_id"
fi
