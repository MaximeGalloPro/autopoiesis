#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <approved-feature-request-id>" >&2
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
WORKTREE="$ROOT/worktrees/god-$REQUEST_ID"
CODEX_BIN="${CODEX_BIN:-codex}"
CODEX_MODEL="${CODEX_GOD_MODEL:-${CODEX_MODEL:-gpt-5.6-sol}}"
CODEX_REASONING_EFFORT="${CODEX_GOD_REASONING_EFFORT:-${CODEX_REASONING_EFFORT:-low}}"

if [[ ! -s "$DATA_DIR/approved_feature_requests.jsonl" ]]; then
  echo "Aucune demande approuvee" >&2
  exit 1
fi
if ! git -C "$ROOT" diff --quiet || ! git -C "$ROOT" diff --cached --quiet; then
  echo "Le depot principal doit etre propre avant de creer le worktree de Dieu" >&2
  exit 1
fi
if [[ -e "$WORKTREE" ]]; then
  echo "Worktree deja present: $WORKTREE" >&2
  exit 1
fi

mkdir -p "$RUN_DIR" "$ROOT/worktrees"
python3 - "$DATA_DIR/approved_feature_requests.jsonl" "$REQUEST_ID" "$RUN_DIR" <<'PY'
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
request_id = sys.argv[2]
run_dir = pathlib.Path(sys.argv[3])
request = None
for line in source.read_text(encoding="utf-8").splitlines():
    if line.strip() and json.loads(line).get("id") == request_id:
        request = json.loads(line)
        break
if request is None:
    raise SystemExit(f"Demande approuvee inconnue: {request_id}")
if request.get("status") != "approved":
    raise SystemExit(f"La demande {request_id} n'est pas approved")

(run_dir / "approved-request.json").write_text(json.dumps(request, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
prompt = f"""Tu es l'instance Dieu d'Autopoiesis.

Tu es lance apres une approbation explicite. Travaille uniquement dans ce worktree et ne modifie aucun fichier hors depot.
Lis AGENTS.md et normes/glossaire.md. Implemente uniquement la demande fournie ci-dessous.

Regles imperatives :
- commence par ecrire un test qui echoue ;
- implemente le minimum necessaire pour le rendre vert ;
- conserve la separation entre decideur IA et moteur deterministe ;
- ne lis ni ne modifies .env, secrets, scripts d'orchestration ou journaux de donnees ;
- n'ajoute aucun mecanisme voisin, refactor ou amelioration opportuniste ;
- ne committe rien et n'active rien ;
- termine par un compte rendu concis des fichiers modifies et des tests executes.

Demande approuvee :
{json.dumps(request, ensure_ascii=False, indent=2)}
"""
(run_dir / "god-prompt.txt").write_text(prompt, encoding="utf-8")
PY

git -C "$ROOT" worktree add --detach "$WORKTREE" HEAD
"$CODEX_BIN" exec \
  --cd "$WORKTREE" \
  --model "$CODEX_MODEL" \
  -c "model_reasoning_effort=\"$CODEX_REASONING_EFFORT\"" \
  --sandbox workspace-write \
  --ask-for-approval never \
  --ephemeral \
  --output-last-message "$RUN_DIR/god-result.txt" \
  - < "$RUN_DIR/god-prompt.txt" \
  > "$RUN_DIR/god.stdout.log" \
  2> "$RUN_DIR/god.stderr.log"

git -C "$WORKTREE" add -N -- .
git -C "$WORKTREE" diff --binary > "$RUN_DIR/god.patch"
git -C "$WORKTREE" diff --name-only > "$RUN_DIR/changed-files.txt"
printf '%s\n' "$WORKTREE" > "$RUN_DIR/worktree.path"
