using UnrealBuildTool;

public class UnrealBridge : ModuleRules
{
	public UnrealBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking",
			"Json",
			"JsonUtilities",
			"PythonScriptPlugin",
			"DeveloperSettings",
			"BlueprintGraph",
			"KismetCompiler",
			"UMG",
			"AssetRegistry",
			"Kismet",
			"GraphEditor",
			"UMGEditor",
			"AnimGraph",
			"UnrealEd",
			"EditorSubsystem",
			"MovieScene",
			"AnimGraphRuntime",
			"ContentBrowser",
			"ContentBrowserData",
			"LevelEditor",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"MainFrame",
			"NavigationSystem",
			"Navmesh",
			"EnhancedInput",
			"Projects",
			"Slate",
			"SlateCore",
			"InputCore",
			"SourceControl",
			"ImageCore",
			"ImageWrapper",
			"RenderCore",
			"RHI",
		});

		// Live Coding is a Windows-only editor module. Guard the dep so
		// non-Windows builds of this editor plugin don't fail to link.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("LiveCoding");
		}
	}
}
