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
WORKTREE_FILE="$RUN_DIR/worktree.path"
CODEX_BIN="$($ROOT/scripts/find-codex.sh)"
CODEX_MODEL="${CODEX_GOD_MODEL:-${CODEX_MODEL:-gpt-5.6-sol}}"
CODEX_REASONING_EFFORT="${CODEX_GOD_REASONING_EFFORT:-${CODEX_REASONING_EFFORT:-low}}"

if [[ ! -s "$WORKTREE_FILE" ]]; then
  echo "Aucun worktree de Dieu pour $REQUEST_ID" >&2
  exit 1
fi
WORKTREE="$(< "$WORKTREE_FILE")"
if [[ ! -d "$WORKTREE" ]]; then
  echo "Worktree introuvable: $WORKTREE" >&2
  exit 1
fi
rm -f "$RUN_DIR/god-correction-failed"

attempt=0
if [[ -s "$RUN_DIR/correction-count" ]]; then
  attempt="$(< "$RUN_DIR/correction-count")"
fi
if ! [[ "$attempt" =~ ^[0-9]+$ ]]; then
  attempt=0
fi
attempt=$((attempt + 1))
printf '%s\n' "$attempt" > "$RUN_DIR/correction-count"
if [[ -f "$RUN_DIR/verification.json" ]]; then
  cp "$RUN_DIR/verification.json" "$RUN_DIR/verification-attempt-$attempt.json"
  rm -f "$RUN_DIR/verification.json" "$RUN_DIR/verification-started" \
    "$RUN_DIR/verification-failed" "$RUN_DIR/verification-finished"
fi

python3 - "$RUN_DIR" "$REQUEST_ID" "$attempt" <<'PY'
import json
import pathlib
import sys

run_dir = pathlib.Path(sys.argv[1])
request_id = sys.argv[2]
attempt = sys.argv[3]
verification = {}
previous = run_dir / f"verification-attempt-{attempt}.json"
if previous.exists():
    verification = json.loads(previous.read_text(encoding="utf-8"))
details = []
for name in ("verify-cmake.log", "verify-build.log", "verify-tests.log", "verify-docker.log"):
    path = run_dir / name
    if path.exists():
        lines = [line for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip()]
        if lines:
            details.append(f"{name}: {lines[-8:]}")
prompt = f"""Tu es Dieu, dans une boucle de correction TDD d'Autopoiesis.

La demande {request_id} a deja ete implementee dans ce worktree, mais le verificateur a signale un echec.
Corrige uniquement cette demande dans le worktree existant. Lis AGENTS.md et normes/glossaire.md.

Regles :
- identifie la cause a partir du diagnostic ci-dessous ;
- ajoute ou ajuste d'abord un test qui reproduit l'echec ;
- implemente la correction minimale ;
- execute les tests pertinents puis la suite ;
- ne modifie ni .env, ni les secrets, ni les scripts d'orchestration ;
- ne committe pas manuellement : l'orchestrateur committra uniquement apres verification ;
- termine par un compte rendu concis.

Resultat du verificateur :
{json.dumps(verification, ensure_ascii=False, indent=2)}

Extraits des logs :
{chr(10).join(details)}
"""
(run_dir / f"god-correction-{attempt}-prompt.txt").write_text(prompt, encoding="utf-8")
PY

set +e
"$CODEX_BIN" --ask-for-approval never exec \
  --cd "$WORKTREE" \
  --model "$CODEX_MODEL" \
  -c "model_reasoning_effort=\"$CODEX_REASONING_EFFORT\"" \
  --sandbox workspace-write \
  --ephemeral \
  --output-last-message "$RUN_DIR/god-correction-$attempt-result.txt" \
  - < "$RUN_DIR/god-correction-$attempt-prompt.txt" \
  > "$RUN_DIR/god-correction-$attempt.stdout.log" \
  2> "$RUN_DIR/god-correction-$attempt.stderr.log"
codex_status=$?
set -e
if [[ "$codex_status" -ne 0 ]]; then
  cp "$RUN_DIR/verification-attempt-$attempt.json" "$RUN_DIR/verification.json"
  date -u +%Y-%m-%dT%H:%M:%SZ > "$RUN_DIR/god-correction-failed"
  exit "$codex_status"
fi

cp "$RUN_DIR/god-correction-$attempt-result.txt" "$RUN_DIR/god-result.txt"
cp "$RUN_DIR/god-correction-$attempt.stdout.log" "$RUN_DIR/god.stdout.log"
cp "$RUN_DIR/god-correction-$attempt.stderr.log" "$RUN_DIR/god.stderr.log"
git -C "$WORKTREE" add -N -- .
git -C "$WORKTREE" diff --binary > "$RUN_DIR/god.patch"
git -C "$WORKTREE" diff --name-only > "$RUN_DIR/changed-files.txt"
