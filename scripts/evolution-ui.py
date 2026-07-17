#!/usr/bin/env python3
"""Small dependency-free terminal interface for human evolution decisions."""

import argparse
import json
import pathlib
import subprocess
import sys
import time


def read_jsonl(path):
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def load_state(root):
    data = root / "data"
    requests = read_jsonl(data / "feature_requests.jsonl")
    approved = {item.get("id") for item in read_jsonl(data / "approved_feature_requests.jsonl")}
    rejected = {item.get("id") for item in read_jsonl(data / "rejected_feature_requests.jsonl")}
    actionable = []
    for request in requests:
        request_id = request.get("id")
        if request.get("status") != "pending" or request_id in approved or request_id in rejected:
            continue
        record_path = data / "evolution_runs" / request_id / "validation-record.json"
        record = json.loads(record_path.read_text(encoding="utf-8")) if record_path.exists() else None
        if record and record.get("status") in {"rejected", "reformulated"}:
            continue
        actionable.append((request, record))
    return actionable


def load_approved_evolutions(root):
    """Return approved requests whose God/verification run is not complete."""
    data = root / "data"
    pending = []
    for request in read_jsonl(data / "approved_feature_requests.jsonl"):
        request_id = request.get("id")
        if not request_id:
            continue
        run_dir = data / "evolution_runs" / request_id
        if (run_dir / "verification.json").exists():
            continue
        record_path = run_dir / "validation-record.json"
        record = json.loads(record_path.read_text(encoding="utf-8")) if record_path.exists() else None
        pending.append((request, record))
    return pending


def has_work(root):
    return bool(load_state(root) or load_approved_evolutions(root))


def recommendation(record):
    if not record:
        return "EN ATTENTE DU VALIDATOR"
    item = record.get("recommendation", {})
    return item.get("decision", "AVIS INCOMPLET").upper()


def print_request(request, record, index, total):
    print("\n" + "=" * 64)
    print(f"Demande {index}/{total} · {request.get('id')}")
    print("=" * 64)
    print(f"Titre       : {request.get('title', '(sans titre)')}")
    print(f"Personnage  : {request.get('agent_name', request.get('agent_id', '?'))}")
    print(f"Besoin      : {request.get('need', '')}")
    print(f"Obstacle    : {request.get('obstacle', '')}")
    print(f"Mécanisme   : {(request.get('mechanism') or {}).get('name', '')}")
    print(f"Validator   : {recommendation(record)}")
    if record:
        print(f"Avis        : {record.get('recommendation', {}).get('reason', '')}")


def show_detail(request):
    print("\nDemande complète :")
    print(json.dumps(request, ensure_ascii=False, indent=2))


def run_decision(root, request_id, action, reason=""):
    script = root / "scripts" / ("approve-feature.sh" if action == "approve" else "reject-feature.sh")
    command = [str(script), request_id]
    if action == "reject" and reason:
        command.append(reason)
    subprocess.run(command, cwd=root, check=True)


def evolution_snapshot(root, request_id):
    run_dir = root / "data" / "evolution_runs" / request_id
    worktree = root / "worktrees" / f"god-{request_id}"
    verification_path = run_dir / "verification.json"
    if verification_path.exists():
        return "verification", json.loads(verification_path.read_text(encoding="utf-8"))
    if (run_dir / "god-result.txt").exists():
        return "god-finished", None
    if worktree.exists() or (run_dir / "god-prompt.txt").exists():
        return "god-running", None
    if (root / "data" / "approved_feature_requests.jsonl").exists():
        approved = {item.get("id") for item in read_jsonl(root / "data" / "approved_feature_requests.jsonl")}
        if request_id in approved:
            return "approved", None
    return "waiting", None


