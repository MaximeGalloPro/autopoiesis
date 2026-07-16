#!/usr/bin/env python3
import pathlib
import sys

changed = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8").splitlines()
forbidden = (".git/", "data/", "scripts/")
violations = [path for path in changed if path == ".env" or path.startswith(forbidden)]
if violations:
    raise SystemExit("Fichiers interdits modifies par Dieu: " + ", ".join(violations))
