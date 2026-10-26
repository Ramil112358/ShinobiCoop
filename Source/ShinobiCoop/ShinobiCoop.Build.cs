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
			"RigLogicModule", // face sliders run RigLogic natively (ShinobiFaceShapeAnimInstance)
			"AnimGraphRuntime",
			"HairStrandsCore", // creator looks: grooms bound to the picked face
			"AnimationCore",
			"Landscape",
			"ImageCore",
			"NavigationSystem",
			"RenderCore",
			"RHI",
			"MeshDescription",
			"StaticMeshDescription",
			"SkeletalMeshDescription", // creator: face blend morphs baked into mesh descriptions (ShinobiFaceBlendLibrary)
			"MotionWarping",
			"Chooser",
			"AnimationWarpingRuntime",
			"PoseSearch",
			"PhysicsControl",
			"ClothingSystemRuntimeInterface", // outfit cloth: simulation data and interactors
			"ClothingSystemRuntimeCommon", // ...and the clothing assets (tests read their painted maps)
			"ChaosCloth" // ...and each cloth's own gravity (wall-runs)
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("MeshUtilities"); // world builder: skeletal to static mesh (editor only)
			// outfit builder: Chaos cloth on the fitted copies (editor only)
			PrivateDependencyModuleNames.AddRange(new string[] { "ClothingSystemEditorInterface", "SkeletalMeshEditor", "PhysicsUtilities", "AssetRegistry" });
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
