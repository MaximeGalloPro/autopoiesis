#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <feature-request-id>" >&2
  exit 2
fi

ROOT="$(git rev-parse --show-toplevel)"
DATA_DIR="${DATA_DIR:-$ROOT/data}"
REQUEST_ID="$1"
python3 - "$DATA_DIR" "$REQUEST_ID" <<'PY'
import datetime, json, pathlib, sys

directory = pathlib.Path(sys.argv[1])
request_id = sys.argv[2]
pending = directory / "feature_requests.jsonl"
approved = directory / "approved_feature_requests.jsonl"
if not pending.exists():
    raise SystemExit(f"Aucune demande dans {pending}")

found = None
for line in pending.read_text().splitlines():
    if not line.strip():
        continue
    try:
        item = json.loads(line)
    except json.JSONDecodeError:
        continue
    if not isinstance(item, dict):
        continue
    if item.get("id") == request_id:
        found = item
        break
if found is None:
    raise SystemExit(f"Demande inconnue: {request_id}")

if approved.exists():
    for line in approved.read_text().splitlines():
        if not line.strip():
            continue
        try:
            item = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(item, dict) and item.get("id") == request_id:
            print(f"Déjà approuvée: {request_id}")
            raise SystemExit(0)

found["status"] = "approved"
found["approved_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
found["approval_mode"] = "human"
approved.parent.mkdir(parents=True, exist_ok=True)
with approved.open("a") as stream:
    stream.write(json.dumps(found, ensure_ascii=False) + "\n")
run_dir = directory / "evolution_runs" / request_id
run_dir.mkdir(parents=True, exist_ok=True)
(run_dir / "validation-record.json").write_text(json.dumps({
    "request_id": request_id,
    "status": "validated",
    "recommendation": {
        "decision": "approve",
        "reviewer": "human",
        "reason": "Approbation explicite de l'utilisateur",
    },
}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"Demande approuvée: {request_id}")
PY
