#!/usr/bin/env bash
set -euo pipefail
ARQUIVO="${1:-$(ls -1t /home/ubuntu/backups/bayuka-diario/bayuka-*.sql.gz | head -1)}"
NOME=bayuka-restauracao-teste
IMAGEM=$(docker inspect -f '{{.Config.Image}}' ac-database)
SENHA=$(openssl rand -hex 16)

docker rm -f "$NOME" >/dev/null 2>&1 || true
trap 'docker rm -f "$NOME" >/dev/null 2>&1 || true' EXIT
docker run -d --name "$NOME" --network none -e MYSQL_ROOT_PASSWORD="$SENHA" "$IMAGEM" >/dev/null

for _ in $(seq 1 60); do
  docker exec -e MYSQL_PWD="$SENHA" "$NOME" mysql -uroot -e 'select 1' >/dev/null 2>&1 && break
  sleep 2
done

zcat "$ARQUIVO" | docker exec -i -e MYSQL_PWD="$SENHA" "$NOME" mysql -uroot

CONSULTA="select (select count(*) from acore_auth.account), (select count(*) from acore_characters.characters), (select count(*) from acore_world.creature), (select count(*) from acore_world.spell_area where spell=58600)"
echo "restaurado: $(docker exec -e MYSQL_PWD="$SENHA" "$NOME" mysql -uroot -N -e "$CONSULTA")"
echo "producao:   $(docker exec ac-database sh -c 'MYSQL_PWD="$MYSQL_ROOT_PASSWORD" mysql -uroot -N -e "'"$CONSULTA"'"')"
