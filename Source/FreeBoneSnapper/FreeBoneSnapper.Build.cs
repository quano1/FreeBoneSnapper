// Copyright longlt00502@gmail.com 2023-2025. All rights reserved.

using UnrealBuildTool;

public class FreeBoneSnapper : ModuleRules
{
	public FreeBoneSnapper(ReadOnlyTargetRules Target) : base(Target)
	{
				PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"EditorSubsystem",
				"DeveloperSettings",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"Persona",
				"Core",
				"ToolWidgets",
				"MessageLog",
				"IKRig",
				"IKRigEditor",
			}
        );
	}
}
