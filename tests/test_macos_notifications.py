#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile


def executable(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")
    path.chmod(0o755)


root = Path(__file__).resolve().parents[1]
notifier = root / "scripts" / "notify-macos.sh"

with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    binaries = directory / "bin"
    binaries.mkdir()
    capture = directory / "notification"
    executable(binaries / "uname", "#!/bin/sh\nprintf 'Darwin\\n'\n")
    executable(
        binaries / "osascript",
        "#!/bin/sh\nprintf '%s\\n' \"$@\" >> \"$NOTIFICATION_CAPTURE.args\"\n"
        "cat >> \"$NOTIFICATION_CAPTURE.script\"\n",
    )
    environment = os.environ.copy()
    environment["PATH"] = f"{binaries}:{environment['PATH']}"
    environment["NOTIFICATION_CAPTURE"] = str(capture)

    environment["MACOS_NOTIFICATIONS"] = "0"
    subprocess.run(
        [str(notifier), "success", "request-1"],
        check=True,
        env=environment,
    )
    assert not Path(f"{capture}.args").exists()

    environment["MACOS_NOTIFICATIONS"] = "1"
    subprocess.run(
        [str(notifier), "success", "request-1"],
        check=True,
        env=environment,
    )
    arguments = Path(f"{capture}.args").read_text(encoding="utf-8")
    script = Path(f"{capture}.script").read_text(encoding="utf-8")
    assert "Autopoiesis" in arguments
    assert "request-1" in arguments
    assert "Version activée et poussée" in arguments
    assert "display notification" in script

    subprocess.run(
        [str(notifier), "timeout", "request-2"],
        check=True,
        env=environment,
    )
    arguments = Path(f"{capture}.args").read_text(encoding="utf-8")
    assert "demande ton attention" in arguments

    Path(f"{capture}.args").unlink()
    Path(f"{capture}.script").unlink()
    data = directory / "data"
    runs = data / "evolution_runs"
    runs.mkdir(parents=True)
    session = data / "evolution-session-started"
    session.touch()
    os.utime(session, (1, 1))
    evidence = {
        "success-request": "activation.json",
        "failed-request": "god-failed",
        "timeout-request": "ui-work-timeout",
    }
    for request_id, marker in evidence.items():
        run = runs / request_id
        run.mkdir()
        (run / marker).write_text("event\n", encoding="utf-8")

    observer = root / "scripts" / "notify-evolution-events.sh"
    subprocess.run([str(observer), str(data)], check=True, env=environment, cwd=root)
    first_pass = Path(f"{capture}.args").read_text(encoding="utf-8")
    assert first_pass.count("Autopoiesis") == 3
    assert "Version activée et poussée" in first_pass
    assert "a échoué" in first_pass
    assert "demande ton attention" in first_pass
    assert (runs / "success-request/notification-success-sent").exists()
    assert (runs / "failed-request/notification-failure-sent").exists()
    assert (runs / "timeout-request/notification-timeout-sent").exists()

    subprocess.run([str(observer), str(data)], check=True, env=environment, cwd=root)
    second_pass = Path(f"{capture}.args").read_text(encoding="utf-8")
    assert second_pass == first_pass
