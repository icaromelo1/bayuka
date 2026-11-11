#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/local/bin:/usr/bin:/bin:/home/ubuntu/.local/bin

ESTADO=/home/ubuntu/builds/.bayuka-vigia
REINICIOS=/home/ubuntu/builds/reinicios.log
CONTAINERS="ac-worldserver ac-authserver ac-database"

mkdir -p "$ESTADO"

avisar() { suporte-aviso "$1" bayuka "$2" >/dev/null 2>&1 || true; }

reinicio_planejado() {
  local ultimo
  ultimo=$(grep ' inicio: ' "$REINICIOS" 2>/dev/null | tail -1 | cut -c1-19)
  [ -n "$ultimo" ] || return 1
  [ $(( $(date +%s) - $(date -d "$ultimo" +%s) )) -lt 1800 ]
}

for c in $CONTAINERS; do
  atual=$(docker inspect -f '{{.State.Status}} {{.State.StartedAt}}' "$c" 2>/dev/null || echo "ausente -")
  status=${atual%% *}
  anterior=$(cat "$ESTADO/$c" 2>/dev/null || echo "")
  echo "$atual" > "$ESTADO/$c"
  [ -n "$anterior" ] || continue
  [ "$atual" != "$anterior" ] || continue

  if [ "$status" != "running" ]; then
    reinicio_planejado || avisar alerta "$c parou ($status)."
  elif [ "${anterior%% *}" != "running" ]; then
    reinicio_planejado || avisar alerta "$c voltou a rodar."
  else
    reinicio_planejado || avisar alerta "$c reiniciou sozinho (queda). Ver: docker logs --since 15m $c"
  fi
done
