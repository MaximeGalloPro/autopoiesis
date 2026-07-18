#!/usr/bin/env python3
"""Create one bounded reformulation from a Validator recommendation."""

import datetime
import json
import pathlib
import sys


def non_empty(value):
    return isinstance(value, str) and bool(value.strip())


def non_empty_list(value):
    return isinstance(value, list) and bool(value) and all(non_empty(item) for item in value)


def valid_request(value):
    if not isinstance(value, dict):
        return False
    for field in ("title", "need", "obstacle", "proposed_change", "acceptance_tests"):
        if field == "acceptance_tests":
            if not non_empty_list(value.get(field)):
                return False
        elif not non_empty(value.get(field)):
            return False
    mechanism = value.get("mechanism")
    if not isinstance(mechanism, dict):
        return False
    if not non_empty(mechanism.get("name")) or not non_empty(mechanism.get("summary")):
        return False
    return all(non_empty_list(mechanism.get(field)) for field in (
        "resources", "actions", "preconditions", "deterministic_effects"
    ))


def write_record(path, record):
    path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def read_jsonl(path, start_line=0):
    items = []
    for line in path.read_text(encoding="utf-8").splitlines()[start_line:]:
        if not line.strip():
            continue
        try:
            items.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return items


def main():
    if len(sys.argv) not in {3, 4}:
        print("Usage: reformulate-feature.py <data-directory> <max-reformulations> [request-offset-file]", file=sys.stderr)
        return 2
    data_dir = pathlib.Path(sys.argv[1])
    maximum = int(sys.argv[2])
    requests_path = data_dir / "feature_requests.jsonl"
    runs_dir = data_dir / "evolution_runs"
    if not requests_path.exists():
        return 0

    offset = 0
    if len(sys.argv) == 4:
        try:
            offset = max(0, int(pathlib.Path(sys.argv[3]).read_text(encoding="utf-8").strip()))
        except (FileNotFoundError, ValueError):
            pass
    requests = read_jsonl(requests_path, offset)
    known_ids = {item.get("id") for item in read_jsonl(requests_path)}
    for request in requests:
        request_id = request.get("id")
        if not request_id:
            continue
        run_dir = runs_dir / request_id
        record_path = run_dir / "validation-record.json"
        reformulation_record_path = run_dir / "reformulation-record.json"
        if not record_path.exists() or reformulation_record_path.exists():
            continue
        try:
            record = json.loads(record_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            continue
        if record.get("terminal") or record.get("status") in {"rejected", "reformulated"}:
            continue
        recommendation = record.get("recommendation", {})
        if recommendation.get("decision") != "reformulate":
            continue

        attempt = int(request.get("reformulation_attempt", 0))
        if attempt >= maximum:
            record.update({
                "status": "rejected",
                "terminal": True,
                "rejection_reason": f"Limite de {maximum} reformulations atteinte",
                "rejected_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            })
            write_record(record_path, record)
            print(json.dumps({"status": "rejected", "request_id": request_id}, ensure_ascii=False))
            return 0

        candidate = recommendation.get("reformulated_request")
        if not valid_request(candidate):
            record.update({
                "status": "rejected",
                "terminal": True,
                "rejection_reason": "La reformulation Validator ne respecte pas le contrat",
                "rejected_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            })
            write_record(record_path, record)
            print(json.dumps({"status": "rejected", "request_id": request_id}, ensure_ascii=False))
            return 0

        next_attempt = attempt + 1
        original_id = request.get("original_request_id", request_id)
        child_id = f"{original_id}-r{next_attempt}"
        while child_id in known_ids:
            next_attempt += 1
            child_id = f"{request_id}-r{next_attempt}"
        child = dict(request)
        child.update(candidate)
        child.update({
            "id": child_id,
            "status": "pending",
            "source": "validator_reformulation",
            "original_request_id": original_id,
            "parent_request_id": request_id,
            "reformulation_attempt": next_attempt,
            "reformulated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        })
        with requests_path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(child, ensure_ascii=False) + "\n")
        record.update({
            "status": "reformulated",
            "reformulated_to": child_id,
            "reformulation_attempt": next_attempt,
            "reformulated_at": child["reformulated_at"],
        })
        write_record(record_path, record)
        write_record(reformulation_record_path, {
            "request_id": request_id,
            "new_request_id": child_id,
            "attempt": next_attempt,
            "status": "created",
        })
        print(json.dumps({"status": "reformulated", "request_id": request_id, "new_request_id": child_id}, ensure_ascii=False))
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
