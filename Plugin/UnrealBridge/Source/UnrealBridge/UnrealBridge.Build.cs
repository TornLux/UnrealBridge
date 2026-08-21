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
			"ApplicationCore",
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
			"MovieSceneTracks",
			"AnimGraphRuntime",
			"AnimationCore",
			"AnimationBlueprintLibrary",
			"AnimationModifiers",
			"ContentBrowser",
			"ContentBrowserData",
			"LevelEditor",
			"GameplayAbilities",
			"GameplayAbilitiesEditor",
			"GameplayTags",
			"GameplayTagsEditor",
			"GameplayTasks",
			"GameplayTasksEditor",
			"MainFrame",
			"NavigationSystem",
			"Navmesh",
			"Landscape",
			"EnhancedInput",
			"InputBlueprintNodes",
			"InputEditor",
			"Projects",
			"Slate",
			"SlateCore",
			"InputCore",
			"SourceControl",
			"ImageCore",
			"ImageWrapper",
			"RenderCore",
			"RHI",
			"MaterialEditor",
			"PoseSearch",
			"Chooser",
			"ChooserEditor",
			"StructUtils",
			"PropertyBindingUtils",
			"StateTreeModule",
			"StateTreeEditorModule",
			"GameplayStateTreeModule",
			"AssetTools",
			"EditorScriptingUtilities",
			"MotionWarping",
			// Geometry Script — Lane 2 of the procedural-content roadmap
			// (UnrealBridgeGeometryLibrary). UDynamicMesh + the runtime BP
			// function libs (CopyMeshFromStaticMesh, ApplyMeshBoolean, etc.)
			// live in GeometryScriptingCore / GeometryFramework; the editor-
			// only asset-creation lib (CreateNewStaticMeshAssetFromMesh) is
			// in GeometryScriptingEditor — UnrealBridge is editor-only so
			// linking the editor module is fine.
			"GeometryScriptingCore",
			"GeometryFramework",
			"GeometryScriptingEditor",
			// PCG — Lane 3 of the procedural-content roadmap. Read-only +
			// trigger only (we do not edit PCG graphs — see roadmap §5/§8).
			"PCG",
			// TraceLog hosts UE::Trace::EnumerateChannels (used by M4-4
			// list_trace_channels). Core publicly forwards TraceLog headers
			// but the symbols are __declspec(dllimport) so a direct link
			// dep is required. TraceLog.Build.cs sets
			// bRequiresImplementModule=false, so this only pulls the link
			// import — no extra runtime cost.
			"TraceLog",
			// TraceServices — perf-capability M4-5 ParseTraceToSummary.
			// Loads + analyses .utrace files via IAnalysisService::Analyze
			// (synchronous). Pulls in TraceAnalysis transitively.
			"TraceServices",
		});

		// Live Coding is a Windows-only editor module. Guard the dep so
		// non-Windows builds of this editor plugin don't fail to link.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("LiveCoding");
		}

		// SmartObjects first ships in the supported engine matrix after 5.3,
		// while UnrealBridge's authoring/runtime API intentionally targets the
		// stable UE 5.7 surface. Older engines compile the generated safe stubs.
		if (Target.Version.MajorVersion > 5
			|| (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 7))
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				// Niagara authoring and compile diagnostics. The public API is kept
				// build-safe on older engines through generated reflected stubs.
				"Niagara",
				"NiagaraCore",
				"NiagaraEditor",
				"NiagaraShader",
				// Rig authoring. The editor-facing controllers are intentionally
				// version-gated because their data models changed substantially in 5.6.
				"ControlRig",
				"ControlRigDeveloper",
				"ControlRigEditor",
				"RigVM",
				"RigVMDeveloper",
				"RigVMEditor",
				"IKRig",
				"IKRigEditor",
				"SmartObjectsModule",
				"SmartObjectsEditorModule",
				"WorldConditions",
				"FieldNotification",
				"ModelViewViewModel",
				"ModelViewViewModelBlueprint",
			});
		}
	}
}
