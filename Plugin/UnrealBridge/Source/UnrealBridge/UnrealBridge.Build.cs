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
			"UMGEditor",
			"AnimGraph",
			"UnrealEd",
			"MovieScene",
			"AnimGraphRuntime",
			"ContentBrowser",
			"ContentBrowserData",
			"LevelEditor",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"MainFrame",
		});
	}
}
