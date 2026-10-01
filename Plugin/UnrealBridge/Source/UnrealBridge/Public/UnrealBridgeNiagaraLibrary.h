#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeNiagaraLibrary.generated.h"

/** Result shared by Niagara authoring operations. */
USTRUCT(BlueprintType)
struct FBridgeNiagaraOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString AssetPath;

	/** Emitter handle, module node, renderer object, or preview handle. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	TArray<FString> Warnings;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraTemplateInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString AssetPath;

	/** System or Emitter. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString AssetType;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	TArray<FString> Tags;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraScriptInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Keywords;

	/** Module, DynamicInput, Function, or another reflected Niagara usage. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Usage;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 MajorVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 MinorVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bLibraryVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bDeprecated = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bExperimental = false;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraSystemInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bReadyToRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bDirty = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bFixedBounds = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FBox FixedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	float WarmupTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	float WarmupTickDelta = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 WarmupTickCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 EmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 EnabledEmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 ModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 RendererCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 UserParameterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString EffectTypePath;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraEmitterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString SourceAssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bLocalSpace = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bDeterministic = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 RandomSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString SimTarget;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString InterpolatedSpawnMode;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bFixedBounds = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FBox FixedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 ModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 RendererCount = 0;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraModuleInfo
{
	GENERATED_BODY()

	/** Opaque scoped identifier (emitter GUID/node GUID, or system/node GUID). Use unchanged in module calls.
	 * 发射器/系统作用域内的节点标识；原样传入模块操作，避免复制发射器后节点 GUID 重复。 */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString EmitterId;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString EmitterName;

	/** EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate, or another Niagara usage. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Usage;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString UsageId;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString ScriptPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString VersionId;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bAssignment = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bDeprecated = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 InputCount = 0;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraModuleInputInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString ModuleId;

	/** Unaliased module input name, for example SpawnRate or Lifetime.Min. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Type;

	/** Default, Local, Linked, Dynamic, DataInterface, or Object. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Mode;

	/** Export-text value for Local/Default, or target path/name for another mode. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bStatic = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bHidden = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString VariableId;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraRendererInfo
{
	GENERATED_BODY()

	/** Renderer UObject name, used by renderer mutation calls. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString EmitterId;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString MaterialPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	TArray<FString> MaterialPaths;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	TArray<FString> MeshPaths;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 BindingCount = 0;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraPropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bEditable = false;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Value;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bUserParameter = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bDataInterface = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bObject = false;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraCompileMessage
{
	GENERATED_BODY()

	/** Log, Display, Warning, Error, or Validation. */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Severity;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString ShortDescription;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString EmitterId;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString ScriptUsage;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString PinId;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraCompileResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bReadyToRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 ErrorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 WarningCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraCompileMessage> Messages;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Error;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraAuditResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bPassed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 ErrorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 WarningCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 EmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 ModuleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 RendererCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 GpuEmitterCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 TranslucentRendererCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraCompileMessage> Issues;
};

