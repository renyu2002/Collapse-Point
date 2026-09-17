// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Game : ModuleRules
{
	public Game(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json"
		});

		PublicIncludePaths.AddRange(new string[] {
			"Game",
			"Game/CollapsePoint",
			"Game/Variant_Platforming",
			"Game/Variant_Platforming/Animation",
			"Game/Variant_Combat",
			"Game/Variant_Combat/AI",
			"Game/Variant_Combat/Animation",
			"Game/Variant_Combat/Gameplay",
			"Game/Variant_Combat/Interfaces",
			"Game/Variant_Combat/UI",
			"Game/Variant_SideScrolling",
			"Game/Variant_SideScrolling/AI",
			"Game/Variant_SideScrolling/Gameplay",
			"Game/Variant_SideScrolling/Interfaces",
			"Game/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
