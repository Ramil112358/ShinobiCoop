// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShinobiCoop : ModuleRules
{
	public ShinobiCoop(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"IKRig",
			"Landscape",
			"ImageCore",
			"NavigationSystem",
			"RenderCore",
			"RHI"
		});

		PublicIncludePaths.AddRange(new string[] {
			"ShinobiCoop",
			"ShinobiCoop/Variant_Combat",
			"ShinobiCoop/Variant_Combat/AI",
			"ShinobiCoop/Variant_Combat/Animation",
			"ShinobiCoop/Variant_Combat/Gameplay",
			"ShinobiCoop/Variant_Combat/Interfaces",
			"ShinobiCoop/Variant_Combat/UI"
		});
	}
}
