#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <feature-request-id>" >&2
  exit 2
fi

ROOT="$(git rev-parse --show-toplevel)"
if [[ -f "$ROOT/.env" ]]; then
  set -a
  source "$ROOT/.env"
  set +a
fi
REQUEST_ID="$1"
DATA_DIR="${DATA_DIR:-$ROOT/data}"
RUN_DIR="$DATA_DIR/evolution_runs/$REQUEST_ID"
CODEX_BIN="$($ROOT/scripts/find-codex.sh)"
CODEX_MODEL="${CODEX_VALIDATOR_MODEL:-${CODEX_MODEL:-gpt-5.6-sol}}"
CODEX_REASONING_EFFORT="${CODEX_VALIDATOR_REASONING_EFFORT:-${CODEX_REASONING_EFFORT:-low}}"
SCHEMA="$ROOT/scripts/schemas/validator-result.json"

mkdir -p "$RUN_DIR"
python3 - "$DATA_DIR/feature_requests.jsonl" "$REQUEST_ID" "$RUN_DIR" <<'PY'
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
request_id = sys.argv[2]
run_dir = pathlib.Path(sys.argv[3])
if not source.exists():
    raise SystemExit(f"Aucune demande dans {source}")

request = None
for line in source.read_text(encoding="utf-8").splitlines():
    if not line.strip():
        continue
    try:
        candidate = json.loads(line)
    except json.JSONDecodeError:
        continue
    if isinstance(candidate, dict) and candidate.get("id") == request_id:
        request = candidate
        break
if request is None:
    raise SystemExit(f"Demande inconnue: {request_id}")
if request.get("status") != "pending":
    raise SystemExit(f"La demande {request_id} n'est pas pending")

(run_dir / "request.json").write_text(json.dumps(request, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
prompt = f"""Tu es l'instance Validator d'Autopoiesis.

Tu travailles en lecture seule. Tu ne modifies aucun fichier, tu ne lances pas Dieu et tu ne changes aucun statut dans les journaux.
Lis AGENTS.md et normes/glossaire.md. Examine uniquement la demande ci-dessous.

Controle : contrat de demande complet, besoin comprehensible, mecanisme unique, perimetre minimal, preconditions deterministes, effets testables, compatibilite avec les invariants et criteres d'acceptation executables.
Decide `approve` uniquement si la demande est suffisamment precise pour etre implementee sans inventer de regle importante. Sinon utilise `reject` ou `reformulate`.
Une recommandation `approve` n'active rien et ne remplace pas l'autorisation prevue par la politique du projet.
Si tu choisis `reformulate`, fournis dans `reformulated_request` une demande complete et corrigee. Elle doit contenir un titre, un besoin, un obstacle, un changement propose, un mecanisme unique complet et des tests d'acceptation executables. Si tu choisis `approve` ou `reject`, utilise null.

Reponds exclusivement selon le schema fourni.

Demande :
{json.dumps(request, ensure_ascii=False, indent=2)}
"""
(run_dir / "validator-prompt.txt").write_text(prompt, encoding="utf-8")
PY

"$CODEX_BIN" --ask-for-approval never exec \
  --cd "$ROOT" \
  --model "$CODEX_MODEL" \
  -c "model_reasoning_effort=\"$CODEX_REASONING_EFFORT\"" \
  --sandbox read-only \
  --ephemeral \
  --output-schema "$SCHEMA" \
  --output-last-message "$RUN_DIR/validator-result.json" \
  - < "$RUN_DIR/validator-prompt.txt" \
  > "$RUN_DIR/validator.stdout.log" \
  2> "$RUN_DIR/validator.stderr.log"

python3 - "$RUN_DIR/validator-result.json" "$REQUEST_ID" "$RUN_DIR" <<'PY'
import json
import pathlib
import sys

result_path = pathlib.Path(sys.argv[1])
request_id = sys.argv[2]
run_dir = pathlib.Path(sys.argv[3])
result = json.loads(result_path.read_text(encoding="utf-8"))
if result.get("request_id") != request_id:
    raise SystemExit("La réponse Validator ne correspond pas à la demande")
if result.get("decision") not in {"approve", "reject", "reformulate"}:
    raise SystemExit("Décision Validator invalide")
if result.get("decision") == "reformulate" and not isinstance(result.get("reformulated_request"), dict):
    raise SystemExit("Une reformulation Validator doit fournir une demande complete")
record = {"request_id": request_id, "status": "validated", "recommendation": result}
(run_dir / "validation-record.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps(record, ensure_ascii=False))
PY
