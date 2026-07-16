#!/usr/bin/env bash
set -euo pipefail

# Point d'entrée volontairement sans mutation : Codex ne traite que les demandes approuvées.
DATA_DIR="${DATA_DIR:-data}"
FILE="$DATA_DIR/approved_feature_requests.jsonl"
if [[ ! -s "$FILE" ]]; then
  exit 0
fi

echo "Demandes de fonctionnalités approuvées à traiter par Codex :"
python3 - "$FILE" <<'PY'
import json, sys
for line in open(sys.argv[1], encoding="utf-8"):
    if line.strip():
        item = json.loads(line)
        result = item.get("desired_result") or item.get("proposed_change") or "proposition non détaillée"
        print(f"- {item['id']}: {result} (besoin: {item.get('need', 'non précisé')})")
PY