/** Generic name/value entry used by renderer and data-interface recipes. */
USTRUCT(BlueprintType)
struct FBridgeNiagaraPropertyValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Name;

	/** Unreal export-text value. Object properties also accept a content path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Value;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraInputValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Name;

	/** Local, Linked, Dynamic, Object, or DataInterface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Mode = TEXT("Local");

	/** Optional type override. Normally inferred from the module input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Type;

	/** Export-text local value, linked parameter name, or object asset path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Value;

	/** Dynamic input script path or data-interface class path. Configure nested inputs with a later module-input call. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString SourcePath;

	/** Optional reflected properties applied to a newly created data-interface input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraPropertyValue> Properties;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraModuleSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString ScriptPath;

	/** EmitterSpawn, EmitterUpdate, ParticleSpawn, or ParticleUpdate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Usage = TEXT("ParticleUpdate");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	int32 Index = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraInputValue> Inputs;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraRendererSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Name;

	/** Sprite, Ribbon, Mesh, Light, Decal, or Component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Type = TEXT("Sprite");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString MaterialPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString MeshPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraPropertyValue> Properties;

	/** Renderer binding property name -> Niagara variable name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraPropertyValue> Bindings;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraEmitterSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Name = TEXT("Emitter");

	/** Optional emitter template/asset. Empty creates a standard initialized emitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString TemplatePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	bool bLocalSpace = false;

	/** CPU or GPU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString SimTarget = TEXT("CPU");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	bool bUseFixedBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FBox FixedBounds = FBox(FVector(-500.0), FVector(500.0));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraModuleSpec> Modules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraRendererSpec> Renderers;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraParameterSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString Type = TEXT("Float");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString DefaultValue;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraSystemRecipe
{
	GENERATED_BODY()

	/** Optional system template to duplicate before applying the recipe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString TemplatePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	float WarmupTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	float WarmupTickDelta = 1.0f / 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	bool bUseFixedBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FBox FixedBounds = FBox(FVector(-1000.0), FVector(1000.0));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	FString EffectTypePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraParameterSpec> UserParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraEmitterSpec> Emitters;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraEmitterRuntimeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString ExecutionState;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 ParticleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int64 BytesUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	float CpuTimeMs = 0.0f;
};

USTRUCT(BlueprintType)
struct FBridgeNiagaraPreviewInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString Handle;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FString SystemPath;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	FTransform Transform;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	bool bComplete = false;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	float DesiredAge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int32 TotalParticleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	int64 TotalBytesUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|Niagara")
	TArray<FBridgeNiagaraEmitterRuntimeInfo> Emitters;
};

/**
 * Niagara/VFX authoring, diagnostics, preset delivery, and transient preview.
 * The functional implementation targets UE 5.7+; older supported engines
 * expose the same calls as logged safe stubs so the plugin still builds.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeNiagaraLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara")
	static bool IsNiagaraApiAvailable();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara")
	static FString GetLastNiagaraError();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Discovery")
	static TArray<FBridgeNiagaraTemplateInfo> ListNiagaraTemplates(const FString& AssetType = TEXT("All"), const FString& Query = TEXT(""), int32 MaxResults = 200);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Discovery")
	static TArray<FBridgeNiagaraScriptInfo> ListNiagaraScripts(const FString& Usage = TEXT("Module"), const FString& Query = TEXT(""), int32 MaxResults = 500);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Discovery")
	static FBridgeNiagaraScriptInfo GetNiagaraScriptInfo(const FString& ScriptPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Assets")
	static FBridgeNiagaraOperationResult CreateNiagaraSystem(const FString& AssetPath, const FString& TemplateSystemPath = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Assets")
	static FBridgeNiagaraOperationResult CreateNiagaraEmitter(const FString& AssetPath, const FString& TemplateEmitterPath = TEXT(""), bool bAddDefaultModulesAndRenderer = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Assets")
	static FBridgeNiagaraOperationResult CreateNiagaraSystemFromRecipe(const FString& AssetPath, const FBridgeNiagaraSystemRecipe& Recipe, bool bCompile = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Assets")
	static FBridgeNiagaraOperationResult DeleteNiagaraAsset(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Assets")
	static FBridgeNiagaraSystemInfo GetNiagaraSystemInfo(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Emitters")
	static TArray<FBridgeNiagaraEmitterInfo> ListNiagaraEmitters(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Emitters")
	static FBridgeNiagaraOperationResult AddNiagaraEmitter(const FString& SystemPath, const FString& Name, const FString& EmitterAssetOrTemplatePath = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Emitters")
	static FBridgeNiagaraOperationResult DuplicateNiagaraEmitter(const FString& SystemPath, const FString& EmitterIdOrName, const FString& NewName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Emitters")
	static bool RemoveNiagaraEmitter(const FString& SystemPath, const FString& EmitterIdOrName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Emitters")
	static bool RenameNiagaraEmitter(const FString& SystemPath, const FString& EmitterIdOrName, const FString& NewName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Emitters")
	static bool SetNiagaraEmitterEnabled(const FString& SystemPath, const FString& EmitterIdOrName, bool bEnabled, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Emitters")
	static bool SetNiagaraEmitterProperties(const FString& SystemPath, const FString& EmitterIdOrName, bool bLocalSpace, const FString& SimTarget, bool bDeterministic, int32 RandomSeed, bool bUseFixedBounds, const FBox& FixedBounds, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static TArray<FBridgeNiagaraModuleInfo> ListNiagaraModules(const FString& SystemPath, const FString& EmitterIdOrName = TEXT(""), const FString& Usage = TEXT("All"));

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static TArray<FBridgeNiagaraModuleInputInfo> ListNiagaraModuleInputs(const FString& SystemPath, const FString& ModuleId, bool bIncludeHidden = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static FBridgeNiagaraOperationResult AddNiagaraModule(const FString& SystemPath, const FString& EmitterIdOrName, const FString& Usage, const FString& ScriptPath, const FString& SuggestedName = TEXT(""), int32 Index = -1, bool bEnabled = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static bool RemoveNiagaraModule(const FString& SystemPath, const FString& ModuleId, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static bool SetNiagaraModuleEnabled(const FString& SystemPath, const FString& ModuleId, bool bEnabled, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static bool SetNiagaraModuleInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static bool LinkNiagaraModuleInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& LinkedParameter, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static FBridgeNiagaraOperationResult SetNiagaraModuleDynamicInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& DynamicInputScriptPath, const FString& SuggestedName = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static bool SetNiagaraModuleObjectInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& ObjectPath, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static FBridgeNiagaraOperationResult SetNiagaraModuleDataInterfaceInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& DataInterfaceClassPath, const TArray<FBridgeNiagaraPropertyValue>& Properties, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static TArray<FBridgeNiagaraPropertyInfo> ListNiagaraModuleInputObjectProperties(const FString& SystemPath, const FString& ModuleId, const FString& InputName, bool bIncludeAdvanced = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static bool SetNiagaraModuleInputObjectProperty(const FString& SystemPath, const FString& ModuleId, const FString& InputName, const FString& PropertyName, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static bool ResetNiagaraModuleInput(const FString& SystemPath, const FString& ModuleId, const FString& InputName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Modules")
	static FBridgeNiagaraOperationResult AddNiagaraParameterAssignment(const FString& SystemPath, const FString& EmitterIdOrName, const FString& Usage, const FString& ParameterName, const FString& Type, const FString& Value, int32 Index = -1, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Parameters")
	static TArray<FBridgeNiagaraParameterInfo> ListNiagaraUserParameters(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Parameters")
	static bool AddNiagaraUserParameter(const FString& SystemPath, const FString& Name, const FString& Type, const FString& DefaultValue, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Parameters")
	static bool SetNiagaraUserParameterDefault(const FString& SystemPath, const FString& Name, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Parameters")
	static bool RenameNiagaraUserParameter(const FString& SystemPath, const FString& OldName, const FString& NewName, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Parameters")
	static bool RemoveNiagaraUserParameter(const FString& SystemPath, const FString& Name, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static TArray<FBridgeNiagaraRendererInfo> ListNiagaraRenderers(const FString& SystemPath, const FString& EmitterIdOrName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static FBridgeNiagaraOperationResult AddNiagaraRenderer(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererType, const FString& Name = TEXT(""), const FString& MaterialPath = TEXT(""), const FString& MeshPath = TEXT(""), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static bool RemoveNiagaraRenderer(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static bool SetNiagaraRendererEnabled(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, bool bEnabled, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static TArray<FBridgeNiagaraPropertyInfo> ListNiagaraRendererProperties(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, bool bIncludeAdvanced = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static FString GetNiagaraRendererProperty(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& PropertyName);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static bool SetNiagaraRendererProperty(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& PropertyName, const FString& Value, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static bool SetNiagaraRendererMaterial(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& MaterialPath, int32 MaterialIndex = 0, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Renderers")
	static bool SetNiagaraRendererBinding(const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, const FString& BindingProperty, const FString& VariableName, const FString& SourceMode = TEXT("Particles"), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Settings")
	static bool SetNiagaraSystemWarmup(const FString& SystemPath, float WarmupTime, float TickDelta = 0.033333333f, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Settings")
	static bool SetNiagaraSystemFixedBounds(const FString& SystemPath, bool bEnabled, const FBox& Bounds, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Settings")
	static bool SetNiagaraSystemEffectType(const FString& SystemPath, const FString& EffectTypePath, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Compile")
	static FBridgeNiagaraCompileResult CompileNiagaraSystem(const FString& SystemPath, bool bForce = true, bool bWaitForGpuShaders = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Compile")
	static FBridgeNiagaraCompileResult GetNiagaraCompileDiagnostics(const FString& SystemPath);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Compile")
	static FBridgeNiagaraAuditResult ValidateNiagaraSystem(const FString& SystemPath, bool bCheckMaterials = true, bool bCheckBounds = true, int32 MaxEmitters = 16, int32 MaxRenderersPerEmitter = 8, int32 MaxModulesPerEmitter = 64);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Presets")
	static FBridgeNiagaraOperationResult CreateWeaponTrailEffect(const FString& AssetPath, const FString& Style = TEXT("Ribbon"), const FString& MaterialPath = TEXT(""), const FLinearColor& Color = FLinearColor(1.0f, 0.35f, 0.05f, 1.0f), float Width = 12.0f, float Lifetime = 0.35f, float SpawnRate = 90.0f, bool bLocalSpace = false, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Presets")
	static FBridgeNiagaraOperationResult CreateSparkEffect(const FString& AssetPath, const FString& Style = TEXT("Directional"), const FString& MaterialPath = TEXT(""), const FLinearColor& Color = FLinearColor(1.0f, 0.45f, 0.05f, 1.0f), int32 Count = 48, float Speed = 900.0f, float Lifetime = 0.6f, float Gravity = -980.0f, bool bCollision = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Presets")
	static FBridgeNiagaraOperationResult CreateExplosionEffect(const FString& AssetPath, const FString& Style = TEXT("Layered"), const FString& MaterialPath = TEXT(""), const FLinearColor& CoreColor = FLinearColor(1.0f, 0.12f, 0.01f, 1.0f), float Scale = 1.0f, float Duration = 1.5f, int32 DebrisCount = 64, bool bShockwave = true, bool bLight = true, bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Presets")
	static FBridgeNiagaraOperationResult CreateDissolveEffect(const FString& AssetPath, const FString& Style = TEXT("Ash"), const FString& MaterialPath = TEXT(""), const FLinearColor& Color = FLinearColor(0.08f, 0.8f, 1.0f, 1.0f), int32 Count = 128, float Duration = 2.0f, float Radius = 100.0f, const FVector& Direction = FVector(0.0f, 0.0f, 1.0f), bool bSave = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static FBridgeNiagaraOperationResult SpawnNiagaraPreview(const FString& SystemPath, const FTransform& Transform, bool bAutoActivate = true, bool bResetOnChange = true);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static TArray<FBridgeNiagaraPreviewInfo> ListNiagaraPreviews();

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static FBridgeNiagaraPreviewInfo GetNiagaraPreviewInfo(const FString& Handle);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static bool AdvanceNiagaraPreview(const FString& Handle, float Seconds, float TickDelta = 0.016666667f);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static bool SetNiagaraPreviewTransform(const FString& Handle, const FTransform& Transform, bool bTeleport = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static bool SetNiagaraPreviewVariable(const FString& Handle, const FString& Name, const FString& Type, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static bool ControlNiagaraPreview(const FString& Handle, const FString& Action);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static bool RemoveNiagaraPreview(const FString& Handle);

	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|Niagara|Preview")
	static int32 RemoveAllNiagaraPreviews();
};
