#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <success|failure|timeout> <request-id>" >&2
  exit 2
fi

if [[ "${MACOS_NOTIFICATIONS:-1}" != "1" ]]; then
  exit 0
fi
if [[ "$(uname -s)" != "Darwin" ]] || ! command -v osascript >/dev/null 2>&1; then
  exit 0
fi

event="$1"
request_id="$2"
title="Autopoiesis"
case "$event" in
  success)
    message="Dieu a terminé. Version activée et poussée pour $request_id."
    sound="Glass"
    ;;
  failure)
    message="Le travail de Dieu a échoué pour $request_id. Consultez le terminal."
    sound="Basso"
    ;;
  timeout)
    message="Le travail de Dieu dépasse le délai pour $request_id et demande ton attention."
    sound="Ping"
    ;;
  *)
    echo "Type de notification inconnu: $event" >&2
    exit 2
    ;;
esac

osascript - "$title" "$message" "$sound" >/dev/null 2>&1 <<'APPLESCRIPT' || true
on run argv
  display notification (item 2 of argv) with title (item 1 of argv) sound name (item 3 of argv)
end run
APPLESCRIPT
