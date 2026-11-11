#include "CharacterCache.h"
#include "Chat.h"
#include "CommandScript.h"
#include "DBCStructure.h"
#include "Log.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GossipDef.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringConvert.h"
#include "Tokenize.h"

#include <mutex>
#include <set>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
constexpr uint32 MENU_ESCOLHA_FACCAO = 0xFACC0;
constexpr uint32 ACAO_FICAR_NATIVA = 1;
constexpr uint32 ACAO_TROCAR_FACCAO = 2;
constexpr uint32 SPELL_IDIOMA_ORCISH = 669;
constexpr uint32 SPELL_IDIOMA_COMMON = 668;
constexpr uint32 ESPERA_MAXIMA_CINEMATICA = 180;

struct Configuracao
{
    bool ativo = true;
    bool escolhaNoPrimeiroLogin = true;
    bool converterIdioma = true;
    uint8 nivelMaximoEscolha = 1;
    uint8 racaRepresentanteHorda = RACE_ORC;
    uint8 racaRepresentanteAlianca = RACE_HUMAN;
    std::set<uint8> racasPermitidas;
    std::set<uint8> classesBloqueadas;
};

struct Escolha
{
    TeamId equipe;
    bool aplicarNoLogin;
};

Configuracao config;
std::unordered_map<ObjectGuid::LowType, Escolha> escolhas;
std::mutex escolhasMutex;

std::set<uint8> LerLista(std::string const& valor)
{
    std::set<uint8> lista;
    for (std::string_view parte : Acore::Tokenize(valor, ',', false))
        if (Optional<uint32> numero = Acore::StringTo<uint32>(parte))
            lista.insert(uint8(*numero));
    return lista;
}

TeamId EquipeNativa(Player const* player)
{
    return Player::TeamIdForRace(player->getRace(true));
}

uint8 RacaRepresentante(TeamId equipe)
{
    return equipe == TEAM_HORDE ? config.racaRepresentanteHorda : config.racaRepresentanteAlianca;
}

bool Elegivel(Player const* player)
{
    if (!config.ativo)
        return false;
    if (config.classesBloqueadas.count(player->getClass()))
        return false;
    return config.racasPermitidas.empty() || config.racasPermitidas.count(player->getRace(true));
}

Optional<Escolha> BuscarEscolha(ObjectGuid::LowType guid)
{
    std::lock_guard<std::mutex> trava(escolhasMutex);
    auto it = escolhas.find(guid);
    if (it == escolhas.end())
        return {};
    return it->second;
}

void GravarEscolha(ObjectGuid::LowType guid, TeamId equipe, TeamId nativa, bool aplicarNoLogin)
{
    std::lock_guard<std::mutex> trava(escolhasMutex);
    if (equipe == nativa)
    {
        escolhas.erase(guid);
        CharacterDatabase.Execute("DELETE FROM `faccao_personagem` WHERE `guid` = {}", guid);
        return;
    }
    escolhas[guid] = { equipe, aplicarNoLogin };
    CharacterDatabase.Execute(
        "REPLACE INTO `faccao_personagem` (`guid`, `equipe`, `aplicar_no_login`) VALUES ({}, {}, {})",
        guid, uint32(equipe), aplicarNoLogin ? 1 : 0);
}

void MarcarAplicado(ObjectGuid::LowType guid)
{
    std::lock_guard<std::mutex> trava(escolhasMutex);
    auto it = escolhas.find(guid);
    if (it == escolhas.end() || !it->second.aplicarNoLogin)
        return;
    it->second.aplicarNoLogin = false;
    CharacterDatabase.Execute("UPDATE `faccao_personagem` SET `aplicar_no_login` = 0 WHERE `guid` = {}", guid);
}

void AjustarEquipe(Player* player)
{
    Optional<Escolha> escolha = BuscarEscolha(player->GetGUID().GetCounter());
    if (!config.ativo || !escolha || escolha->equipe == EquipeNativa(player))
    {
        player->SetFactionProxyRace(0);
        return;
    }

    uint8 representante = RacaRepresentante(escolha->equipe);
    ChrRacesEntry const* entrada = sChrRacesStore.LookupEntry(representante);
    if (!entrada)
    {
        player->SetFactionProxyRace(0);
        return;
    }

    player->SetFactionProxyRace(representante);
    player->setTeamId(escolha->equipe);
    player->SetFaction(entrada->FactionID);
}

