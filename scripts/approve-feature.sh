#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <feature-request-id>" >&2
  exit 2
fi

DATA_DIR="${DATA_DIR:-data}"
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
    item = json.loads(line)
    if item.get("id") == request_id:
        found = item
        break
if found is None:
    raise SystemExit(f"Demande inconnue: {request_id}")

if approved.exists():
    for line in approved.read_text().splitlines():
        if line.strip() and json.loads(line).get("id") == request_id:
            print(f"Déjà approuvée: {request_id}")
            raise SystemExit(0)

found["status"] = "approved"
found["approved_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
approved.parent.mkdir(parents=True, exist_ok=True)
with approved.open("a") as stream:
    stream.write(json.dumps(found, ensure_ascii=False) + "\n")
print(f"Demande approuvée: {request_id}")
PY
