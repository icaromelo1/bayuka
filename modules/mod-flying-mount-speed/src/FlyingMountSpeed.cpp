#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"

#include <algorithm>

namespace
{
float FlyingMountMinimumPercent = 310.0f;

class FlyingMountSpeedWorldScript : public WorldScript
{
public:
    FlyingMountSpeedWorldScript() : WorldScript("FlyingMountSpeedWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        FlyingMountMinimumPercent = std::clamp(
            sConfigMgr->GetOption<float>("FlyingMountSpeed.MinimumPercent", 310.0f), 0.0f, 1000.0f);
    }
};

class FlyingMountSpeedUnitScript : public UnitScript
{
public:
    FlyingMountSpeedUnitScript() : UnitScript("FlyingMountSpeedUnitScript", true, { UNITHOOK_ON_AURA_APPLY }) { }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (!unit->IsPlayer())
            return;

        for (uint8 effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; ++effectIndex)
        {
            AuraEffect* effect = aura->GetEffect(effectIndex);
            if (!effect || effect->GetAuraType() != SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED ||
                effect->GetAmount() >= FlyingMountMinimumPercent)
                continue;

            effect->ChangeAmount(static_cast<int32>(FlyingMountMinimumPercent));
        }
    }
};
}

void AddFlyingMountSpeedScripts()
{
    new FlyingMountSpeedWorldScript();
    new FlyingMountSpeedUnitScript();
}
