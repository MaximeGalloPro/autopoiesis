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
activation_failed() {
  date -u +%Y-%m-%dT%H:%M:%SZ > "$RUN_DIR/activation-failed"
}
trap activation_failed ERR

if [[ -f "$RUN_DIR/activation.json" ]]; then
  exit 0
fi
rm -f "$RUN_DIR/activation-failed"
if [[ ! -f "$RUN_DIR/activation-eligible" ]]; then
  echo "Cette execution precede le workflow d'activation actuel" >&2
  exit 1
fi
if [[ ! -s "$RUN_DIR/verification.json" ]] || [[ "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("status", ""))' "$RUN_DIR/verification.json")" != "verified" ]]; then
  echo "La demande $REQUEST_ID n'est pas verifiee" >&2
  exit 1
fi
if [[ ! -s "$WORKTREE_FILE" ]]; then
  echo "Aucun worktree de Dieu pour $REQUEST_ID" >&2
  exit 1
fi
WORKTREE="$(< "$WORKTREE_FILE")"
if [[ ! -d "$WORKTREE" ]]; then
  echo "Worktree introuvable: $WORKTREE" >&2
  exit 1
fi
if ! git -C "$ROOT" diff --quiet || ! git -C "$ROOT" diff --cached --quiet; then
  echo "Le depot principal doit etre propre avant activation" >&2
  exit 1
fi

mkdir -p "$RUN_DIR"
date -u +%Y-%m-%dT%H:%M:%SZ > "$RUN_DIR/activation-started"
git -C "$WORKTREE" add -A
if git -C "$WORKTREE" diff --cached --quiet; then
  echo "Aucune modification a activer" >&2
  exit 1
fi
git -C "$WORKTREE" commit -m "Activate evolution: $REQUEST_ID"
commit="$(git -C "$WORKTREE" rev-parse HEAD)"
git -C "$ROOT" merge --ff-only "$commit"
git -C "$ROOT" push origin main

python3 - "$RUN_DIR" "$commit" <<'PY'
import json
import pathlib
import sys

run_dir = pathlib.Path(sys.argv[1])
record = {"status": "activated", "commit": sys.argv[2], "branch": "main", "pushed": True}
(run_dir / "activation.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
PY
trap - ERR
date -u +%Y-%m-%dT%H:%M:%SZ > "$RUN_DIR/activation-finished"
python3 "$ROOT/scripts/write-god-changelog.py" "$RUN_DIR" "$DATA_DIR/god-changelog.md"
git -C "$ROOT" worktree remove --force "$WORKTREE"
