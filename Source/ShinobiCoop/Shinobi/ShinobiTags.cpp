#include "Shinobi/ShinobiTags.h"

namespace ShinobiTags
{
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_ChakraGain, "Data.ChakraGain");
	UE_DEFINE_GAMEPLAY_TAG(Data_Cooldown, "Data.Cooldown");

	UE_DEFINE_GAMEPLAY_TAG(Event_Hit_Stagger, "Event.Hit.Stagger");
	UE_DEFINE_GAMEPLAY_TAG(Event_Substitution, "Event.Substitution");

	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_StaminaRegenBlocked, "State.StaminaRegenBlocked");
	UE_DEFINE_GAMEPLAY_TAG(State_Sprinting, "State.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_ChakraCharging, "State.ChakraCharging");
	UE_DEFINE_GAMEPLAY_TAG(State_Dodging, "State.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Burning, "State.Burning");
	UE_DEFINE_GAMEPLAY_TAG(State_Burning_Normal, "State.Burning.Normal");
	UE_DEFINE_GAMEPLAY_TAG(State_Burning_Black, "State.Burning.Black");
	UE_DEFINE_GAMEPLAY_TAG(State_Shocked, "State.Shocked");
	UE_DEFINE_GAMEPLAY_TAG(State_ShockBonus, "State.ShockBonus");
	UE_DEFINE_GAMEPLAY_TAG(State_Staggered, "State.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Downed, "State.Downed");
	UE_DEFINE_GAMEPLAY_TAG(State_Reviving, "State.Reviving");
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Casting_Fireball, "State.Casting.Fireball");
	UE_DEFINE_GAMEPLAY_TAG(State_Casting_Chidori, "State.Casting.Chidori");
	UE_DEFINE_GAMEPLAY_TAG(State_Casting_Rasengan, "State.Casting.Rasengan");
	UE_DEFINE_GAMEPLAY_TAG(State_Casting_Amaterasu, "State.Casting.Amaterasu");
	UE_DEFINE_GAMEPLAY_TAG(State_Casting_Charging, "State.Casting.Charging");
	UE_DEFINE_GAMEPLAY_TAG(State_Phased, "State.Phased");
	UE_DEFINE_GAMEPLAY_TAG(State_Susanoo, "State.Susanoo");
	UE_DEFINE_GAMEPLAY_TAG(State_DragonBreathing, "State.DragonBreathing");
	UE_DEFINE_GAMEPLAY_TAG(State_Mounted, "State.Mounted");
	UE_DEFINE_GAMEPLAY_TAG(State_SummonAttacking, "State.SummonAttacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Chilled, "State.Chilled");
	UE_DEFINE_GAMEPLAY_TAG(State_TeleportMarked, "State.TeleportMarked");
	UE_DEFINE_GAMEPLAY_TAG(State_PvpHold, "State.PvpHold");
	UE_DEFINE_GAMEPLAY_TAG(State_AttackBoost, "State.AttackBoost"); // these three names are also looked up by the HUD's boost list
	UE_DEFINE_GAMEPLAY_TAG(State_SpeedBoost, "State.SpeedBoost");
	UE_DEFINE_GAMEPLAY_TAG(State_Shield, "State.Shield");
	UE_DEFINE_GAMEPLAY_TAG(State_Blocking, "State.Blocking");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Sprint, "Ability.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ChakraCharge, "Ability.ChakraCharge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Jutsu, "Ability.Jutsu");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Substitution, "Ability.Substitution");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Revive, "Ability.Revive");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Block, "Ability.Block");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Fireball, "Cooldown.Jutsu.Fireball");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Chidori, "Cooldown.Jutsu.Chidori");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Rasengan, "Cooldown.Jutsu.Rasengan");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Amaterasu, "Cooldown.Jutsu.Amaterasu");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Teleport, "Cooldown.Jutsu.Teleport");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Kamui, "Cooldown.Jutsu.Kamui");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_AlmightyPush, "Cooldown.Jutsu.AlmightyPush");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_EarthWall, "Cooldown.Jutsu.EarthWall");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_WaterWall, "Cooldown.Jutsu.WaterWall");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_FireMeteor, "Cooldown.Jutsu.FireMeteor");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_IceMeteor, "Cooldown.Jutsu.IceMeteor");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_WaterDragon, "Cooldown.Jutsu.WaterDragon");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_Susanoo, "Cooldown.Jutsu.Susanoo");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonDragon, "Cooldown.Jutsu.SummonDragon");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonCrimsonDragon, "Cooldown.Jutsu.SummonCrimsonDragon");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonOwl, "Cooldown.Jutsu.SummonOwl");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonMisha, "Cooldown.Jutsu.SummonMisha");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonPabu, "Cooldown.Jutsu.SummonPabu");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonPusho, "Cooldown.Jutsu.SummonPusho");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_DragonBreath, "Cooldown.DragonBreath");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Substitution, "Cooldown.Substitution");

	UE_DEFINE_GAMEPLAY_TAG(Fail_NoTarget, "Fail.NoTarget");
}
