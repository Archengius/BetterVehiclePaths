// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BetterVehiclePaths : ModuleRules
{
	public BetterVehiclePaths(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		
		PublicDependencyModuleNames.AddRange( new string[] { 
			"Core", 
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"EnhancedInput",
			"DeveloperSettings",
			"FactoryGame"
		} );
		PrivateDependencyModuleNames.AddRange(new string[] {});
	}
}
