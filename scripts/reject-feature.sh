#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <feature-request-id> [reason]" >&2
  exit 2
fi

ROOT="$(git rev-parse --show-toplevel)"
DATA_DIR="${DATA_DIR:-$ROOT/data}"
REQUEST_ID="$1"
REASON="${2:-Refus explicite}"
python3 - "$DATA_DIR" "$REQUEST_ID" "$REASON" <<'PY'
import datetime, json, pathlib, sys

directory = pathlib.Path(sys.argv[1])
request_id = sys.argv[2]
reason = sys.argv[3]
pending = directory / "feature_requests.jsonl"
rejected = directory / "rejected_feature_requests.jsonl"
if not pending.exists():
    raise SystemExit(f"Aucune demande dans {pending}")

found = None
for line in pending.read_text(encoding="utf-8").splitlines():
    if line.strip() and json.loads(line).get("id") == request_id:
        found = json.loads(line)
        break
if found is None:
    raise SystemExit(f"Demande inconnue: {request_id}")

if rejected.exists():
    for line in rejected.read_text(encoding="utf-8").splitlines():
        if line.strip() and json.loads(line).get("id") == request_id:
            print(f"Déjà refusée: {request_id}")
            raise SystemExit(0)

found["status"] = "rejected"
found["rejected_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
found["rejection_mode"] = "human"
found["rejection_reason"] = reason
rejected.parent.mkdir(parents=True, exist_ok=True)
with rejected.open("a", encoding="utf-8") as stream:
    stream.write(json.dumps(found, ensure_ascii=False) + "\n")
run_dir = directory / "evolution_runs" / request_id
run_dir.mkdir(parents=True, exist_ok=True)
(run_dir / "human-decision.json").write_text(json.dumps({
    "request_id": request_id,
    "decision": "reject",
    "reason": reason,
}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"Demande refusée: {request_id}")
PY
