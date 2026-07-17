#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <feature-request-id>" >&2
  exit 2
fi

ROOT="$(git rev-parse --show-toplevel)"
REQUEST_ID="$1"
DATA_DIR="${DATA_DIR:-$ROOT/data}"
RUN_DIR="$DATA_DIR/evolution_runs/$REQUEST_ID"
WORKTREE_FILE="$RUN_DIR/worktree.path"
mkdir -p "$RUN_DIR"
date -u +%Y-%m-%dT%H:%M:%SZ > "$RUN_DIR/verification-started"

if [[ ! -s "$WORKTREE_FILE" ]]; then
  echo "Aucun worktree de Dieu pour $REQUEST_ID" >&2
  exit 1
fi
WORKTREE="$(< "$WORKTREE_FILE")"
if [[ ! -d "$WORKTREE" ]]; then
  echo "Worktree introuvable: $WORKTREE" >&2
  exit 1
fi

python3 "$ROOT/scripts/check-evolution-scope.py" "$WORKTREE" "$RUN_DIR/changed-files.txt"

cmake_status="failed"
tests_status="not_run"
docker_status="failed"
if cmake -S "$WORKTREE" -B "$WORKTREE/build" -DCMAKE_BUILD_TYPE=Release > "$RUN_DIR/verify-cmake.log" 2>&1 && \
   cmake --build "$WORKTREE/build" -j2 > "$RUN_DIR/verify-build.log" 2>&1; then
  cmake_status="passed"
  if ctest --test-dir "$WORKTREE/build" --output-on-failure > "$RUN_DIR/verify-tests.log" 2>&1; then
    tests_status="passed"
  else
    tests_status="failed"
  fi
fi
if docker compose -f "$WORKTREE/compose.yaml" build > "$RUN_DIR/verify-docker.log" 2>&1; then
  docker_status="passed"
fi

python3 - "$RUN_DIR" "$cmake_status" "$tests_status" "$docker_status" <<'PY'
import json
import pathlib
import sys

run_dir = pathlib.Path(sys.argv[1])
result = {
    "status": "verified" if sys.argv[2:] == ["passed", "passed", "passed"] else "rejected",
    "cmake": sys.argv[2],
    "tests": sys.argv[3],
    "docker": sys.argv[4],
}
(run_dir / "verification.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result))
PY
"$ROOT/scripts/write-god-changelog.py" "$RUN_DIR" "$DATA_DIR/god-changelog.md"
if [[ "$cmake_status" != "passed" || "$tests_status" != "passed" || "$docker_status" != "passed" ]]; then
  date -u +%Y-%m-%dT%H:%M:%SZ > "$RUN_DIR/verification-failed"
  exit 1
fi
date -u +%Y-%m-%dT%H:%M:%SZ > "$RUN_DIR/verification-finished"
