#!/bin/bash
set -euo pipefail

uso() {
  echo "uso: bayuka-reiniciar.sh [-m minutos] [-r motivo] [-n nota.json] [--agora] [-- comando de publicacao]"
  echo "  sem comando: reinicia o ac-worldserver atual (docker restart)"
  echo "  com comando: -n e obrigatorio; a nota entra no patch-notes.json do portal quando o servidor volta"
  exit 1
}

MINUTOS=10
MOTIVO="manutencao"
AGORA=0
NOTA=""
COMANDO=()
while [ $# -gt 0 ]; do
  case "$1" in
    -m) MINUTOS=$2; shift 2 ;;
    -r) MOTIVO=$2; shift 2 ;;
    -n) NOTA=$2; shift 2 ;;
    --agora) AGORA=1; shift ;;
    --) shift; COMANDO=("$@"); break ;;
    *) uso ;;
  esac
done
[[ "$MINUTOS" =~ ^[0-9]+$ ]] || uso
if [ ${#COMANDO[@]} -gt 0 ] && [ -z "$NOTA" ]; then
  echo "publicacao sem nota: passe -n nota.json (formato do patch-notes.json do portal)"
  exit 1
fi

. ~/projects/bayuka-wotlk/contas.env
PROD=~/projects/bayuka-wotlk/azerothcore-wotlk
REGISTRO=~/builds/reinicios.log
NOTAS=~/projects/wow-portal/site/patch-notes.json
BACKUP_NOTAS=~/backups/bayuka-patch-notes

nota_valida() {
  python3 - "$1" <<'PY'
import json, sys
nota = json.load(open(sys.argv[1], encoding="utf-8"))
assert isinstance(nota, dict), "a nota precisa ser um objeto"
assert isinstance(nota.get("title"), str) and nota["title"].strip(), "falta title"
grupos = nota.get("groups")
assert isinstance(grupos, list) and grupos, "falta groups"
for grupo in grupos:
    assert isinstance(grupo.get("title"), str) and grupo["title"].strip(), "grupo sem title"
    itens = grupo.get("items")
    assert isinstance(itens, list) and itens and all(isinstance(i, str) and i.strip() for i in itens), "grupo sem items"
print(nota["title"])
PY
}

publicar_nota() {
  mkdir -p "$BACKUP_NOTAS"
  cp "$NOTAS" "$BACKUP_NOTAS/patch-notes-$(date +%Y%m%d-%H%M%S).json"
  python3 - "$NOTA" "$NOTAS" "$(TZ=America/Sao_Paulo date +%F)" <<'PY'
import json, os, sys
nota_arq, destino, hoje = sys.argv[1:4]
nota = json.load(open(nota_arq, encoding="utf-8"))
nota.setdefault("date", hoje)
notas = json.load(open(destino, encoding="utf-8"))
notas.insert(0, nota)
temporario = destino + ".novo"
with open(temporario, "w", encoding="utf-8") as saida:
    json.dump(notas, saida, ensure_ascii=False, indent=2)
    saida.write("\n")
os.chmod(temporario, 0o644)
os.replace(temporario, destino)
PY
}

soap() {
  local cmd=${1//&/&amp;}
  cmd=${cmd//</&lt;}
  cmd=${cmd//>/&gt;}
  docker run --rm --network bayuka-wotlk_ac-network curlimages/curl -s -m 15 -u "claude:$CLAUDE_SENHA" \
    -H "content-type: text/xml" \
    -d "<?xml version=\"1.0\"?><SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:ns1=\"urn:AC\"><SOAP-ENV:Body><ns1:executeCommand><command>$cmd</command></ns1:executeCommand></SOAP-ENV:Body></SOAP-ENV:Envelope>" \
    http://ac-worldserver:7878/ </dev/null | grep -o "<result>[^<]*\|<faultstring>[^<]*" | sed 's/<result>//; s/<faultstring>/ERRO: /; s/&#xD;//g' || true
}

avisar() {
  soap ".announce $1" >/dev/null
  soap ".notify $1" >/dev/null
  echo "$(date '+%H:%M:%S') aviso: $1"
}

quando() {
  local s=$1
  if [ "$s" -ge 120 ]; then echo "$((s / 60)) minutos"
  elif [ "$s" -eq 60 ]; then echo "1 minuto"
  else echo "$s segundos"
  fi
}

TITULO_NOTA=""
if [ -n "$NOTA" ]; then
  TITULO_NOTA=$(nota_valida "$NOTA") || { echo "nota invalida: $NOTA"; exit 1; }
fi

echo "$(date '+%F %T') inicio: ${MINUTOS}min motivo=\"$MOTIVO\" comando=\"${COMANDO[*]:-docker restart ac-worldserver}\"" >> "$REGISTRO"

if [ "$AGORA" -eq 0 ] && [ "$MINUTOS" -gt 0 ]; then
  RESTANTE=$((MINUTOS * 60))
  avisar "Servidor reinicia em $(quando "$RESTANTE") ($MOTIVO)."
  for MARCO in 1800 900 600 300 120 60 30 10; do
    [ "$MARCO" -ge "$RESTANTE" ] && continue
    sleep $((RESTANTE - MARCO))
    RESTANTE=$MARCO
    avisar "Servidor reinicia em $(quando "$MARCO") ($MOTIVO)."
  done
  sleep "$RESTANTE"
fi

avisar "Reiniciando agora. Volta em cerca de 2 minutos."
soap ".saveall" >/dev/null
sleep 3
DESDE=$(date -u +%Y-%m-%dT%H:%M:%SZ)

if [ ${#COMANDO[@]} -gt 0 ]; then
  (cd "$PROD" && "${COMANDO[@]}")
else
  docker restart ac-worldserver >/dev/null
fi

for i in $(seq 1 90); do
  docker logs --since "$DESDE" ac-worldserver 2>&1 | grep -q "ready\.\.\." && break
  sleep 5
done
ESTADO=$(docker inspect ac-worldserver --format '{{.Config.Image}} {{.State.Status}}')
PRONTO=$(docker logs --since "$DESDE" ac-worldserver 2>&1 | grep -c "ready\.\.\." || true)
echo "$(date '+%F %T') fim: $ESTADO pronto=$PRONTO" >> "$REGISTRO"
echo "$ESTADO pronto=$PRONTO"
[ "$PRONTO" -gt 0 ] || exit 1

if [ -n "$NOTA" ]; then
  publicar_nota
  echo "$(date '+%F %T') nota publicada: $TITULO_NOTA" >> "$REGISTRO"
  sleep 20
  avisar "Novidades no ar: $TITULO_NOTA. Detalhes em icaromelodev.com.br/bayuka"
fi