PlayerInfo const* InicioDaEquipe(Player const* player, TeamId equipe)
{
    uint8 raca = equipe == EquipeNativa(player) ? player->getRace(true) : RacaRepresentante(equipe);
    if (PlayerInfo const* info = sObjectMgr->GetPlayerInfo(raca, player->getClass()))
        return info;
    return sObjectMgr->GetPlayerInfo(raca, CLASS_WARRIOR);
}

void AplicarConsequencias(Player* player, bool mover)
{
    TeamId equipe = player->GetTeamId();

    player->GetReputationMgr().ResetToBase();
    player->GetReputationMgr().SendInitialReputations();

    std::string mascaraVazia;
    for (std::size_t i = 0; i < TaxiMaskSize; ++i)
        mascaraVazia += "0 ";
    player->m_taxi.LoadTaxiMask(mascaraVazia);
    player->InitTaxiNodesForLevel();

    player->learnSpell(equipe == TEAM_HORDE ? SPELL_IDIOMA_ORCISH : SPELL_IDIOMA_COMMON);

    if (PlayerInfo const* inicio = InicioDaEquipe(player, equipe))
    {
        WorldLocation local(inicio->mapId, inicio->positionX, inicio->positionY, inicio->positionZ, inicio->orientation);
        player->SetHomebind(local, inicio->areaId);
        if (mover)
            player->TeleportTo(local);
    }

    player->SaveToDB(false, false);
}

void DefinirEquipe(Player* player, TeamId equipe, bool mover)
{
    GravarEscolha(player->GetGUID().GetCounter(), equipe, EquipeNativa(player), false);
    player->SetFactionForRace(player->getRace(true));
    AplicarConsequencias(player, mover);
}

std::string NomeEquipe(TeamId equipe)
{
    return equipe == TEAM_HORDE ? "Horda" : "Alianca";
}

void MostrarMenu(Player* player)
{
    TeamId nativa = EquipeNativa(player);
    TeamId outra = nativa == TEAM_HORDE ? TEAM_ALLIANCE : TEAM_HORDE;

    ClearGossipMenuFor(player);
    player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ESCOLHA_FACCAO);
    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Ficar na " + NomeEquipe(nativa), 0, ACAO_FICAR_NATIVA);
    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Jogar pela " + NomeEquipe(outra), 0, ACAO_TROCAR_FACCAO,
        "Seu personagem vai para a cidade inicial da " + NomeEquipe(outra) + ". Confirmar?", 0, false);
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
}

bool PodeEscolherSozinho(Player const* player)
{
    return Elegivel(player) && player->GetLevel() <= config.nivelMaximoEscolha;
}

void MostrarMenuDepoisDaCinematica(Player* player, uint32 segundos)
{
    player->m_Events.AddEventAtOffset([player, segundos]()
    {
        if (!player->IsInWorld() || !PodeEscolherSozinho(player))
            return;
        if (player->GetCinematicMgr().GetActiveCinematicCamera() && segundos < ESPERA_MAXIMA_CINEMATICA)
        {
            MostrarMenuDepoisDaCinematica(player, segundos + 1);
            return;
        }
        player->m_Events.AddEventAtOffset([player]()
        {
            if (player->IsInWorld() && PodeEscolherSozinho(player))
                MostrarMenu(player);
        }, 2s);
    }, 1s);
}

