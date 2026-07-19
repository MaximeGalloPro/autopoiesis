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
        web_capture = root / "web-arguments.txt"
        docker_capture = root / "docker-arguments.txt"

        executable(
            tools / "docker",
            '#!/bin/sh\nif [ "${2:-}" = run ]; then printf "%s\\n" "$@" > "$DOCKER_CAPTURE"; fi\n',
        )
        executable(tools / "cmake", "#!/bin/sh\nexit 0\n")
        executable(
            tools / "bun",
            """#!/bin/sh
case "$*" in
  *web/server/index.ts*)
    printf '%s\n' "$AUTOPOIESIS_BACKEND_PATH" "$AUTOPOIESIS_DATA_DIR" "$USE_API" > "$WEB_CAPTURE"
    printf '%s\n' "$@" >> "$WEB_CAPTURE"
    ;;
esac
""",
        )

        environment = os.environ.copy()
        environment.update(
            {
                "PATH": f"{tools}:{environment['PATH']}",
                "AUTOPOIESIS_UI": "web",
                "EVOLUTION_AUTOSTART": "0",
                "USE_API": "0",
                "WEB_CAPTURE": str(web_capture),
                "DOCKER_CAPTURE": str(docker_capture),
            }
        )
        result = subprocess.run(
            ["/bin/bash", str(root / "run.sh"), "--days", "3"],
            cwd=root,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert web_capture.read_text(encoding="utf-8").splitlines() == [
            str(root / "build" / "autopoiesis_backend"),
            str(root / "data"),
            "0",
            str(root / "web" / "server" / "index.ts"),
            "--days",
            "3",
        ]

        terminal_result = subprocess.run(
            ["/bin/bash", str(root / "run.sh"), "--terminal", "--days", "2"],
            cwd=root,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )
        assert terminal_result.returncode == 0, terminal_result.stderr
        assert docker_capture.read_text(encoding="utf-8").splitlines() == [
            "compose",
            "run",
            "--rm",
            "--entrypoint",
            "/app/build/autopoiesis",
            "autopoiesis",
            "--no-api",
            "--days",
            "2",
        ]


if __name__ == "__main__":
    main()
