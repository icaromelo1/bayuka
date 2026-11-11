# mod-faccao-personagem

Deixa um personagem jogar pela facção oposta à da sua raça (por exemplo, Humano na
Horda) sem mudar a raça nem afetar os outros personagens da mesma raça.

## Como funciona

- O personagem continua com a raça real: modelo, animações, raciais e as classes
  permitidas pela matriz do `mod-uac` (Druida segue só Elfo Noturno/Tauren).
- A escolha fica em `acore_characters.faccao_personagem`. Ao carregar o personagem, o
  hook `OnPlayerUpdateFaction` troca a equipe e o faction template para os da facção
  escolhida e define uma "raça de facção" (Orc para a Horda, Humano para a Aliança).
- O patch do core (`faccao-core.patch`) faz as regras de facção usarem essa raça de
  facção: missões, reputação inicial, itens por facção, voos iniciais, recompensas
  por correio, leilão e buffs da ICC. Condições de raça aceitam a raça real ou a de
  facção. Raciais, habilidades de classe e auras de pet continuam pela raça real.
- Fala em Comum vira Órquico para quem joga pela Horda (e o inverso), e o personagem
  aprende o idioma da facção escolhida.

## Uso

- Primeiro login de personagem elegível (padrão: Humano, nível 1, não Cavaleiro da
  Morte): aparece o menu "Ficar na Aliança / Jogar pela Horda".
- `.faccao menu`: reabre o menu enquanto o personagem estiver no nível permitido.
- `.faccao definir <nome> <horda|alianca|nativa>` (GM): troca qualquer personagem.
  Online, aplica na hora; offline, aplica no próximo login.
- `.faccao ver <nome>` (GM): mostra raça, facção nativa, escolha e estado online.

Ao trocar, o personagem tem a reputação refeita para a base da nova facção, os voos
conhecidos refeitos, o lar movido e é levado para a área inicial da raça
representante (Valley of Trials ou Northshire).

## Configuração

`conf/faccao_personagem.conf.dist`: raças permitidas, classes bloqueadas, nível máximo
para o jogador escolher sozinho, raças representantes e conversão de idioma.

## Instalação e remoção

- SQL: `data/sql/db-characters/base/faccao_personagem.sql`.
- Remoção: `data/sql/db-uninstall/faccao_personagem_uninstall.sql`. Sem o módulo, os
  personagens voltam a seguir a facção da raça.
