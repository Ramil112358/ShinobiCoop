#include "Shinobi/ShinobiPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"

AShinobiPlayerCharacter::AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StartupAbilities = { UGA_Sprint::StaticClass(), UGA_Dodge::StaticClass(), UGA_ChakraCharge::StaticClass() };
}