class FaccaoPersonagemWorld : public WorldScript
{
public:
    FaccaoPersonagemWorld() : WorldScript("FaccaoPersonagemWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.ativo = sConfigMgr->GetOption<bool>("FaccaoPersonagem.Ativo", true);
        config.escolhaNoPrimeiroLogin = sConfigMgr->GetOption<bool>("FaccaoPersonagem.EscolhaNoPrimeiroLogin", true);
        config.converterIdioma = sConfigMgr->GetOption<bool>("FaccaoPersonagem.ConverterIdioma", true);
        config.nivelMaximoEscolha = uint8(sConfigMgr->GetOption<uint32>("FaccaoPersonagem.NivelMaximoEscolha", 1));
        config.racaRepresentanteHorda = uint8(sConfigMgr->GetOption<uint32>("FaccaoPersonagem.RacaRepresentanteHorda", RACE_ORC));
        config.racaRepresentanteAlianca = uint8(sConfigMgr->GetOption<uint32>("FaccaoPersonagem.RacaRepresentanteAlianca", RACE_HUMAN));
        config.racasPermitidas = LerLista(sConfigMgr->GetOption<std::string>("FaccaoPersonagem.RacasPermitidas", "1"));
        config.classesBloqueadas = LerLista(sConfigMgr->GetOption<std::string>("FaccaoPersonagem.ClassesBloqueadas", "6"));
    }

    void OnStartup() override
    {
        std::lock_guard<std::mutex> trava(escolhasMutex);
        escolhas.clear();
        if (QueryResult resultado = CharacterDatabase.Query("SELECT `guid`, `equipe`, `aplicar_no_login` FROM `faccao_personagem`"))
        {
            do
            {
                Field* campos = resultado->Fetch();
                uint8 equipe = campos[1].Get<uint8>();
                if (equipe > TEAM_HORDE)
                    continue;
                escolhas[campos[0].Get<uint32>()] = { TeamId(equipe), campos[2].Get<bool>() };
            } while (resultado->NextRow());
        }
        LOG_INFO("module", "mod-faccao-personagem: {} personagem(ns) com faccao escolhida.", escolhas.size());
    }
};

class FaccaoPersonagemPlayer : public PlayerScript
{
public:
    FaccaoPersonagemPlayer() : PlayerScript("FaccaoPersonagemPlayer", {
        PLAYERHOOK_ON_UPDATE_FACTION,
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_FIRST_LOGIN,
        PLAYERHOOK_ON_GOSSIP_SELECT,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_ON_DELETE_FROM_DB }) { }

    void OnPlayerUpdateFaction(Player* player) override
    {
        AjustarEquipe(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        Optional<Escolha> escolha = BuscarEscolha(player->GetGUID().GetCounter());
        if (!config.ativo || !escolha)
            return;
        if (escolha->aplicarNoLogin)
        {
            AplicarConsequencias(player, true);
            MarcarAplicado(player->GetGUID().GetCounter());
        }
        player->learnSpell(player->GetTeamId() == TEAM_HORDE ? SPELL_IDIOMA_ORCISH : SPELL_IDIOMA_COMMON);
    }

    void OnPlayerFirstLogin(Player* player) override
    {
        if (!config.escolhaNoPrimeiroLogin || !PodeEscolherSozinho(player))
            return;
        MostrarMenuDepoisDaCinematica(player, 0);
    }

    void OnPlayerGossipSelect(Player* player, uint32 menuId, uint32 /*sender*/, uint32 acao) override
    {
        if (menuId != MENU_ESCOLHA_FACCAO)
            return;
        CloseGossipMenuFor(player);
        if (!PodeEscolherSozinho(player))
            return;

        TeamId nativa = EquipeNativa(player);
        if (acao == ACAO_TROCAR_FACCAO)
        {
            TeamId outra = nativa == TEAM_HORDE ? TEAM_ALLIANCE : TEAM_HORDE;
            DefinirEquipe(player, outra, true);
            ChatHandler(player->GetSession()).PSendSysMessage("Voce agora joga pela {}.", NomeEquipe(outra));
        }
        else if (acao == ACAO_FICAR_NATIVA && player->GetTeamId() != nativa)
        {
            DefinirEquipe(player, nativa, true);
            ChatHandler(player->GetSession()).PSendSysMessage("Voce voltou para a {}.", NomeEquipe(nativa));
        }
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& lang, std::string& /*msg*/) override
    {
        if (!config.converterIdioma || !player->GetFactionProxyRace())
            return;
        if (player->GetTeamId() == TEAM_HORDE && lang == LANG_COMMON)
            lang = LANG_ORCISH;
        else if (player->GetTeamId() == TEAM_ALLIANCE && lang == LANG_ORCISH)
            lang = LANG_COMMON;
    }

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        trans->Append("DELETE FROM `faccao_personagem` WHERE `guid` = {}", guid);
        std::lock_guard<std::mutex> trava(escolhasMutex);
        escolhas.erase(guid);
    }
};

