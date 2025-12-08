// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCPServer : ModuleRules
{
	public UnrealMCPServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd", // For ImportObjectProperties and editor functionality
				"Slate",
				"SlateCore",
				"HTTPServer", // For HTTP server functionalities
				"Json", // For FJsonObject
				"JsonUtilities", // For FJsonObjectConverter
				"HTTP",
				"AssetRegistry", // For Blueprint search functionality
				"BlueprintGraph", // For Blueprint graph analysis
				"AssetTools" // For UFactory and asset import functionality
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
