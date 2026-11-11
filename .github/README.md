# Bayuka

Servidor privado de World of Warcraft 3.3.5a (Wrath of the Lich King, build 12340), com bots que completam grupo e mudanças próprias de jogo. É um fork do [AzerothCore com Playerbots](https://github.com/mod-playerbots/azerothcore-wotlk), que por sua vez segue o [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk).

- Para jogar: [icaromelodev.com.br/bayuka](https://icaromelodev.com.br/bayuka) (cadastro, instalador e patch notes).
- Reino: `set realmlist icaromelodev.com.br`

## O que o Bayuka muda

| Mudança | Onde está |
|---|---|
| Voo liberado no mundo aberto de Azeroth, com velocidade mínima de 310% | commit "Libera voo no mundo aberto de Azeroth", `modules/mod-flying-mount-speed` |
| Voo proibido nas capitais antigas (aviso, desmonte e paraquedas, como Dalaran) | config `ZonasSemVoo`, `bayuka/sql/2026-11-11-zonas-sem-voo.sql` |
| Facção escolhida por personagem (ex.: Humano na Horda) | `modules/mod-faccao-personagem` e commit "Adiciona facção por personagem no núcleo" |
| 92 combinações de raça e classe | `modules/mod-uac` (fork em [icaroMelo1/mod-uac](https://github.com/icaroMelo1/mod-uac), branch `bayuka`) |
| Transmog | `modules/mod-transmog` |
| 80 bots pelo mundo, Aliança e Horda | `modules/mod-playerbots`, configuração em `bayuka/docker/` |
| Grupos entre facções e todo o conteúdo do Lich King até o nível 80 | configuração do servidor |

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `src/` | núcleo do AzerothCore com as mudanças do Bayuka |
| `modules/` | módulos próprios (no repositório) e de terceiros (submódulos fixados na versão em produção) |
| `bayuka/docker/` | override do Docker Compose usado no servidor |
| `bayuka/sql/` | SQL aplicado à mão no banco, cada um com o seu rollback |
| `bayuka/operacao/` | reinício com aviso no jogo, backup diário, teste de restauração e alerta de queda |
| `bayuka/patch-notes/` | notas publicadas no portal, uma por atualização |
| `bayuka/cliente/` | ferramentas que geram os patches do cliente a partir da instalação do jogador |

## Compilar

```sh
git clone --recurse-submodules -b bayuka https://github.com/icaroMelo1/bayuka.git
cd bayuka
cp bayuka/docker/docker-compose.override.yml .
docker compose build ac-db-import ac-worldserver ac-authserver ac-client-data-init
docker compose up -d
```

O resto (dados do cliente, primeira importação do banco, configuração) segue a [documentação do AzerothCore](https://www.azerothcore.org/wiki/install-with-docker).

## Contribuir

1. Abra uma issue descrevendo a mudança antes de começar, principalmente se mexer em regra de jogo.
2. Faça um fork, crie uma branch a partir de `bayuka` e mande o pull request para `bayuka`.
3. Um assunto por pull request. Mudança de núcleo pequena e isolada; o que der para fazer em módulo, faça em módulo.
4. SQL de banco vai em `bayuka/sql/` com data no nome e um `.rollback.sql` ao lado.
5. Toda mudança visível para o jogador traz uma nota em `bayuka/patch-notes/`.
6. Nada de arquivos da Blizzard (MPQ, DBC, modelos, texturas) no repositório: as ferramentas leem da instalação de quem joga.
7. Commits em português, no imperativo ("Adiciona…", "Corrige…").

Mudanças que valem para todo o AzerothCore devem ir também para o [projeto original](https://github.com/azerothcore/azerothcore-wotlk).

## Licença

GPL-2.0, a mesma do AzerothCore (ver `LICENSE`). World of Warcraft é marca da Blizzard Entertainment; este projeto não tem relação com ela e não distribui o jogo.