Optional<TeamId> LerEquipe(std::string_view texto)
{
    if (texto == "horda" || texto == "horde")
        return TEAM_HORDE;
    if (texto == "alianca" || texto == "alliance")
        return TEAM_ALLIANCE;
    return {};
}

class FaccaoPersonagemComandos : public CommandScript
{
public:
    FaccaoPersonagemComandos() : CommandScript("FaccaoPersonagemComandos") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable faccaoTabela =
        {
            { "menu",    HandleMenu,    SEC_PLAYER,     Console::No  },
            { "definir", HandleDefinir, SEC_GAMEMASTER, Console::Yes },
            { "ver",     HandleVer,     SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable tabela =
        {
            { "faccao", faccaoTabela },
        };
        return tabela;
    }

    static bool HandleMenu(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!PodeEscolherSozinho(player))
        {
            handler->SendErrorMessage("A escolha de faccao so vale para personagens elegiveis ate o nivel {}.", config.nivelMaximoEscolha);
            return false;
        }
        MostrarMenu(player);
        return true;
    }

    static bool HandleDefinir(ChatHandler* handler, PlayerIdentifier alvo, std::string_view texto)
    {
        ObjectGuid guid = alvo.GetGUID();
        CharacterCacheEntry const* cache = sCharacterCache->GetCharacterCacheByGuid(guid);
        if (!cache)
        {
            handler->SendErrorMessage("Personagem nao encontrado.");
            return false;
        }

        TeamId nativa = Player::TeamIdForRace(cache->Race);
        TeamId equipe = nativa;
        if (texto != "nativa")
        {
            Optional<TeamId> lida = LerEquipe(texto);
            if (!lida)
            {
                handler->SendErrorMessage("Use horda, alianca ou nativa.");
                return false;
            }
            equipe = *lida;
        }

        if (Player* player = alvo.GetConnectedPlayer())
        {
            DefinirEquipe(player, equipe, true);
            handler->PSendSysMessage("{} agora joga pela {}.", alvo.GetName(), NomeEquipe(player->GetTeamId()));
            return true;
        }

        GravarEscolha(guid.GetCounter(), equipe, nativa, true);
        if (equipe == nativa)
            handler->PSendSysMessage("{} volta para a {}. Reputacao, voos e lar sao refeitos quando a escolha for reaplicada online.", alvo.GetName(), NomeEquipe(nativa));
        else
            handler->PSendSysMessage("{} vai jogar pela {} a partir do proximo login.", alvo.GetName(), NomeEquipe(equipe));
        return true;
    }

    static bool HandleVer(ChatHandler* handler, PlayerIdentifier alvo)
    {
        CharacterCacheEntry const* cache = sCharacterCache->GetCharacterCacheByGuid(alvo.GetGUID());
        if (!cache)
        {
            handler->SendErrorMessage("Personagem nao encontrado.");
            return false;
        }
        Optional<Escolha> escolha = BuscarEscolha(alvo.GetGUID().GetCounter());
        handler->PSendSysMessage("{}: raca {}, faccao nativa {}, faccao escolhida {}{}.", alvo.GetName(), uint32(cache->Race),
            NomeEquipe(Player::TeamIdForRace(cache->Race)), escolha ? NomeEquipe(escolha->equipe) : "nenhuma",
            escolha && escolha->aplicarNoLogin ? " (pendente para o proximo login)" : "");
        if (Player* player = alvo.GetConnectedPlayer())
            handler->PSendSysMessage("Online: equipe {}, faction template {}, raca de faccao {}, mascara {}.",
                NomeEquipe(player->GetTeamId()), player->GetFaction(), uint32(player->GetFactionRace()), player->GetFactionRaceMask());
        return true;
    }
};
}

void AddFaccaoPersonagemScripts()
{
    new FaccaoPersonagemWorld();
    new FaccaoPersonagemPlayer();
    new FaccaoPersonagemComandos();
}
