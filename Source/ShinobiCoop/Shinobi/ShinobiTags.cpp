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
	UE_DEFINE_GAMEPLAY_TAG(State_Mounted, "State.Mounted");
	UE_DEFINE_GAMEPLAY_TAG(State_SummonAttacking, "State.SummonAttacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Chilled, "State.Chilled");
	UE_DEFINE_GAMEPLAY_TAG(State_TeleportMarked, "State.TeleportMarked");
	UE_DEFINE_GAMEPLAY_TAG(State_JutsuPage_1, "State.JutsuPage.1");
	UE_DEFINE_GAMEPLAY_TAG(State_JutsuPage_2, "State.JutsuPage.2");
	UE_DEFINE_GAMEPLAY_TAG(State_JutsuPage_3, "State.JutsuPage.3");
	UE_DEFINE_GAMEPLAY_TAG(State_JutsuPage_4, "State.JutsuPage.4");
	UE_DEFINE_GAMEPLAY_TAG(State_JutsuPage_5, "State.JutsuPage.5");

	FGameplayTag JutsuPage(int32 Page)
	{
		const FNativeGameplayTag* Pages[NumJutsuPages] = { &State_JutsuPage_1, &State_JutsuPage_2, &State_JutsuPage_3, &State_JutsuPage_4, &State_JutsuPage_5 };
		return Pages[FMath::Clamp(Page, 1, NumJutsuPages) - 1]->GetTag();
	}

	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee, "Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge, "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Sprint, "Ability.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ChakraCharge, "Ability.ChakraCharge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Jutsu, "Ability.Jutsu");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Substitution, "Ability.Substitution");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Revive, "Ability.Revive");

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
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonToad, "Cooldown.Jutsu.SummonToad");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonSnake, "Cooldown.Jutsu.SummonSnake");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Jutsu_SummonDogs, "Cooldown.Jutsu.SummonDogs");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_DragonBreath, "Cooldown.DragonBreath");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Substitution, "Cooldown.Substitution");

	UE_DEFINE_GAMEPLAY_TAG(Fail_NoTarget, "Fail.NoTarget");
}
