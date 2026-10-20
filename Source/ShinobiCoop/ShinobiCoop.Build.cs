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
			"AnimationCore",
			"Landscape",
			"ImageCore",
			"NavigationSystem",
			"RenderCore",
			"RHI",
			"MeshDescription",
			"StaticMeshDescription",
			"MotionWarping",
			"Chooser",
			"AnimationWarpingRuntime",
			"PoseSearch",
			"PhysicsControl",
			"ClothingSystemRuntimeInterface" // outfit cloth: simulation data and interactors
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("MeshUtilities"); // world builder: skeletal to static mesh (editor only)
			// outfit builder: Chaos cloth on the fitted copies (editor only)
			PrivateDependencyModuleNames.AddRange(new string[] { "ClothingSystemEditorInterface", "ClothingSystemRuntimeCommon", "SkeletalMeshEditor", "ChaosCloth" });
		}

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
