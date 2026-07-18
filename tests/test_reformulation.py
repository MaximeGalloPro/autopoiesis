#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys
import tempfile


def request(request_id):
    return {
        "id": request_id,
        "status": "pending",
        "title": "Test",
        "need": "Need",
        "obstacle": "Obstacle",
        "proposed_change": "Change",
        "mechanism": {
            "name": "Mechanism",
            "summary": "Summary",
            "resources": ["resource"],
            "actions": ["action"],
            "preconditions": ["precondition"],
            "deterministic_effects": ["effect"],
        },
        "acceptance_tests": ["It works"],
    }


def write_record(runs, item, terminal=False):
    run = runs / item["id"]
    run.mkdir(parents=True)
    replacement = request("replacement")
    replacement.pop("id")
    replacement.pop("status")
    record = {
        "status": "rejected" if terminal else "reviewed",
        "terminal": terminal,
        "recommendation": {
            "decision": "reformulate",
            "reformulated_request": replacement,
        },
    }
    (run / "validation-record.json").write_text(json.dumps(record), encoding="utf-8")


def main():
    root = pathlib.Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory() as temporary:
        data = pathlib.Path(temporary)
        runs = data / "evolution_runs"
        old = request("old-request")
        terminal = request("terminal-request")
        current = request("current-request")
        requests = data / "feature_requests.jsonl"
        requests.write_text(
            "\n".join(json.dumps(item) for item in (old, terminal, current)) + "\n",
            encoding="utf-8",
        )
        offset = data / "evolution-session-request-offset"
        offset.write_text("1\n", encoding="utf-8")
        write_record(runs, old)
        write_record(runs, terminal, terminal=True)
        write_record(runs, current)

        subprocess.run(
            [sys.executable, str(root / "scripts/reformulate-feature.py"), str(data), "3", str(offset)],
            check=True,
            capture_output=True,
            text=True,
        )
        items = [json.loads(line) for line in requests.read_text(encoding="utf-8").splitlines()]
        ids = {item["id"] for item in items}
        assert "current-request-r1" in ids
        assert "old-request-r1" not in ids
        assert "terminal-request-r1" not in ids

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
