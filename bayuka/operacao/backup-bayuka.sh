#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/local/bin:/usr/bin:/bin:/home/ubuntu/.local/bin

DESTINO_REMOTO="${DESTINO_REMOTO:-gdrive:oracle-backups/bayuka}"
PASTA=/home/ubuntu/backups/bayuka-diario
LOG=/home/ubuntu/projects/backup/backup-bayuka.log
DIAS_LOCAL=7
DIAS_REMOTO=30
BANCOS="acore_auth acore_characters acore_playerbots acore_world"

mkdir -p "$PASTA"
chmod 700 "$PASTA"
CARIMBO=$(date +%F_%H%M)
ARQUIVO="$PASTA/bayuka-$CARIMBO.sql.gz"

registrar() { echo "[$(date '+%F %T')] $*" >> "$LOG"; }

falhar() {
  registrar "FALHA: $1"
  rm -f "$ARQUIVO"
  suporte-aviso alerta bayuka "Backup dos bancos falhou: $1" >/dev/null 2>&1 || true
  exit 1
}

docker exec ac-database sh -c 'MYSQL_PWD="$MYSQL_ROOT_PASSWORD" exec mysqldump -uroot --single-transaction --routines --triggers --events --databases '"$BANCOS" \
  | gzip -6 > "$ARQUIVO"
[ "${PIPESTATUS[0]}" -eq 0 ] || falhar "mysqldump terminou com erro"
chmod 600 "$ARQUIVO"

gzip -t "$ARQUIVO" || falhar "arquivo gzip corrompido"
zcat "$ARQUIVO" | tail -1 | grep -q "Dump completed" || falhar "dump incompleto"
for banco in $BANCOS; do
  zgrep -q "^USE \`$banco\`" "$ARQUIVO" || falhar "banco $banco ausente no dump"
done

rclone copy "$ARQUIVO" "$DESTINO_REMOTO" >> "$LOG" 2>&1 || falhar "envio ao Google Drive"

find "$PASTA" -name 'bayuka-*.sql.gz' -mtime +"$DIAS_LOCAL" -delete
rclone delete "$DESTINO_REMOTO" --min-age "${DIAS_REMOTO}d" >> "$LOG" 2>&1 || registrar "aviso: limpeza remota falhou"

registrar "ok $(du -h "$ARQUIVO" | cut -f1) $ARQUIVO"
