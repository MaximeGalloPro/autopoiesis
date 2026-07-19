#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def executable(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")
    path.chmod(0o755)


def main() -> None:
    source_root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="autopoiesis-run-launcher-") as temporary:
        root = Path(temporary)
        shutil.copy2(source_root / "run.sh", root / "run.sh")
        (root / "build").mkdir()
        tools = root / "tools"
        tools.mkdir()
        capture = root / "arguments.txt"

        executable(tools / "docker", "#!/bin/sh\nexit 0\n")
        executable(tools / "cmake", "#!/bin/sh\nexit 0\n")
        executable(
            root / "build" / "autopoiesis_gui",
            '#!/bin/sh\nprintf "%s\\n" "$@" > "$RUN_CAPTURE"\n',
        )

        environment = os.environ.copy()
        environment.update(
            {
                "PATH": f"{tools}:{environment['PATH']}",
                "AUTOPOIESIS_UI": "gui",
                "EVOLUTION_AUTOSTART": "0",
                "USE_API": "0",
                "RUN_CAPTURE": str(capture),
            }
        )
        result = subprocess.run(
            ["/bin/bash", str(root / "run.sh")],
            cwd=root,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert capture.read_text(encoding="utf-8").splitlines() == ["--no-api"]

        terminal_environment = environment.copy()
        terminal_environment["AUTOPOIESIS_UI"] = "terminal"
        terminal_result = subprocess.run(
            ["/bin/bash", str(root / "run.sh")],
            cwd=root,
            env=terminal_environment,
            capture_output=True,
            text=True,
            check=False,
        )
        assert terminal_result.returncode == 0, terminal_result.stderr


if __name__ == "__main__":
    main()
