#!/usr/bin/env python3
"""Write a safe, idempotent changelog entry for one God evolution run."""

import datetime
import json
import pathlib
import sys


def load_json(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def markdown_value(value: object) -> str:
    return str(value or "").replace("\n", " ").strip()


def build_entry(run_dir: pathlib.Path) -> str:
    request = load_json(run_dir / "approved-request.json")
    request_id = markdown_value(request.get("id"))
    title = markdown_value(request.get("title")) or "Evolution sans titre"
    mechanism = request.get("mechanism") or {}
    changed_path = run_dir / "changed-files.txt"
    changed = [line.strip() for line in changed_path.read_text(encoding="utf-8").splitlines() if line.strip()]

    status = "implementation_proposed"
    status_label = "implementation proposée, vérification en attente"
    verification_path = run_dir / "verification.json"
    if verification_path.exists():
        verification = load_json(verification_path)
        status = markdown_value(verification.get("status")) or "unknown"
        status_label = {
            "verified": "verified, prête pour revue finale",
            "rejected": "rejected, vérification échouée",
        }.get(status, status)

    files = "\n".join(f"  - `{path}`" for path in changed) or "  - Aucun fichier modifié"
    timestamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    return f"""<!-- evolution:{request_id} -->
## {request_id} : {title}

- Date de mise à jour : `{timestamp}`
- Statut : **{status_label}** (`{status}`)
- Besoin : {markdown_value(request.get("need"))}
- Obstacle : {markdown_value(request.get("obstacle"))}
- Mécanisme : **{markdown_value(mechanism.get("name"))}**
- Résumé : {markdown_value(mechanism.get("summary"))}
- Fichiers modifiés :
{files}
- Rapport de Dieu : `evolution_runs/{request_id}/god-result.txt`
- Vérification détaillée : `evolution_runs/{request_id}/verification.json`

"""


def update_index(index_path: pathlib.Path, request_id: str, entry: str) -> None:
    index_path.parent.mkdir(parents=True, exist_ok=True)
    existing = index_path.read_text(encoding="utf-8") if index_path.exists() else ""
    marker = f"<!-- evolution:{request_id} -->"
    if marker in existing:
        before, remainder = existing.split(marker, 1)
        next_entry = remainder.find("<!-- evolution:")
        suffix = remainder[next_entry:] if next_entry >= 0 else ""
        content = before + entry + suffix
    else:
        header = "# Changelog de Dieu\n\nLes entrées sont générées par le workflow d'évolution. Une entrée `verified` reste soumise à la revue finale avant activation.\n\n"
        content = header + entry + existing
    index_path.write_text(content, encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: write-god-changelog.py <run-directory> <changelog-path>", file=sys.stderr)
        return 2
    run_dir = pathlib.Path(sys.argv[1])
    index_path = pathlib.Path(sys.argv[2])
    request = load_json(run_dir / "approved-request.json")
    request_id = markdown_value(request.get("id"))
    entry = build_entry(run_dir)
    (run_dir / "god-changelog.md").write_text(entry, encoding="utf-8")
    update_index(index_path, request_id, entry)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