def start_daemon(root):
    return subprocess.Popen(
        [str(root / "scripts" / "evolution-daemon.sh")],
        cwd=root,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def show_live_status(root, request_id, phase, elapsed):
    labels = {
        "waiting": "en attente du daemon",
        "approved": "demande approuvée, Dieu va démarrer",
        "god-running": "Dieu travaille dans son worktree isolé",
        "god-finished": "Dieu a terminé, vérification en préparation",
        "verification": "vérification terminée",
    }
    print(f"\r[{elapsed:>4}s] {labels.get(phase, phase)}" + " " * 20, end="", flush=True)


def tail(path, count=8):
    if not path.exists():
        return []
    return path.read_text(encoding="utf-8", errors="replace").splitlines()[-count:]


def display_run_summary(root, request_id, verification):
    run_dir = root / "data" / "evolution_runs" / request_id
    print("\n\n" + "=" * 64)
    print(f"Bilan de l'évolution · {request_id}")
    print("=" * 64)
    if verification:
        print(f"Statut     : {verification.get('status', 'inconnu')}")
        print(f"CMake      : {verification.get('cmake', 'inconnu')}")
        print(f"Tests      : {verification.get('tests', 'inconnu')}")
        print(f"Docker     : {verification.get('docker', 'inconnu')}")
    changed = run_dir / "changed-files.txt"
    if changed.exists():
        print("Fichiers   :")
        for path in changed.read_text(encoding="utf-8").splitlines():
            if path.strip():
                print(f"  - {path}")
    result_lines = tail(run_dir / "god-result.txt")
    if result_lines:
        print("\nDernier compte rendu de Dieu :")
        print("\n".join(result_lines))


def run_approved_evolution(root, request_id):
    """Drive God and then verification while keeping the terminal alive."""
    started = time.monotonic()
    last_phase = None
    god_process = None
    for _ in range(30):
        god_process = start_daemon(root)
        while god_process.poll() is None:
            phase, _ = evolution_snapshot(root, request_id)
            if phase != last_phase:
                print()
                last_phase = phase
            show_live_status(root, request_id, phase, int(time.monotonic() - started))
            time.sleep(2)
        if (root / "data" / "evolution_runs" / request_id / "god-result.txt").exists():
            break
        time.sleep(2)
    if not god_process or not (root / "data" / "evolution_runs" / request_id / "god-result.txt").exists():
        code = god_process.returncode if god_process else "inconnu"
        print(f"\n\nDieu n'a pas terminé correctement (code {code}).")
        return False

    verification_process = None
    verification = None
    for _ in range(30):
        verification_process = start_daemon(root)
        while verification_process.poll() is None:
            phase, _ = evolution_snapshot(root, request_id)
            if phase != last_phase:
                print()
                last_phase = phase
            show_live_status(root, request_id, phase, int(time.monotonic() - started))
            time.sleep(2)
        phase, verification = evolution_snapshot(root, request_id)
        if phase == "verification":
            break
        time.sleep(2)
    phase, verification = evolution_snapshot(root, request_id)
    if phase != "verification":
        code = verification_process.returncode if verification_process else "inconnu"
        print(f"\n\nLa vérification n'a pas produit de bilan (code {code}).")
        return False
    display_run_summary(root, request_id, verification)
    return True


def interactive(root):
    print("Autopoiesis · interface de validation")
    print("Les fichiers JSON restent la source officielle.")
    while True:
        approved_items = load_approved_evolutions(root)
        if approved_items:
            request, record = approved_items[0]
            print_request(request, record, 1, 1)
            print("\nCette demande est déjà approuvée et attend l'exécution de Dieu.")
            choice = input("[l] lancer Dieu  [d] détail  [q] quitter : ").strip().lower()
            if choice == "l":
                run_approved_evolution(root, request["id"])
                again = input("\nRelancer une autre évolution ? [o/N] : ").strip().lower()
                if again == "o":
                    continue
                return 0
            if choice == "d":
                show_detail(request)
                input("Appuyez sur Entrée pour revenir à la demande.")
                continue
            if choice == "q":
                return 0
            print("Choix inconnu.")
            continue

        items = load_state(root)
        if not items:
            print("\nAucune demande à valider.")
            return 0
        for index, (request, record) in enumerate(items, 1):
            print_request(request, record, index, len(items))
            if not record:
                print("Le Validator travaille encore. Appuyez sur Entrée pour actualiser, ou q pour quitter.")
                choice = input("[Entrée/q] : ").strip().lower()
                if choice == "q":
                    return 0
                break
            choice = input("\n[a] approuver  [r] refuser  [d] détail  [q] quitter : ").strip().lower()
            if choice == "a":
                confirmation = input("Confirmer l'approbation ? [o/N] : ").strip().lower()
                if confirmation == "o":
                    run_decision(root, request["id"], "approve")
                    run_approved_evolution(root, request["id"])
                    again = input("\nRelancer une autre évolution ? [o/N] : ").strip().lower()
                    if again == "o":
                        break
                    return 0
                break
            if choice == "r":
                reason = input("Motif du refus (facultatif) : ").strip()
                run_decision(root, request["id"], "reject", reason)
                break
            if choice == "d":
                show_detail(request)
                input("Appuyez sur Entrée pour revenir à la demande.")
                break
            if choice == "q":
                return 0
            print("Choix inconnu.")
            break


def main():
    parser = argparse.ArgumentParser(description="Interface terminal des demandes d'évolution")
    parser.add_argument("--has-work", action="store_true", help="retourne 0 si une demande ou une évolution attend une action")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    if args.has_work:
        return 0 if has_work(root) else 1
    try:
        return interactive(root)
    except KeyboardInterrupt:
        print("\nInterface interrompue.")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
