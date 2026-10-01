#include "UnrealBridgeNiagaraLibrary.h"
#include "Misc/EngineVersionComparison.h"

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/PackageName.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentRendererProperties.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraEffectType.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterBase.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraGraph.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraMeshRendererMeshProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraShared.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraTypeRegistry.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "EdGraphSchema_Niagara.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeNiagaraLibrary"

namespace BridgeNiagaraImpl
{

static FString LastError;
static TMap<FString, TWeakObjectPtr<UNiagaraComponent>> PreviewComponents;

static void ClearError()
{
	LastError.Reset();
}

static bool SetError(const FString& Error)
{
	LastError = Error;
	UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Niagara: %s"), *Error);
	return false;
}

static FString NormalizeToken(FString Value)
{
	Value.TrimStartAndEndInline();
	Value.ReplaceInline(TEXT("_"), TEXT(""));
	Value.ReplaceInline(TEXT("-"), TEXT(""));
	Value.ReplaceInline(TEXT(" "), TEXT(""));
	Value.ReplaceInline(TEXT("::"), TEXT(""));
	Value.ReplaceInline(TEXT("."), TEXT(""));
	return Value.ToLower();
}

static FString NormalizeObjectPath(FString Path)
{
	Path.TrimStartAndEndInline();
	int32 FirstQuote = INDEX_NONE;
	if (Path.FindChar(TEXT('\''), FirstQuote))
	{
		const int32 LastQuote = Path.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastQuote > FirstQuote) Path = Path.Mid(FirstQuote + 1, LastQuote - FirstQuote - 1);
	}
	if (!Path.Contains(TEXT(".")))
	{
		FString AssetName;
		if (Path.Split(TEXT("/"), nullptr, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
			&& !AssetName.IsEmpty())
		{
			Path += TEXT(".") + AssetName;
		}
	}
	return Path;
}

static bool SplitAssetPath(const FString& InPath, FString& OutPackagePath, FString& OutAssetName, FString& OutPackageName)
{
	OutPackageName = InPath;
	OutPackageName.TrimStartAndEndInline();
	int32 Dot = INDEX_NONE;
	if (OutPackageName.FindChar(TEXT('.'), Dot)) OutPackageName = OutPackageName.Left(Dot);
	if (!OutPackageName.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(OutPackageName))
	{
		return SetError(FString::Printf(TEXT("'%s' is not a valid /Game asset path"), *InPath));
	}
	int32 Slash = INDEX_NONE;
	if (!OutPackageName.FindLastChar(TEXT('/'), Slash) || Slash <= 0 || Slash == OutPackageName.Len() - 1)
	{
		return SetError(FString::Printf(TEXT("asset path '%s' must include an asset name"), *InPath));
	}
	OutPackagePath = OutPackageName.Left(Slash);
	OutAssetName = OutPackageName.Mid(Slash + 1);
	return true;
}

static FString MakeObjectPath(const FString& PackageName, const FString& AssetName)
{
	return PackageName + TEXT(".") + AssetName;
}

template <typename T>
static T* LoadNiagaraAsset(const FString& AssetPath, const TCHAR* Kind)
{
	T* Asset = LoadObject<T>(nullptr, *NormalizeObjectPath(AssetPath));
	if (!Asset)
	{
		SetError(FString::Printf(TEXT("%s '%s' was not found or has the wrong type"), Kind, *AssetPath));
	}
	return Asset;
}

static bool EnsureAssetDoesNotExist(const FString& PackageName, const FString& AssetName)
{
	if (FindObject<UObject>(nullptr, *MakeObjectPath(PackageName, AssetName))
		|| FPackageName::DoesPackageExist(PackageName))
	{
		return SetError(FString::Printf(TEXT("asset '%s' already exists"), *MakeObjectPath(PackageName, AssetName)));
	}
	return true;
}

static bool SaveAsset(UObject* Asset)
{
	return Asset && UEditorLoadingAndSavingUtils::SavePackages({Asset->GetOutermost()}, false);
}

template <typename T>
static T* DuplicateAsset(const FString& SourcePath, const FString& AssetPath)
{
	T* Source = LoadNiagaraAsset<T>(SourcePath, TEXT("template asset"));
	if (!Source) return nullptr;
	FString PackagePath, AssetName, PackageName;
	if (!SplitAssetPath(AssetPath, PackagePath, AssetName, PackageName)
		|| !EnsureAssetDoesNotExist(PackageName, AssetName)) return nullptr;
	UPackage* Package = CreatePackage(*PackageName);
	T* Asset = Cast<T>(StaticDuplicateObject(Source, Package, FName(*AssetName), RF_AllFlags));
	if (!Asset)
	{
		SetError(FString::Printf(TEXT("failed to duplicate '%s' to '%s'"), *SourcePath, *AssetPath));
		return nullptr;
	}
	Asset->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Asset);
	Asset->MarkPackageDirty();
	return Asset;
}

static UNiagaraSystem* CreateBlankSystemAsset(const FString& AssetPath)
{
	FString PackagePath, AssetName, PackageName;
	if (!SplitAssetPath(AssetPath, PackagePath, AssetName, PackageName)
		|| !EnsureAssetDoesNotExist(PackageName, AssetName)) return nullptr;
	UPackage* Package = CreatePackage(*PackageName);
	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (!System)
	{
		SetError(FString::Printf(TEXT("failed to create Niagara System '%s'"), *AssetPath));
		return nullptr;
	}
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);
	FAssetRegistryModule::AssetCreated(System);
	System->MarkPackageDirty();
	return System;
}

static UNiagaraEmitter* CreateBlankEmitterAsset(const FString& AssetPath, bool bAddDefaults)
{
	FString PackagePath, AssetName, PackageName;
	if (!SplitAssetPath(AssetPath, PackagePath, AssetName, PackageName)
		|| !EnsureAssetDoesNotExist(PackageName, AssetName)) return nullptr;
	UPackage* Package = CreatePackage(*PackageName);
	UNiagaraEmitter* Emitter = NewObject<UNiagaraEmitter>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (!Emitter)
	{
		SetError(FString::Printf(TEXT("failed to create Niagara Emitter '%s'"), *AssetPath));
		return nullptr;
	}
	UNiagaraEmitterFactoryNew::InitializeEmitter(Emitter, bAddDefaults);
	FAssetRegistryModule::AssetCreated(Emitter);
	Emitter->MarkPackageDirty();
	return Emitter;
}

static bool FinishSystemMutation(UNiagaraSystem* System, bool bSave, bool bRequestCompile = true)
{
	if (!System) return false;
	System->MarkPackageDirty();
	if (bRequestCompile) System->RequestCompile(false);
	if (bSave && !SaveAsset(System))
	{
		return SetError(FString::Printf(TEXT("failed to save Niagara System '%s'"), *System->GetPathName()));
	}
	return true;
}

static void RemoveEmitterTopologyNodes(UNiagaraSystem* System, const FGuid& EmitterId)
{
	if (!System) return;
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(System->GetSystemSpawnScript()->GetLatestSource());
	UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
	if (Graph)
	{
		TArray<UEdGraphNode*> Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			if (!Node || Node->GetClass()->GetName() != TEXT("NiagaraNodeEmitter")) continue;
			FStructProperty* IdProperty = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("EmitterHandleId"));
			const FGuid* NodeEmitterId = IdProperty ? IdProperty->ContainerPtrToValuePtr<FGuid>(Node) : nullptr;
			if (!NodeEmitterId || *NodeEmitterId != EmitterId) continue;
			UEdGraphPin* InputPin = nullptr;
			UEdGraphPin* OutputPin = nullptr;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				if (!InputPin && Pin->Direction == EGPD_Input) InputPin = Pin;
				else if (!OutputPin && Pin->Direction == EGPD_Output) OutputPin = Pin;
			}
			UEdGraphPin* UpstreamPin = InputPin && InputPin->LinkedTo.Num() == 1 ? InputPin->LinkedTo[0] : nullptr;
			UEdGraphPin* DownstreamPin = OutputPin && OutputPin->LinkedTo.Num() == 1 ? OutputPin->LinkedTo[0] : nullptr;
			Node->Modify();
			Node->DestroyNode();
			if (UpstreamPin && DownstreamPin) UpstreamPin->MakeLinkTo(DownstreamPin);
		}
		Graph->NotifyGraphChanged();
	}
	if (UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
	{
		EditorData->SynchronizeOverviewGraphWithSystem(*System);
	}
}

static FNiagaraEmitterHandle* FindEmitterHandle(UNiagaraSystem* System, const FString& IdOrName)
{
	if (!System) return nullptr;
	FGuid WantedGuid;
	const bool bGuid = FGuid::Parse(IdOrName, WantedGuid);
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if ((bGuid && Handle.GetId() == WantedGuid)
			|| Handle.GetName().ToString().Equals(IdOrName, ESearchCase::IgnoreCase)
			|| Handle.GetUniqueInstanceName().Equals(IdOrName, ESearchCase::IgnoreCase))
		{
			return &Handle;
		}
	}
	if (IdOrName.IsEmpty() && System->GetEmitterHandles().Num() == 1) return &System->GetEmitterHandles()[0];
	return nullptr;
}

static FNiagaraEmitterHandle* RequireEmitterHandle(UNiagaraSystem* System, const FString& IdOrName)
{
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, IdOrName);
	if (!Handle)
	{
		SetError(FString::Printf(TEXT("emitter '%s' was not found in '%s'"), *IdOrName,
			System ? *System->GetPathName() : TEXT("<null>")));
	}
	return Handle;
}

static FString UsageToString(ENiagaraScriptUsage Usage)
{
	const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Usage)) : FString();
}

static bool ParseUsage(const FString& Text, ENiagaraScriptUsage& OutUsage)
{
	const FString Token = NormalizeToken(Text);
	if (Token == TEXT("systemspawn")) OutUsage = ENiagaraScriptUsage::SystemSpawnScript;
	else if (Token == TEXT("systemupdate")) OutUsage = ENiagaraScriptUsage::SystemUpdateScript;
	else if (Token == TEXT("emitterspawn")) OutUsage = ENiagaraScriptUsage::EmitterSpawnScript;
	else if (Token == TEXT("emitterupdate")) OutUsage = ENiagaraScriptUsage::EmitterUpdateScript;
	else if (Token == TEXT("particlespawn") || Token == TEXT("spawn")) OutUsage = ENiagaraScriptUsage::ParticleSpawnScript;
	else if (Token == TEXT("particleupdate") || Token == TEXT("update")) OutUsage = ENiagaraScriptUsage::ParticleUpdateScript;
	else return SetError(FString::Printf(TEXT("unknown Niagara stack usage '%s'"), *Text));
	return true;
}

static FString SimTargetToString(ENiagaraSimTarget Target)
{
	return Target == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU");
}

static bool ParseSimTarget(const FString& Text, ENiagaraSimTarget& OutTarget)
{
	const FString Token = NormalizeToken(Text);
	if (Token == TEXT("cpu") || Token == TEXT("cpusim")) OutTarget = ENiagaraSimTarget::CPUSim;
	else if (Token == TEXT("gpu") || Token == TEXT("gpucompute") || Token == TEXT("gpucomputesim"))
		OutTarget = ENiagaraSimTarget::GPUComputeSim;
	else return SetError(FString::Printf(TEXT("unknown Niagara simulation target '%s'"), *Text));
	return true;
}

static bool GetBoolProperty(const UObject* Object, const FName Name, bool bFallback = false)
{
	if (!Object) return bFallback;
	if (const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name))
	{
		return Property->GetPropertyValue_InContainer(Object);
	}
	return bFallback;
}

static bool SetBoolProperty(UObject* Object, const FName Name, bool Value)
{
	if (!Object) return false;
	if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name))
	{
		Object->Modify();
		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
	return false;
}

static FString ExportPropertyValue(UObject* Object, FProperty* Property)
{
	if (!Object || !Property) return FString();
	FString Value;
	Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Object), nullptr, Object, PPF_None);
	return Value;
}

static bool ImportPropertyValue(UObject* Object, FProperty* Property, const FString& Value)
{
	if (!Object || !Property) return false;
	void* Address = Property->ContainerPtrToValuePtr<void>(Object);
	Object->Modify();
	Object->PreEditChange(Property);
	bool bImported = false;
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		UObject* Loaded = nullptr;
		if (!Value.IsEmpty() && !Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			Loaded = LoadObject<UObject>(nullptr, *NormalizeObjectPath(Value));
			if (!Loaded || !Loaded->IsA(ObjectProperty->PropertyClass))
			{
				return SetError(FString::Printf(TEXT("'%s' is not a valid %s for property '%s'"),
					*Value, *ObjectProperty->PropertyClass->GetName(), *Property->GetName()));
			}
		}
		ObjectProperty->SetObjectPropertyValue(Address, Loaded);
		bImported = true;
	}
	else
	{
		bImported = Property->ImportText_Direct(*Value, Address, Object, PPF_None) != nullptr;
	}
	FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
	Object->PostEditChangeProperty(ChangedEvent);
	if (!bImported)
	{
		return SetError(FString::Printf(TEXT("could not import '%s' into property '%s'"), *Value, *Property->GetName()));
	}
	return true;
}

static FNiagaraTypeDefinition ResolveType(const FString& TypeName)
{
	const FString Token = NormalizeToken(TypeName);
	if (Token == TEXT("float") || Token == TEXT("float32") || Token == TEXT("niagarafloat")
		|| Token == TEXT("scalar") || Token == TEXT("double")) return FNiagaraTypeDefinition::GetFloatDef();
	if (Token == TEXT("int") || Token == TEXT("int32") || Token == TEXT("niagaraint32")
		|| Token == TEXT("integer")) return FNiagaraTypeDefinition::GetIntDef();
	if (Token == TEXT("bool") || Token == TEXT("boolean") || Token == TEXT("niagarabool")) return FNiagaraTypeDefinition::GetBoolDef();
	if (Token == TEXT("vec2") || Token == TEXT("vector2") || Token == TEXT("vector2d")
		|| Token == TEXT("vector2f")) return FNiagaraTypeDefinition::GetVec2Def();
	if (Token == TEXT("vec3") || Token == TEXT("vector") || Token == TEXT("vector3")
		|| Token == TEXT("vector3f")) return FNiagaraTypeDefinition::GetVec3Def();
	if (Token == TEXT("position")) return FNiagaraTypeDefinition::GetPositionDef();
	if (Token == TEXT("vec4") || Token == TEXT("vector4") || Token == TEXT("vector4f")) return FNiagaraTypeDefinition::GetVec4Def();
	if (Token == TEXT("color") || Token == TEXT("linearcolor")) return FNiagaraTypeDefinition::GetColorDef();
	if (Token == TEXT("quat") || Token == TEXT("quaternion") || Token == TEXT("quat4f")) return FNiagaraTypeDefinition::GetQuatDef();
	if (Token == TEXT("material") || Token == TEXT("materialinterface")) return FNiagaraTypeDefinition::GetUMaterialDef();
	if (Token == TEXT("staticmesh") || Token == TEXT("mesh")) return FNiagaraTypeDefinition::GetUStaticMeshDef();
	if (Token == TEXT("texture")) return FNiagaraTypeDefinition::GetUTextureDef();
	if (Token == TEXT("object") || Token == TEXT("uobject")) return FNiagaraTypeDefinition::GetUObjectDef();
	for (const FNiagaraTypeDefinition& Type : FNiagaraTypeRegistry::GetRegisteredTypes())
	{
		if (NormalizeToken(Type.GetName()) == Token) return Type;
		const UObject* TypeObject = Type.GetClass();
		if (!TypeObject) TypeObject = Type.GetScriptStruct();
		if (!TypeObject) TypeObject = Type.GetEnum();
		if (TypeObject)
		{
			if (NormalizeToken(TypeObject->GetName()) == Token || NormalizeToken(TypeObject->GetPathName()) == Token) return Type;
		}
	}
	return FNiagaraTypeDefinition();
}

static void ParseFloatList(FString Value, TArray<float>& OutValues)
{
	Value.ReplaceInline(TEXT("("), TEXT(" "));
	Value.ReplaceInline(TEXT(")"), TEXT(" "));
	Value.ReplaceInline(TEXT(","), TEXT(" "));
	Value.ReplaceInline(TEXT("="), TEXT(" "));
	TArray<FString> Tokens;
	Value.ParseIntoArrayWS(Tokens);
	for (const FString& Token : Tokens)
	{
		float Number = 0.0f;
		if (FDefaultValueHelper::ParseFloat(Token, Number)) OutValues.Add(Number);
	}
}

static bool ParseVariableValue(const FNiagaraTypeDefinition& Type, const FString& Value, FNiagaraVariable& OutVariable)
{
	OutVariable = FNiagaraVariable(Type, NAME_None);
	if (!Type.IsValid() || Type.IsDataInterface() || Type.IsUObject()) return false;
	if (Type == FNiagaraTypeDefinition::GetFloatDef())
	{
		OutVariable.SetValue(FCString::Atof(*Value));
		return true;
	}
	if (Type == FNiagaraTypeDefinition::GetIntDef())
	{
		OutVariable.SetValue(FCString::Atoi(*Value));
		return true;
	}
	if (Type == FNiagaraTypeDefinition::GetBoolDef())
	{
		const FString Token = NormalizeToken(Value);
		OutVariable.SetValue(FNiagaraBool(Token == TEXT("true") || Token == TEXT("1") || Token == TEXT("yes")));
		return true;
	}
	TArray<float> Numbers;
	ParseFloatList(Value, Numbers);
	if (Type == FNiagaraTypeDefinition::GetVec2Def() && Numbers.Num() >= 2)
	{
		OutVariable.SetValue(FVector2f(Numbers[0], Numbers[1]));
		return true;
	}
	if (Type == FNiagaraTypeDefinition::GetVec3Def() && Numbers.Num() >= 3)
	{
		OutVariable.SetValue(FVector3f(Numbers[0], Numbers[1], Numbers[2]));
		return true;
	}
	if (Type == FNiagaraTypeDefinition::GetPositionDef() && Numbers.Num() >= 3)
	{
		OutVariable.SetValue(FNiagaraPosition(Numbers[0], Numbers[1], Numbers[2]));
		return true;
	}
	if (Type == FNiagaraTypeDefinition::GetVec4Def() && Numbers.Num() >= 4)
	{
		OutVariable.SetValue(FVector4f(Numbers[0], Numbers[1], Numbers[2], Numbers[3]));
		return true;
	}
	if (Type == FNiagaraTypeDefinition::GetColorDef() && Numbers.Num() >= 3)
	{
		OutVariable.SetValue(FLinearColor(Numbers[0], Numbers[1], Numbers[2], Numbers.Num() > 3 ? Numbers[3] : 1.0f));
		return true;
	}
	if (Type == FNiagaraTypeDefinition::GetQuatDef() && Numbers.Num() >= 4)
	{
		OutVariable.SetValue(FQuat4f(Numbers[0], Numbers[1], Numbers[2], Numbers[3]));
		return true;
	}
	if (UEnum* Enum = Type.GetEnum())
	{
		int64 EnumValue = Enum->GetValueByNameString(Value, EGetByNameFlags::CheckAuthoredName);
		if (EnumValue == INDEX_NONE) EnumValue = FCString::Atoi64(*Value);
		OutVariable.SetValue(static_cast<int32>(EnumValue));
		return true;
	}
	OutVariable.AllocateData();
	if (UScriptStruct* Struct = Type.GetScriptStruct())
	{
		return Struct->ImportText(*Value, OutVariable.GetData(), nullptr, PPF_None, nullptr, Struct->GetName()) != nullptr;
	}
	return false;
}

static FString VariableValueToString(const FNiagaraVariable& Variable)
{
	return Variable.IsDataAllocated() ? Variable.GetType().ToString(Variable.GetData()) : FString();
}

static UEdGraphPin* GetParameterMapPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	if (!Node) return nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction
			&& UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			return Pin;
		}
	}
	return nullptr;
}

static UNiagaraGraph* GetEmitterGraph(FNiagaraEmitterHandle* Handle)
{
	if (!Handle) return nullptr;
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
	return Source ? Source->NodeGraph : nullptr;
}

static UNiagaraGraph* GetSystemGraph(UNiagaraSystem* System)
{
	if (!System || !System->GetSystemSpawnScript()) return nullptr;
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(System->GetSystemSpawnScript()->GetLatestSource());
	return Source ? Source->NodeGraph : nullptr;
}

static UNiagaraNodeOutput* FindOutputNode(UNiagaraGraph* Graph, ENiagaraScriptUsage Usage, const FGuid& UsageId = FGuid())
{
	if (!Graph) return nullptr;
	TArray<UNiagaraNodeOutput*> Outputs;
	Graph->GetNodesOfClass(Outputs);
	for (UNiagaraNodeOutput* Output : Outputs)
	{
		if (Output && Output->GetUsage() == Usage && (!UsageId.IsValid() || Output->GetUsageId() == UsageId)) return Output;
	}
	return nullptr;
}

static void CollectModulesForOutput(UNiagaraNodeOutput* Output, TArray<UNiagaraNodeFunctionCall*>& OutModules)
{
	OutModules.Reset();
	UEdGraphPin* Cursor = GetParameterMapPin(Output, EGPD_Input);
	TSet<const UEdGraphNode*> Visited;
	while (Cursor && Cursor->LinkedTo.Num() > 0)
	{
		UEdGraphPin* UpstreamPin = Cursor->LinkedTo[0];
		UEdGraphNode* Node = UpstreamPin ? UpstreamPin->GetOwningNode() : nullptr;
		if (!Node || Visited.Contains(Node)) break;
		Visited.Add(Node);
		if (UNiagaraNodeFunctionCall* Function = Cast<UNiagaraNodeFunctionCall>(Node))
		{
			if (GetParameterMapPin(Function, EGPD_Input) && GetParameterMapPin(Function, EGPD_Output)) OutModules.Add(Function);
		}
		Cursor = GetParameterMapPin(Node, EGPD_Input);
	}
	Algo::Reverse(OutModules);
}

struct FModuleContext
{
	UNiagaraSystem* System = nullptr;
	FNiagaraEmitterHandle* Handle = nullptr;
	FVersionedNiagaraEmitterData* EmitterData = nullptr;
	UNiagaraGraph* Graph = nullptr;
	UNiagaraNodeOutput* Output = nullptr;
	UNiagaraNodeFunctionCall* Node = nullptr;
	ENiagaraScriptUsage Usage = ENiagaraScriptUsage::Function;
	int32 Index = INDEX_NONE;
};

static bool ResolveTargetOutput(UNiagaraSystem* System, const FString& EmitterIdOrName, ENiagaraScriptUsage Usage,
	FNiagaraEmitterHandle*& OutHandle, UNiagaraGraph*& OutGraph, UNiagaraNodeOutput*& OutOutput)
{
	OutHandle = nullptr;
	OutGraph = nullptr;
	OutOutput = nullptr;
	if (Usage == ENiagaraScriptUsage::SystemSpawnScript || Usage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		OutGraph = GetSystemGraph(System);
	}
	else
	{
		OutHandle = RequireEmitterHandle(System, EmitterIdOrName);
		if (!OutHandle) return false;
		OutGraph = GetEmitterGraph(OutHandle);
	}
	OutOutput = FindOutputNode(OutGraph, Usage);
	if (!OutOutput)
	{
		return SetError(FString::Printf(TEXT("stack output '%s' was not found"), *UsageToString(Usage)));
	}
	UEdGraphPin* Cursor = GetParameterMapPin(OutOutput, EGPD_Input);
	TSet<UEdGraphNode*> Visited;
	bool bHasInput = false;
	while (Cursor && !Cursor->LinkedTo.IsEmpty())
	{
		UEdGraphNode* Upstream = Cursor->LinkedTo[0]->GetOwningNode();
		if (!Upstream || Visited.Contains(Upstream)) break;
		Visited.Add(Upstream);
		if (Cast<UNiagaraNodeInput>(Upstream)) { bHasInput = true; break; }
		Cursor = GetParameterMapPin(Upstream, EGPD_Input);
	}
	if (!bHasInput)
		return SetError(FString::Printf(TEXT("stack '%s' has no connected input/output chain; repair the graph before inserting modules"), *UsageToString(Usage)));
	return true;
}

static FString ScopedModuleId(const FNiagaraEmitterHandle* Handle, const UNiagaraNodeFunctionCall* Node)
{
	return (Handle ? Handle->GetId().ToString(EGuidFormats::DigitsWithHyphens) : TEXT("system"))
		+ TEXT("/") + Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
}

static bool FindModuleContext(UNiagaraSystem* System, const FString& ModuleId, FModuleContext& OutContext)
{
	FString Scope, NodeId = ModuleId;
	const bool bScoped = ModuleId.Split(TEXT("/"), &Scope, &NodeId);
	FGuid Wanted;
	if (!FGuid::Parse(NodeId, Wanted)) return SetError(FString::Printf(TEXT("'%s' is not a valid module identifier"), *ModuleId));
	auto SearchGraph = [&](FNiagaraEmitterHandle* Handle, UNiagaraGraph* Graph) -> bool
	{
		if (!Graph) return false;
		if (bScoped && !Scope.Equals(Handle ? Handle->GetId().ToString(EGuidFormats::DigitsWithHyphens) : TEXT("system"), ESearchCase::IgnoreCase)) return false;
		TArray<UNiagaraNodeOutput*> Outputs;
		Graph->GetNodesOfClass(Outputs);
		for (UNiagaraNodeOutput* Output : Outputs)
		{
			TArray<UNiagaraNodeFunctionCall*> Modules;
			CollectModulesForOutput(Output, Modules);
			for (int32 Index = 0; Index < Modules.Num(); ++Index)
			{
				if (Modules[Index] && Modules[Index]->NodeGuid == Wanted)
				{
					OutContext.System = System;
					OutContext.Handle = Handle;
					OutContext.EmitterData = Handle ? Handle->GetEmitterData() : nullptr;
					OutContext.Graph = Graph;
					OutContext.Output = Output;
					OutContext.Node = Modules[Index];
					OutContext.Usage = Output->GetUsage();
					OutContext.Index = Index;
					return true;
				}
			}
			// Dynamic inputs are function calls upstream of a stack module, not stack entries themselves.
			// 动态输入位于模块的输入链上，不会出现在模块列表中。
			TArray<UEdGraphNode*> Pending{Output};
			TSet<UEdGraphNode*> Visited;
			while (!Pending.IsEmpty())
			{
				UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
				if (!Node || Visited.Contains(Node)) continue;
				Visited.Add(Node);
				if (auto* Function = Cast<UNiagaraNodeFunctionCall>(Node); Function && Function->NodeGuid == Wanted)
				{
					OutContext.System = System;
					OutContext.Handle = Handle;
					OutContext.EmitterData = Handle ? Handle->GetEmitterData() : nullptr;
					OutContext.Graph = Graph; OutContext.Output = Output; OutContext.Node = Function;
					OutContext.Usage = Output->GetUsage(); OutContext.Index = INDEX_NONE;
					return true;
				}
				for (UEdGraphPin* Pin : Node->Pins)
					if (Pin && Pin->Direction == EGPD_Input)
						for (UEdGraphPin* Linked : Pin->LinkedTo)
							if (Linked) Pending.Add(Linked->GetOwningNode());
			}
		}
		return false;
	};
	// Duplicating emitters preserves graph node GUIDs. Bare GUIDs must never silently edit the first emitter.
	// 复制发射器会保留节点 GUID；旧的裸 GUID 存在歧义时必须拒绝，不能改到第一个发射器。
	int32 Matches = SearchGraph(nullptr, GetSystemGraph(System)) ? 1 : 0;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (SearchGraph(&Handle, GetEmitterGraph(&Handle))) ++Matches;
	}
	if (Matches == 1) return true;
	if (Matches > 1) return SetError(TEXT("ambiguous module GUID; use the scoped id returned by list_niagara_modules"));
	return SetError(FString::Printf(TEXT("module '%s' was not found in '%s'"), *ModuleId, *System->GetPathName()));
}

static FString StripModuleNamespace(const FString& Name)
{
	FString Result = Name;
	if (Result.StartsWith(TEXT("Module."), ESearchCase::IgnoreCase)) Result.RightChopInline(7);
	return Result;
}

static bool ResolveFunctionInput(const FModuleContext& Context, const FString& InputName,
	FNiagaraVariable& OutInput, FGuid& OutVariableId, bool* OutHidden = nullptr)
{
	if (!Context.Node) return false;
	FCompileConstantResolver Resolver = Context.Handle
		? FCompileConstantResolver(Context.Handle->GetInstance(), Context.Usage)
		: FCompileConstantResolver(Context.System, Context.Usage);
	TArray<FNiagaraVariable> Inputs;
	TSet<FNiagaraVariable> Hidden;
	FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Context.Node, Inputs, Hidden, Resolver,
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::AllInputs, false);
	const FString Wanted = NormalizeToken(StripModuleNamespace(InputName));
	for (const FNiagaraVariable& Input : Inputs)
	{
		if (NormalizeToken(StripModuleNamespace(Input.GetName().ToString())) == Wanted
			|| NormalizeToken(Input.GetName().ToString()) == NormalizeToken(InputName))
		{
			OutInput = Input;
			OutVariableId.Invalidate();
			if (Context.Node->FunctionScript)
			{
				if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Context.Node->FunctionScript->GetLatestSource()))
				{
					if (Source->NodeGraph)
					{
						if (UNiagaraScriptVariable* ScriptVariable = Source->NodeGraph->GetScriptVariable(Input.GetName()))
							OutVariableId = ScriptVariable->Metadata.GetVariableGuid();
					}
				}
			}
			if (OutHidden) *OutHidden = Hidden.Contains(Input);
			return true;
		}
	}
	return SetError(FString::Printf(TEXT("input '%s' was not found on module '%s'"),
		*InputName, *Context.Node->GetFunctionName()));
}

static FNiagaraParameterHandle MakeAliasedInputHandle(const FModuleContext& Context, const FNiagaraVariable& Input)
{
	return FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(Input.GetName()), Context.Node);
}

static UEdGraphPin* FindOverridePin(const FModuleContext& Context, const FNiagaraVariable& Input)
{
	if (!Context.Node) return nullptr;
	UEdGraphPin* ModuleInput = GetParameterMapPin(Context.Node, EGPD_Input);
	if (!ModuleInput || ModuleInput->LinkedTo.IsEmpty()) return nullptr;
	UEdGraphNode* OverrideNode = ModuleInput->LinkedTo[0]->GetOwningNode();
	if (!OverrideNode || !OverrideNode->GetClass()->GetName().Equals(TEXT("NiagaraNodeParameterMapSet"))) return nullptr;
	const FName AliasedName = MakeAliasedInputHandle(Context, Input).GetParameterHandleString();
	for (UEdGraphPin* Pin : OverrideNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == AliasedName) return Pin;
	}
	return nullptr;
}

static UEdGraphPin* GetOrCreateOverridePin(const FModuleContext& Context, const FNiagaraVariable& Input, const FGuid& VariableId)
{
	FNiagaraParameterHandle Handle = MakeAliasedInputHandle(Context, Input);
	return &FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
		*Context.Node, Handle, Input.GetType(), VariableId, FGuid());
}

static UNiagaraRendererProperties* FindRenderer(FNiagaraEmitterHandle* Handle, const FString& RendererId)
{
	FVersionedNiagaraEmitterData* Data = Handle ? Handle->GetEmitterData() : nullptr;
	if (!Data) return nullptr;
	for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
	{
		if (Renderer && (Renderer->GetName().Equals(RendererId, ESearchCase::IgnoreCase)
			|| Renderer->GetPathName().Equals(RendererId, ESearchCase::IgnoreCase))) return Renderer;
	}
	return nullptr;
}

static UNiagaraRendererProperties* RequireRenderer(FNiagaraEmitterHandle* Handle, const FString& RendererId)
{
	UNiagaraRendererProperties* Renderer = FindRenderer(Handle, RendererId);
	if (!Renderer) SetError(FString::Printf(TEXT("renderer '%s' was not found"), *RendererId));
	return Renderer;
}

static FString RendererTypeName(const UNiagaraRendererProperties* Renderer)
{
	if (Cast<UNiagaraSpriteRendererProperties>(Renderer)) return TEXT("Sprite");
	if (Cast<UNiagaraRibbonRendererProperties>(Renderer)) return TEXT("Ribbon");
	if (Cast<UNiagaraMeshRendererProperties>(Renderer)) return TEXT("Mesh");
	if (Cast<UNiagaraLightRendererProperties>(Renderer)) return TEXT("Light");
	if (Cast<UNiagaraDecalRendererProperties>(Renderer)) return TEXT("Decal");
	if (Cast<UNiagaraComponentRendererProperties>(Renderer)) return TEXT("Component");
	return Renderer ? Renderer->GetClass()->GetName() : FString();
}

static UClass* ResolveRendererClass(const FString& RendererType)
{
	const FString Token = NormalizeToken(RendererType);
	if (Token == TEXT("sprite")) return UNiagaraSpriteRendererProperties::StaticClass();
	if (Token == TEXT("ribbon") || Token == TEXT("trail")) return UNiagaraRibbonRendererProperties::StaticClass();
	if (Token == TEXT("mesh")) return UNiagaraMeshRendererProperties::StaticClass();
	if (Token == TEXT("light")) return UNiagaraLightRendererProperties::StaticClass();
	if (Token == TEXT("decal")) return UNiagaraDecalRendererProperties::StaticClass();
	if (Token == TEXT("component")) return UNiagaraComponentRendererProperties::StaticClass();
	return nullptr;
}

static FString NewPreviewHandle()
{
	return FString::Printf(TEXT("NiagaraPreview_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UNiagaraComponent* FindPreview(const FString& Handle)
{
	if (TWeakObjectPtr<UNiagaraComponent>* Found = PreviewComponents.Find(Handle))
	{
		if (Found->IsValid()) return Found->Get();
		PreviewComponents.Remove(Handle);
	}
	SetError(FString::Printf(TEXT("Niagara preview '%s' was not found"), *Handle));
	return nullptr;
}

static FString ExecutionStateToString(ENiagaraExecutionState State)
{
	const UEnum* Enum = StaticEnum<ENiagaraExecutionState>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(State)) : FString();
}

static FBridgeNiagaraPreviewInfo BuildPreviewInfo(const FString& Handle, UNiagaraComponent* Component)
{
	FBridgeNiagaraPreviewInfo Info;
	Info.Handle = Handle;
	if (!Component) return Info;
	Info.SystemPath = Component->GetAsset() ? Component->GetAsset()->GetPathName() : FString();
	Info.Transform = Component->GetComponentTransform();
	Info.bActive = Component->IsActive();
	Info.bComplete = Component->IsComplete();
	Info.DesiredAge = Component->GetDesiredAge();
	FNiagaraSystemInstanceControllerPtr Controller = Component->GetSystemInstanceController();
	FNiagaraSystemInstance* Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
	if (Instance)
	{
		for (const FNiagaraEmitterInstanceRef& EmitterRef : Instance->GetEmitters())
		{
			const FNiagaraEmitterInstance* Emitter = &EmitterRef.Get();
			if (!Emitter) continue;
			FBridgeNiagaraEmitterRuntimeInfo Runtime;
			Runtime.Name = Emitter->GetEmitterHandle().GetName().ToString();
			Runtime.ExecutionState = ExecutionStateToString(Emitter->GetExecutionState());
			Runtime.ParticleCount = Emitter->GetNumParticles();
			Runtime.BytesUsed = Emitter->GetTotalBytesUsed();
			Info.TotalParticleCount += Runtime.ParticleCount;
			Info.TotalBytesUsed += Runtime.BytesUsed;
			Info.Emitters.Add(MoveTemp(Runtime));
		}
	}
	return Info;
}

static void AddAuditIssue(FBridgeNiagaraAuditResult& Result, const FString& Severity,
	const FString& Message, const FString& EmitterId = FString())
{
	FBridgeNiagaraCompileMessage Issue;
	Issue.Severity = Severity;
	Issue.Message = Message;
	Issue.EmitterId = EmitterId;
	Result.Issues.Add(MoveTemp(Issue));
	if (Severity.Equals(TEXT("Error"), ESearchCase::IgnoreCase)) ++Result.ErrorCount;
	else if (Severity.Equals(TEXT("Warning"), ESearchCase::IgnoreCase)) ++Result.WarningCount;
}

} // namespace BridgeNiagaraImpl

bool UUnrealBridgeNiagaraLibrary::IsNiagaraApiAvailable()
{
	return true;
}

FString UUnrealBridgeNiagaraLibrary::GetLastNiagaraError()
{
	return BridgeNiagaraImpl::LastError;
}

TArray<FBridgeNiagaraTemplateInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraTemplates(
	const FString& AssetType, const FString& Query, int32 MaxResults)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraTemplateInfo> Result;
	const FString TypeToken = NormalizeToken(AssetType);
	const FString QueryToken = Query.ToLower();
	MaxResults = FMath::Clamp(MaxResults, 1, 5000);
	FARFilter Filter;
	if (TypeToken.IsEmpty() || TypeToken == TEXT("all") || TypeToken == TEXT("system"))
		Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	if (TypeToken.IsEmpty() || TypeToken == TEXT("all") || TypeToken == TEXT("emitter"))
		Filter.ClassPaths.Add(UNiagaraEmitter::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	if (Filter.ClassPaths.IsEmpty())
	{
		SetError(FString::Printf(TEXT("unknown Niagara template asset type '%s'"), *AssetType));
		return Result;
	}
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
	for (const FAssetData& Data : Assets)
	{
		const FString Path = Data.GetObjectPathString();
		const FString Searchable = (Path + TEXT(" ") + Data.AssetName.ToString()).ToLower();
		if (!Path.Contains(TEXT("/Templates/"), ESearchCase::IgnoreCase)
			&& !Path.Contains(TEXT("/Template/"), ESearchCase::IgnoreCase)) continue;
		if (!QueryToken.IsEmpty() && !Searchable.Contains(QueryToken)) continue;
		FBridgeNiagaraTemplateInfo Info;
		Info.AssetPath = Path;
		Info.Name = Data.AssetName.ToString();
		if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Data.GetAsset()))
		{
			Info.AssetType = TEXT("System");
			Info.Description = System->TemplateAssetDescription.ToString();
			Info.Category = System->Category.ToString();
		}
		else if (UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(Data.GetAsset()))
		{
			Info.AssetType = TEXT("Emitter");
			Info.Description = Emitter->TemplateAssetDescription.ToString();
			Info.Category = Emitter->Category.ToString();
		}
		else continue;
		Result.Add(MoveTemp(Info));
		if (Result.Num() >= MaxResults) break;
	}
	Result.Sort([](const FBridgeNiagaraTemplateInfo& A, const FBridgeNiagaraTemplateInfo& B)
	{
		return A.AssetPath < B.AssetPath;
	});
	return Result;
}

static FBridgeNiagaraScriptInfo BuildNiagaraScriptInfo(UNiagaraScript* Script)
{
	FBridgeNiagaraScriptInfo Info;
	if (!Script) return Info;
	Info.AssetPath = Script->GetPathName();
	Info.Name = Script->GetName();
	Info.Usage = BridgeNiagaraImpl::UsageToString(Script->GetUsage());
	const FNiagaraAssetVersion Version = Script->GetExposedVersion();
	Info.MajorVersion = Version.MajorVersion;
	Info.MinorVersion = Version.MinorVersion;
	if (const FVersionedNiagaraScriptData* Data = Script->GetLatestScriptData())
	{
		Info.Description = Data->Description.ToString();
		Info.Category = Data->Category.ToString();
		Info.Keywords = Data->Keywords.ToString();
		Info.bLibraryVisible = Data->LibraryVisibility != ENiagaraScriptLibraryVisibility::Unexposed;
		Info.bDeprecated = Data->bDeprecated;
		Info.bExperimental = Data->bExperimental;
	}
	return Info;
}

TArray<FBridgeNiagaraScriptInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraScripts(
	const FString& Usage, const FString& Query, int32 MaxResults)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraScriptInfo> Result;
	const FString UsageToken = NormalizeToken(Usage);
	const FString QueryToken = Query.ToLower();
	MaxResults = FMath::Clamp(MaxResults, 1, 10000);
	FARFilter Filter;
	Filter.ClassPaths.Add(UNiagaraScript::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
	for (const FAssetData& Data : Assets)
	{
		const FString Path = Data.GetObjectPathString();
		if (!QueryToken.IsEmpty() && !(Path + TEXT(" ") + Data.AssetName.ToString()).ToLower().Contains(QueryToken)) continue;
		UNiagaraScript* Script = Cast<UNiagaraScript>(Data.GetAsset());
		if (!Script) continue;
		FBridgeNiagaraScriptInfo Info = BuildNiagaraScriptInfo(Script);
		const FString ActualUsage = NormalizeToken(Info.Usage);
		if (!UsageToken.IsEmpty() && UsageToken != TEXT("all")
			&& ActualUsage != UsageToken
			&& !(UsageToken == TEXT("module") && Script->IsModuleScript())) continue;
		Result.Add(MoveTemp(Info));
		if (Result.Num() >= MaxResults) break;
	}
	Result.Sort([](const FBridgeNiagaraScriptInfo& A, const FBridgeNiagaraScriptInfo& B)
	{
		return A.AssetPath < B.AssetPath;
	});
	return Result;
}

FBridgeNiagaraScriptInfo UUnrealBridgeNiagaraLibrary::GetNiagaraScriptInfo(const FString& ScriptPath)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	return BuildNiagaraScriptInfo(LoadNiagaraAsset<UNiagaraScript>(ScriptPath, TEXT("Niagara Script")));
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::CreateNiagaraSystem(
	const FString& AssetPath, const FString& TemplateSystemPath, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = TemplateSystemPath.IsEmpty()
		? CreateBlankSystemAsset(AssetPath)
		: DuplicateAsset<UNiagaraSystem>(TemplateSystemPath, AssetPath);
	if (!System)
	{
		Result.Message = LastError;
		return Result;
	}
	if (!FinishSystemMutation(System, bSave, true))
	{
		Result.Message = LastError;
		return Result;
	}
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Message = TemplateSystemPath.IsEmpty() ? TEXT("Niagara System created") : TEXT("Niagara System template duplicated");
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::CreateNiagaraEmitter(
	const FString& AssetPath, const FString& TemplateEmitterPath, bool bAddDefaultModulesAndRenderer, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraEmitter* Emitter = TemplateEmitterPath.IsEmpty()
		? CreateBlankEmitterAsset(AssetPath, bAddDefaultModulesAndRenderer)
		: DuplicateAsset<UNiagaraEmitter>(TemplateEmitterPath, AssetPath);
	if (!Emitter)
	{
		Result.Message = LastError;
		return Result;
	}
	if (bSave && !SaveAsset(Emitter))
	{
		SetError(FString::Printf(TEXT("failed to save Niagara Emitter '%s'"), *Emitter->GetPathName()));
		Result.Message = LastError;
		return Result;
	}
	Result.bSuccess = true;
	Result.AssetPath = Emitter->GetPathName();
	Result.Message = TemplateEmitterPath.IsEmpty() ? TEXT("Niagara Emitter created") : TEXT("Niagara Emitter template duplicated");
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::DeleteNiagaraAsset(const FString& AssetPath)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	const FString ObjectPath = NormalizeObjectPath(AssetPath);
	if (!ObjectPath.StartsWith(TEXT("/Game/")))
	{
		Result.Message = TEXT("only exact /Game Niagara asset paths can be deleted");
		SetError(Result.Message);
		return Result;
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!Asset || (!Asset->IsA<UNiagaraSystem>() && !Asset->IsA<UNiagaraEmitter>()))
	{
		Result.Message = FString::Printf(TEXT("Niagara System or Emitter '%s' was not found"), *AssetPath);
		SetError(Result.Message);
		return Result;
	}

	if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset))
	{
		for (auto It = PreviewComponents.CreateIterator(); It; ++It)
		{
			UNiagaraComponent* Component = It.Value().Get();
			if (!Component || Component->GetAsset() == System)
			{
				if (Component) Component->DestroyComponent();
				It.RemoveCurrent();
			}
		}
	}

	const FString DeletedPath = Asset->GetPathName();
	const int32 DeletedCount = ObjectTools::DeleteAssets({FAssetData(Asset)}, false);
	if (DeletedCount != 1)
	{
		Result.Message = FString::Printf(TEXT("failed to delete Niagara asset '%s'; it may still be referenced"), *DeletedPath);
		SetError(Result.Message);
		return Result;
	}

	Result.bSuccess = true;
	Result.AssetPath = DeletedPath;
	Result.Message = TEXT("Niagara asset deleted");
	return Result;
}

FBridgeNiagaraSystemInfo UUnrealBridgeNiagaraLibrary::GetNiagaraSystemInfo(const FString& SystemPath)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraSystemInfo Info;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return Info;
	Info.AssetPath = System->GetPathName();
	Info.bValid = System->IsValid();
	Info.bReadyToRun = System->IsReadyToRun();
	Info.bDirty = System->GetOutermost()->IsDirty();
	Info.bFixedBounds = GetBoolProperty(System, TEXT("bFixedBounds"));
	Info.FixedBounds = System->GetFixedBounds();
	Info.WarmupTime = System->GetWarmupTime();
	Info.WarmupTickDelta = System->GetWarmupTickDelta();
	Info.WarmupTickCount = System->GetWarmupTickCount();
	Info.EmitterCount = System->GetEmitterHandles().Num();
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetIsEnabled()) ++Info.EnabledEmitterCount;
		if (FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
		{
			Info.RendererCount += Data->GetRenderers().Num();
			UNiagaraGraph* Graph = GetEmitterGraph(&Handle);
			if (Graph)
			{
				TArray<UNiagaraNodeOutput*> Outputs;
				Graph->GetNodesOfClass(Outputs);
				for (UNiagaraNodeOutput* Output : Outputs)
				{
					TArray<UNiagaraNodeFunctionCall*> Modules;
					CollectModulesForOutput(Output, Modules);
					Info.ModuleCount += Modules.Num();
				}
			}
		}
	}
	if (UNiagaraGraph* Graph = GetSystemGraph(System))
	{
		TArray<UNiagaraNodeOutput*> Outputs;
		Graph->GetNodesOfClass(Outputs);
		for (UNiagaraNodeOutput* Output : Outputs)
		{
			TArray<UNiagaraNodeFunctionCall*> Modules;
			CollectModulesForOutput(Output, Modules);
			Info.ModuleCount += Modules.Num();
		}
	}
	TArray<FNiagaraVariable> UserParameters;
	System->GetExposedParameters().GetParameters(UserParameters);
	Info.UserParameterCount = UserParameters.Num();
	Info.EffectTypePath = System->GetEffectType() ? System->GetEffectType()->GetPathName() : FString();
	return Info;
}

TArray<FBridgeNiagaraEmitterInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraEmitters(const FString& SystemPath)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraEmitterInfo> Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return Result;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FBridgeNiagaraEmitterInfo Info;
		Info.Id = Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens);
		Info.Name = Handle.GetName().ToString();
		Info.bEnabled = Handle.GetIsEnabled();
		const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
		Info.SourceAssetPath = Instance.Emitter ? Instance.Emitter->GetPathName() : FString();
		if (FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
		{
			Info.bLocalSpace = Data->bLocalSpace;
			Info.bDeterministic = Data->bDeterminism;
			Info.RandomSeed = Data->RandomSeed;
			Info.SimTarget = SimTargetToString(Data->SimTarget);
			const UEnum* InterpolatedEnum = StaticEnum<ENiagaraInterpolatedSpawnMode>();
			Info.InterpolatedSpawnMode = InterpolatedEnum
				? InterpolatedEnum->GetNameStringByValue(static_cast<int64>(Data->InterpolatedSpawnMode)) : FString();
			Info.bFixedBounds = Data->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Fixed;
			Info.FixedBounds = Data->FixedBounds;
			Info.RendererCount = Data->GetRenderers().Num();
		}
		if (UNiagaraGraph* Graph = GetEmitterGraph(&Handle))
		{
			TArray<UNiagaraNodeOutput*> Outputs;
			Graph->GetNodesOfClass(Outputs);
			for (UNiagaraNodeOutput* Output : Outputs)
			{
				TArray<UNiagaraNodeFunctionCall*> Modules;
				CollectModulesForOutput(Output, Modules);
				Info.ModuleCount += Modules.Num();
			}
		}
		Result.Add(MoveTemp(Info));
	}
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::AddNiagaraEmitter(
	const FString& SystemPath, const FString& Name, const FString& EmitterAssetOrTemplatePath, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) { Result.Message = LastError; return Result; }
	UNiagaraEmitter* Source = nullptr;
	if (EmitterAssetOrTemplatePath.IsEmpty())
	{
		Source = NewObject<UNiagaraEmitter>(GetTransientPackage(), NAME_None, RF_Transient);
		UNiagaraEmitterFactoryNew::InitializeEmitter(Source, true);
	}
	else Source = LoadNiagaraAsset<UNiagaraEmitter>(EmitterAssetOrTemplatePath, TEXT("Niagara Emitter"));
	if (!Source) { Result.Message = LastError; return Result; }
	System->Modify();
	const FGuid AddedId = FNiagaraEditorUtilities::AddEmitterToSystem(
		*System, *Source, Source->GetExposedVersion().VersionGuid, true);
	FNiagaraEmitterHandle* StoredHandle = nullptr;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		if (Handle.GetId() == AddedId) { StoredHandle = &Handle; break; }
	if (!AddedId.IsValid() || !StoredHandle)
	{
		SetError(TEXT("Niagara System rejected the emitter handle"));
		Result.Message = LastError;
		return Result;
	}
	if (!Name.IsEmpty()) StoredHandle->SetName(FName(*Name), *System);
	StoredHandle->SetIsEnabled(true, *System, false);
	if (!FinishSystemMutation(System, bSave)) { Result.Message = LastError; return Result; }
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = StoredHandle->GetId().ToString(EGuidFormats::DigitsWithHyphens);
	Result.Message = TEXT("emitter added");
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::DuplicateNiagaraEmitter(
	const FString& SystemPath, const FString& EmitterIdOrName, const FString& NewName, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Source = RequireEmitterHandle(System, EmitterIdOrName);
	if (!System || !Source) { Result.Message = LastError; return Result; }
	const FVersionedNiagaraEmitter SourceInstance = Source->GetInstance();
	if (!SourceInstance.Emitter)
	{
		Result.Message = TEXT("source emitter instance is unavailable");
		SetError(Result.Message);
		return Result;
	}
	const FString SourceName = Source->GetName().ToString();
	const bool bSourceEnabled = Source->GetIsEnabled();
	System->Modify();
	const FGuid AddedId = FNiagaraEditorUtilities::AddEmitterToSystem(
		*System, *SourceInstance.Emitter, SourceInstance.Version, true);
	FNiagaraEmitterHandle* Added = nullptr;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		if (Handle.GetId() == AddedId) { Added = &Handle; break; }
	if (Added)
	{
		Added->SetName(FName(*(NewName.IsEmpty() ? SourceName + TEXT("_Copy") : NewName)), *System);
		Added->SetIsEnabled(bSourceEnabled, *System, false);
	}
	if (!Added || !FinishSystemMutation(System, bSave))
	{
		if (LastError.IsEmpty()) SetError(TEXT("failed to duplicate emitter handle"));
		Result.Message = LastError;
		return Result;
	}
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = Added->GetId().ToString(EGuidFormats::DigitsWithHyphens);
	Result.Message = TEXT("emitter duplicated");
	return Result;
}

bool UUnrealBridgeNiagaraLibrary::RemoveNiagaraEmitter(
	const FString& SystemPath, const FString& EmitterIdOrName, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	if (!System || !Handle) return false;
	const FNiagaraEmitterHandle Copy = *Handle;
	System->Modify();
	System->RemoveEmitterHandle(Copy);
	RemoveEmitterTopologyNodes(System, Copy.GetId());
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::RenameNiagaraEmitter(
	const FString& SystemPath, const FString& EmitterIdOrName, const FString& NewName, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	if (NewName.TrimStartAndEnd().IsEmpty()) return SetError(TEXT("new emitter name is empty"));
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	if (!System || !Handle) return false;
	System->Modify();
	Handle->SetName(FName(*NewName), *System);
	if (UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
		EditorData->SynchronizeOverviewGraphWithSystem(*System);
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraEmitterEnabled(
	const FString& SystemPath, const FString& EmitterIdOrName, bool bEnabled, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	if (!System || !Handle) return false;
	System->Modify();
	Handle->SetIsEnabled(bEnabled, *System, true);
	return FinishSystemMutation(System, bSave, false);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraEmitterProperties(const FString& SystemPath,
	const FString& EmitterIdOrName, bool bLocalSpace, const FString& SimTarget, bool bDeterministic,
	int32 RandomSeed, bool bUseFixedBounds, const FBox& FixedBounds, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	if (!System || !Handle) return false;
	FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
	ENiagaraSimTarget ParsedTarget;
	if (!Data || !ParseSimTarget(SimTarget, ParsedTarget)) return false;
	if (Handle->GetInstance().Emitter) Handle->GetInstance().Emitter->Modify();
	Data->bLocalSpace = bLocalSpace;
	Data->SimTarget = ParsedTarget;
	Data->bDeterminism = bDeterministic;
	Data->RandomSeed = RandomSeed;
	Data->CalculateBoundsMode = bUseFixedBounds
		? ENiagaraEmitterCalculateBoundMode::Fixed : ENiagaraEmitterCalculateBoundMode::Dynamic;
	Data->FixedBounds = FixedBounds;
	return FinishSystemMutation(System, bSave);
}

static FBridgeNiagaraModuleInfo BuildModuleInfo(const BridgeNiagaraImpl::FModuleContext& Context)
{
	using namespace BridgeNiagaraImpl;
	FBridgeNiagaraModuleInfo Info;
	if (!Context.Node) return Info;
	Info.Id = ScopedModuleId(Context.Handle, Context.Node);
	Info.EmitterId = Context.Handle ? Context.Handle->GetId().ToString(EGuidFormats::DigitsWithHyphens) : FString();
	Info.EmitterName = Context.Handle ? Context.Handle->GetName().ToString() : TEXT("System");
	Info.Usage = UsageToString(Context.Usage);
	Info.UsageId = Context.Output ? Context.Output->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens) : FString();
	Info.Index = Context.Index;
	Info.Name = Context.Node->GetFunctionName();
	Info.ScriptPath = Context.Node->FunctionScript ? Context.Node->FunctionScript->GetPathName() : FString();
	Info.VersionId = Context.Node->SelectedScriptVersion.ToString(EGuidFormats::DigitsWithHyphens);
	Info.bEnabled = Context.Node->IsNodeEnabled();
	Info.bAssignment = Cast<UNiagaraNodeAssignment>(Context.Node) != nullptr;
	if (Context.Node->FunctionScript)
	{
		if (const FVersionedNiagaraScriptData* ScriptData = Context.Node->FunctionScript->GetLatestScriptData())
			Info.bDeprecated = ScriptData->bDeprecated;
	}
	FCompileConstantResolver Resolver = Context.Handle
		? FCompileConstantResolver(Context.Handle->GetInstance(), Context.Usage)
		: FCompileConstantResolver(Context.System, Context.Usage);
	TArray<FNiagaraVariable> Inputs;
	FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Context.Node, Inputs, Resolver,
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::AllInputs, false);
	Info.InputCount = Inputs.Num();
	return Info;
}

TArray<FBridgeNiagaraModuleInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraModules(
	const FString& SystemPath, const FString& EmitterIdOrName, const FString& Usage)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraModuleInfo> Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return Result;
	const FString UsageToken = NormalizeToken(Usage);
	auto AppendGraph = [&](FNiagaraEmitterHandle* Handle, UNiagaraGraph* Graph)
	{
		if (!Graph) return;
		TArray<UNiagaraNodeOutput*> Outputs;
		Graph->GetNodesOfClass(Outputs);
		for (UNiagaraNodeOutput* Output : Outputs)
		{
			if (!Output) continue;
			const FString ActualUsage = NormalizeToken(UsageToString(Output->GetUsage()));
			if (!UsageToken.IsEmpty() && UsageToken != TEXT("all") && UsageToken != ActualUsage) continue;
			TArray<UNiagaraNodeFunctionCall*> Modules;
			CollectModulesForOutput(Output, Modules);
			for (int32 Index = 0; Index < Modules.Num(); ++Index)
			{
				FModuleContext Context;
				Context.System = System;
				Context.Handle = Handle;
				Context.EmitterData = Handle ? Handle->GetEmitterData() : nullptr;
				Context.Graph = Graph;
				Context.Output = Output;
				Context.Node = Modules[Index];
				Context.Usage = Output->GetUsage();
				Context.Index = Index;
				Result.Add(BuildModuleInfo(Context));
			}
		}
	};
	if (EmitterIdOrName.IsEmpty())
	{
		AppendGraph(nullptr, GetSystemGraph(System));
		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles()) AppendGraph(&Handle, GetEmitterGraph(&Handle));
	}
	else
	{
		FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
		if (Handle) AppendGraph(Handle, GetEmitterGraph(Handle));
	}
	return Result;
}

static UObject* GetLinkedInputObject(const BridgeNiagaraImpl::FModuleContext& Context,
	const FNiagaraVariable& Input, UNiagaraNodeInput** OutInputNode = nullptr)
{
	using namespace BridgeNiagaraImpl;
	if (OutInputNode) *OutInputNode = nullptr;
	UEdGraphPin* OverridePin = FindOverridePin(Context, Input);
	if (!OverridePin || OverridePin->LinkedTo.IsEmpty()) return nullptr;
	UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(OverridePin->LinkedTo[0]->GetOwningNode());
	if (!InputNode) return nullptr;
	if (OutInputNode) *OutInputNode = InputNode;
	for (const FName PropertyName : {FName(TEXT("DataInterface")), FName(TEXT("ObjectAsset"))})
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(InputNode->GetClass(), PropertyName))
		{
			if (UObject* Object = Property->GetObjectPropertyValue_InContainer(InputNode)) return Object;
		}
	}
	return nullptr;
}

TArray<FBridgeNiagaraModuleInputInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraModuleInputs(
	const FString& SystemPath, const FString& ModuleId, bool bIncludeHidden)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraModuleInputInfo> Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	if (!System || !FindModuleContext(System, ModuleId, Context)) return Result;
	FCompileConstantResolver Resolver = Context.Handle
		? FCompileConstantResolver(Context.Handle->GetInstance(), Context.Usage)
		: FCompileConstantResolver(System, Context.Usage);
	TArray<UEdGraphPin*> SwitchPins;
	TSet<UEdGraphPin*> HiddenSwitches;
	FNiagaraStackGraphUtilities::GetStackFunctionStaticSwitchPins(*Context.Node, SwitchPins, HiddenSwitches, Resolver);
	for (UEdGraphPin* Pin : SwitchPins)
	{
		if (!Pin || (!bIncludeHidden && HiddenSwitches.Contains(Pin))) continue;
		FBridgeNiagaraModuleInputInfo Info;
		Info.ModuleId = ModuleId; Info.Name = StripModuleNamespace(Pin->PinName.ToString());
		Info.Type = UEdGraphSchema_Niagara::PinToTypeDefinition(Pin).GetName();
		Info.bStatic = true; Info.bHidden = HiddenSwitches.Contains(Pin);
		Info.Mode = TEXT("Local"); Info.Value = Pin->DefaultValue;
		Result.Add(MoveTemp(Info));
	}
	TArray<FNiagaraVariable> Inputs;
	TSet<FNiagaraVariable> Hidden;
	FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Context.Node, Inputs, Hidden, Resolver,
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::AllInputs, false);
	for (const FNiagaraVariable& Input : Inputs)
	{
		const bool bHidden = Hidden.Contains(Input);
		if (bHidden && !bIncludeHidden) continue;
		FBridgeNiagaraModuleInputInfo Info;
		Info.ModuleId = ModuleId;
		Info.Name = StripModuleNamespace(Input.GetName().ToString());
		Info.Type = Input.GetType().GetName();
		Info.bStatic = Input.GetType().IsStatic();
		Info.bHidden = bHidden;
		FGuid VariableId;
		if (Context.Node->FunctionScript)
		{
			if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Context.Node->FunctionScript->GetLatestSource()))
			{
				if (Source->NodeGraph)
				{
					if (UNiagaraScriptVariable* ScriptVariable = Source->NodeGraph->GetScriptVariable(Input.GetName()))
						VariableId = ScriptVariable->Metadata.GetVariableGuid();
				}
			}
		}
		Info.VariableId = VariableId.ToString(EGuidFormats::DigitsWithHyphens);
		UEdGraphPin* OverridePin = FindOverridePin(Context, Input);
		if (!OverridePin)
		{
			Info.Mode = TEXT("Default");
			Info.Value = VariableValueToString(Input);
		}
		else if (OverridePin->LinkedTo.IsEmpty())
		{
			Info.Mode = TEXT("Local");
			Info.Value = OverridePin->DefaultValue;
		}
		else
		{
			UEdGraphPin* LinkedPin = OverridePin->LinkedTo[0];
			UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (UNiagaraNodeFunctionCall* Dynamic = Cast<UNiagaraNodeFunctionCall>(LinkedNode))
			{
				Info.Mode = TEXT("Dynamic");
				Info.Value = Dynamic->FunctionScript ? Dynamic->FunctionScript->GetPathName() : Dynamic->GetFunctionName();
			}
			else if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(LinkedNode))
			{
				UObject* Object = GetLinkedInputObject(Context, Input);
				if (Cast<UNiagaraDataInterface>(Object)) Info.Mode = TEXT("DataInterface");
				else if (Object) Info.Mode = TEXT("Object");
				else Info.Mode = TEXT("Linked");
				Info.Value = Object ? Object->GetPathName() : InputNode->Input.GetName().ToString();
			}
			else
			{
				Info.Mode = TEXT("Linked");
				Info.Value = LinkedPin && !LinkedPin->PinName.IsNone()
					? LinkedPin->PinName.ToString()
					: (LinkedNode ? LinkedNode->GetName() : FString());
			}
		}
		Result.Add(MoveTemp(Info));
	}
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::AddNiagaraModule(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& Usage, const FString& ScriptPath,
	const FString& SuggestedName, int32 Index, bool bEnabled, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	UNiagaraScript* Script = LoadNiagaraAsset<UNiagaraScript>(ScriptPath, TEXT("Niagara Script"));
	ENiagaraScriptUsage ParsedUsage;
	if (!System || !Script || !ParseUsage(Usage, ParsedUsage)) { Result.Message = LastError; return Result; }
	if (!Script->IsModuleScript())
	{
		SetError(FString::Printf(TEXT("script '%s' is not a Niagara module script"), *ScriptPath));
		Result.Message = LastError;
		return Result;
	}
	FNiagaraEmitterHandle* Handle = nullptr;
	UNiagaraGraph* Graph = nullptr;
	UNiagaraNodeOutput* Output = nullptr;
	if (!ResolveTargetOutput(System, EmitterIdOrName, ParsedUsage, Handle, Graph, Output))
	{
		Result.Message = LastError;
		return Result;
	}
	System->Modify();
	UNiagaraNodeFunctionCall* Node = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
		Script, *Output, Index < 0 ? INDEX_NONE : Index, SuggestedName, Script->GetExposedVersion().VersionGuid);
	if (!Node)
	{
		SetError(FString::Printf(TEXT("failed to add module '%s' to %s"), *ScriptPath, *Usage));
		Result.Message = LastError;
		return Result;
	}
	if (!bEnabled) FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Node, false);
	if (!FinishSystemMutation(System, bSave)) { Result.Message = LastError; return Result; }
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = ScopedModuleId(Handle, Node);
	Result.Message = TEXT("module added");
	return Result;
}

bool UUnrealBridgeNiagaraLibrary::RemoveNiagaraModule(
	const FString& SystemPath, const FString& ModuleId, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	if (!System || !FindModuleContext(System, ModuleId, Context)) return false;
	UEdGraphPin* ModuleInput = GetParameterMapPin(Context.Node, EGPD_Input);
	UEdGraphPin* ModuleOutput = GetParameterMapPin(Context.Node, EGPD_Output);
	if (!ModuleInput || !ModuleOutput || ModuleInput->LinkedTo.IsEmpty() || ModuleOutput->LinkedTo.IsEmpty())
		return SetError(TEXT("module is not connected to a valid Niagara parameter-map stack"));
	UEdGraphPin* Upstream = ModuleInput->LinkedTo[0];
	UEdGraphNode* OverrideNode = Upstream->GetOwningNode();
	if (OverrideNode && !OverrideNode->GetClass()->GetName().Equals(TEXT("NiagaraNodeParameterMapSet"))) OverrideNode = nullptr;
	if (OverrideNode)
	{
		if (UEdGraphPin* OverrideInput = GetParameterMapPin(OverrideNode, EGPD_Input))
		{
			if (!OverrideInput->LinkedTo.IsEmpty()) Upstream = OverrideInput->LinkedTo[0];
		}
	}
	UEdGraphPin* Downstream = ModuleOutput->LinkedTo[0];
	Context.Graph->Modify();
	Context.Node->Modify();
	ModuleInput->BreakAllPinLinks();
	ModuleOutput->BreakAllPinLinks();
	if (OverrideNode)
	{
		OverrideNode->Modify();
		for (UEdGraphPin* Pin : OverrideNode->Pins) if (Pin) Pin->BreakAllPinLinks();
		Context.Graph->RemoveNode(OverrideNode);
	}
	const UEdGraphSchema* Schema = Context.Graph->GetSchema();
	if (!Schema || !Schema->TryCreateConnection(Upstream, Downstream))
		return SetError(TEXT("failed to reconnect Niagara stack after removing module"));
	Context.Graph->RemoveNode(Context.Node);
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraModuleEnabled(
	const FString& SystemPath, const FString& ModuleId, bool bEnabled, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	if (!System || !FindModuleContext(System, ModuleId, Context)) return false;
	FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Context.Node, bEnabled);
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraModuleInput(const FString& SystemPath,
	const FString& ModuleId, const FString& InputName, const FString& Value, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !FindModuleContext(System, ModuleId, Context)) return false;
	FCompileConstantResolver Resolver = Context.Handle
		? FCompileConstantResolver(Context.Handle->GetInstance(), Context.Usage)
		: FCompileConstantResolver(System, Context.Usage);
	TArray<UEdGraphPin*> SwitchPins;
	TSet<UEdGraphPin*> HiddenSwitches;
	FNiagaraStackGraphUtilities::GetStackFunctionStaticSwitchPins(*Context.Node, SwitchPins, HiddenSwitches, Resolver);
	for (UEdGraphPin* Switch : SwitchPins)
	{
		if (!Switch || NormalizeToken(StripModuleNamespace(Switch->PinName.ToString())) != NormalizeToken(StripModuleNamespace(InputName))) continue;
		const auto* Schema = Cast<UEdGraphSchema_Niagara>(Context.Graph->GetSchema());
		if (!Schema) return SetError(TEXT("Niagara graph schema is unavailable"));
		Context.Node->Modify(); Switch->Modify();
		Schema->TrySetDefaultValue(*Switch, Value, true);
		return FinishSystemMutation(System, bSave);
	}
	if (!ResolveFunctionInput(Context, InputName, Input, VariableId)) return false;
	UEdGraphPin* Pin = GetOrCreateOverridePin(Context, Input, VariableId);
	Pin->Modify();
	Pin->BreakAllPinLinks();
	const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(Context.Graph->GetSchema());
	if (!Schema) return SetError(TEXT("Niagara graph schema is unavailable"));
	Schema->TrySetDefaultValue(*Pin, Value, true);
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::LinkNiagaraModuleInput(const FString& SystemPath,
	const FString& ModuleId, const FString& InputName, const FString& LinkedParameter, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !FindModuleContext(System, ModuleId, Context)
		|| !ResolveFunctionInput(Context, InputName, Input, VariableId)) return false;
	UEdGraphPin* Pin = GetOrCreateOverridePin(Context, Input, VariableId);
	Pin->Modify();
	Pin->BreakAllPinLinks();
	FNiagaraVariableBase Linked(Input.GetType(), FName(*LinkedParameter));
	TSet<FNiagaraVariableBase> KnownParameters;
	KnownParameters.Add(Linked);
	FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(
		*Pin, Linked, KnownParameters, ENiagaraDefaultMode::Value, FGuid());
	return FinishSystemMutation(System, bSave);
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::SetNiagaraModuleDynamicInput(
	const FString& SystemPath, const FString& ModuleId, const FString& InputName,
	const FString& DynamicInputScriptPath, const FString& SuggestedName, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	UNiagaraScript* Script = LoadNiagaraAsset<UNiagaraScript>(DynamicInputScriptPath, TEXT("Niagara Dynamic Input Script"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !Script || !FindModuleContext(System, ModuleId, Context)
		|| !ResolveFunctionInput(Context, InputName, Input, VariableId))
	{
		Result.Message = LastError;
		return Result;
	}
	if (!Script->IsDynamicInputScript())
	{
		SetError(FString::Printf(TEXT("script '%s' is not a Niagara dynamic-input script"), *DynamicInputScriptPath));
		Result.Message = LastError;
		return Result;
	}
	UEdGraphPin* Pin = GetOrCreateOverridePin(Context, Input, VariableId);
	Pin->BreakAllPinLinks();
	UNiagaraNodeFunctionCall* DynamicNode = nullptr;
	FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(*Pin, Script, DynamicNode, FGuid(),
		SuggestedName, Script->GetExposedVersion().VersionGuid);
	if (!DynamicNode || !FinishSystemMutation(System, bSave))
	{
		if (LastError.IsEmpty()) SetError(TEXT("failed to create Niagara dynamic input"));
		Result.Message = LastError;
		return Result;
	}
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = ScopedModuleId(Context.Handle, DynamicNode);
	Result.Message = TEXT("dynamic input created");
	return Result;
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraModuleObjectInput(const FString& SystemPath,
	const FString& ModuleId, const FString& InputName, const FString& ObjectPath, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !FindModuleContext(System, ModuleId, Context)
		|| !ResolveFunctionInput(Context, InputName, Input, VariableId)) return false;
	if (Input.GetType().IsDataInterface())
	{
		UClass* Class = LoadObject<UClass>(nullptr, *NormalizeObjectPath(ObjectPath));
		if (!Class) Class = FindObject<UClass>(nullptr, *ObjectPath);
		if (!Class || !Class->IsChildOf(UNiagaraDataInterface::StaticClass()))
			return SetError(FString::Printf(TEXT("'%s' is not a Niagara data-interface class"), *ObjectPath));
		UEdGraphPin* Pin = GetOrCreateOverridePin(Context, Input, VariableId);
		Pin->BreakAllPinLinks();
		UNiagaraDataInterface* DataInterface = nullptr;
		FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(*Pin, Class, Input.GetName().ToString(), DataInterface);
		return DataInterface && FinishSystemMutation(System, bSave);
	}
	UObject* Object = ObjectPath.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *NormalizeObjectPath(ObjectPath));
	UClass* ExpectedClass = Input.GetType().GetClass();
	if (!Object || (ExpectedClass && !Object->IsA(ExpectedClass)))
		return SetError(FString::Printf(TEXT("'%s' is not a valid object for input '%s'"), *ObjectPath, *InputName));
	UEdGraphPin* Pin = GetOrCreateOverridePin(Context, Input, VariableId);
	Pin->BreakAllPinLinks();
	FNiagaraStackGraphUtilities::SetObjectAssetValueForFunctionInput(*Pin,
		ExpectedClass ? ExpectedClass : UObject::StaticClass(), Input.GetName().ToString(), Object);
	return FinishSystemMutation(System, bSave);
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::SetNiagaraModuleDataInterfaceInput(
	const FString& SystemPath, const FString& ModuleId, const FString& InputName,
	const FString& DataInterfaceClassPath, const TArray<FBridgeNiagaraPropertyValue>& Properties, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !FindModuleContext(System, ModuleId, Context)
		|| !ResolveFunctionInput(Context, InputName, Input, VariableId))
	{
		Result.Message = LastError;
		return Result;
	}
	if (!Input.GetType().IsDataInterface())
	{
		SetError(FString::Printf(TEXT("input '%s' is not a Niagara data-interface input"), *InputName));
		Result.Message = LastError;
		return Result;
	}
	UClass* Class = LoadObject<UClass>(nullptr, *NormalizeObjectPath(DataInterfaceClassPath));
	if (!Class) Class = FindObject<UClass>(nullptr, *DataInterfaceClassPath);
	UClass* Expected = Input.GetType().GetClass();
	if (!Class || !Class->IsChildOf(UNiagaraDataInterface::StaticClass()) || (Expected && !Class->IsChildOf(Expected)))
	{
		SetError(FString::Printf(TEXT("'%s' is not compatible with data-interface input '%s'"),
			*DataInterfaceClassPath, *InputName));
		Result.Message = LastError;
		return Result;
	}
	UEdGraphPin* Pin = GetOrCreateOverridePin(Context, Input, VariableId);
	Pin->BreakAllPinLinks();
	UNiagaraDataInterface* DataInterface = nullptr;
	FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(*Pin, Class,
		Input.GetName().ToString(), DataInterface);
	if (!DataInterface)
	{
		SetError(TEXT("Niagara failed to create the data-interface input object"));
		Result.Message = LastError;
		return Result;
	}
	for (const FBridgeNiagaraPropertyValue& Entry : Properties)
	{
		FProperty* Property = FindFProperty<FProperty>(DataInterface->GetClass(), FName(*Entry.Name));
		if (!Property || !ImportPropertyValue(DataInterface, Property, Entry.Value))
		{
			Result.Warnings.Add(FString::Printf(TEXT("data-interface property '%s' was not applied"), *Entry.Name));
			LastError.Reset();
		}
	}
	if (!FinishSystemMutation(System, bSave)) { Result.Message = LastError; return Result; }
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = DataInterface->GetPathName();
	Result.Message = TEXT("data-interface input created");
	return Result;
}

TArray<FBridgeNiagaraPropertyInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraModuleInputObjectProperties(
	const FString& SystemPath, const FString& ModuleId, const FString& InputName, bool bIncludeAdvanced)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraPropertyInfo> Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !FindModuleContext(System, ModuleId, Context)
		|| !ResolveFunctionInput(Context, InputName, Input, VariableId)) return Result;
	UObject* Object = GetLinkedInputObject(Context, Input);
	if (!Object)
	{
		SetError(FString::Printf(TEXT("input '%s' has no object or data-interface override"), *InputName));
		return Result;
	}
	for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
		if (!bIncludeAdvanced && Property->HasMetaData(TEXT("AdvancedDisplay"))) continue;
		FBridgeNiagaraPropertyInfo Info;
		Info.Name = Property->GetName();
		Info.Type = Property->GetCPPType();
		Info.Value = ExportPropertyValue(Object, Property);
		Info.Category = Property->GetMetaData(TEXT("Category"));
		Info.bEditable = true;
		Result.Add(MoveTemp(Info));
	}
	return Result;
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraModuleInputObjectProperty(const FString& SystemPath,
	const FString& ModuleId, const FString& InputName, const FString& PropertyName,
	const FString& Value, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !FindModuleContext(System, ModuleId, Context)
		|| !ResolveFunctionInput(Context, InputName, Input, VariableId)) return false;
	UObject* Object = GetLinkedInputObject(Context, Input);
	if (!Object) return SetError(FString::Printf(TEXT("input '%s' has no object or data-interface override"), *InputName));
	FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), FName(*PropertyName));
	if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit))
		return SetError(FString::Printf(TEXT("editable property '%s' was not found on '%s'"), *PropertyName, *Object->GetClass()->GetName()));
	return ImportPropertyValue(Object, Property, Value) && FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::ResetNiagaraModuleInput(const FString& SystemPath,
	const FString& ModuleId, const FString& InputName, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FModuleContext Context;
	FNiagaraVariable Input;
	FGuid VariableId;
	if (!System || !FindModuleContext(System, ModuleId, Context)
		|| !ResolveFunctionInput(Context, InputName, Input, VariableId)) return false;
	UEdGraphPin* Pin = FindOverridePin(Context, Input);
	if (!Pin) return true;
	UEdGraphNode* Owner = Pin->GetOwningNode();
	Pin->Modify();
	Pin->BreakAllPinLinks();
	if (!Owner || !Owner->RemovePin(Pin)) return SetError(TEXT("failed to remove Niagara input override pin"));
	return FinishSystemMutation(System, bSave);
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::AddNiagaraParameterAssignment(
	const FString& SystemPath, const FString& EmitterIdOrName, const FString& Usage,
	const FString& ParameterName, const FString& Type, const FString& Value, int32 Index, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	ENiagaraScriptUsage ParsedUsage;
	FNiagaraTypeDefinition TypeDefinition = ResolveType(Type);
	if (!System || !ParseUsage(Usage, ParsedUsage) || !TypeDefinition.IsValid())
	{
		if (System && LastError.IsEmpty()) SetError(FString::Printf(TEXT("unknown Niagara type '%s'"), *Type));
		Result.Message = LastError;
		return Result;
	}
	FNiagaraEmitterHandle* Handle = nullptr;
	UNiagaraGraph* Graph = nullptr;
	UNiagaraNodeOutput* Output = nullptr;
	if (!ResolveTargetOutput(System, EmitterIdOrName, ParsedUsage, Handle, Graph, Output))
	{
		Result.Message = LastError;
		return Result;
	}
	FNiagaraVariable Variable(TypeDefinition, FName(*ParameterName));
	UNiagaraNodeAssignment* Node = FNiagaraStackGraphUtilities::AddParameterModuleToStack(
		{Variable}, *Output, Index < 0 ? INDEX_NONE : Index, {Value});
	if (!Node || !FinishSystemMutation(System, bSave))
	{
		if (LastError.IsEmpty()) SetError(TEXT("failed to create Niagara parameter assignment"));
		Result.Message = LastError;
		return Result;
	}
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = ScopedModuleId(Handle, Node);
	Result.Message = TEXT("parameter assignment added");
	return Result;
}

static FName MakeUserParameterName(const FString& Name)
{
	FString FullName = Name;
	FullName.TrimStartAndEndInline();
	if (!FullName.StartsWith(TEXT("User."), ESearchCase::IgnoreCase)) FullName = TEXT("User.") + FullName;
	return FName(*FullName);
}

static bool FindUserParameter(UNiagaraSystem* System, const FString& Name, FNiagaraVariable& OutVariable)
{
	if (!System) return false;
	const FName Wanted = MakeUserParameterName(Name);
	TArray<FNiagaraVariable> Parameters;
	System->GetExposedParameters().GetParameters(Parameters);
	for (const FNiagaraVariable& Parameter : Parameters)
	{
		if (Parameter.GetName() == Wanted || Parameter.GetName() == FName(*Name))
		{
			OutVariable = Parameter;
			return true;
		}
	}
	return false;
}

static bool SetUserParameterValue(UNiagaraSystem* System, const FNiagaraVariable& Parameter, const FString& Value)
{
	using namespace BridgeNiagaraImpl;
	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
	const FNiagaraTypeDefinition& Type = Parameter.GetType();
	if (Type.IsDataInterface())
	{
		UClass* Class = Type.GetClass();
		if (!Value.IsEmpty())
		{
			UClass* Requested = LoadObject<UClass>(nullptr, *NormalizeObjectPath(Value));
			if (Requested && Requested->IsChildOf(Class)) Class = Requested;
		}
		if (!Class || !Class->IsChildOf(UNiagaraDataInterface::StaticClass()))
			return SetError(FString::Printf(TEXT("parameter '%s' has an invalid data-interface type"), *Parameter.GetName().ToString()));
		Store.SetDataInterface(NewObject<UNiagaraDataInterface>(System, Class, NAME_None, RF_Transactional), Parameter);
		return true;
	}
	if (Type.IsUObject())
	{
		UObject* Object = Value.IsEmpty() || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase)
			? nullptr : LoadObject<UObject>(nullptr, *NormalizeObjectPath(Value));
		if (!Value.IsEmpty() && !Value.Equals(TEXT("None"), ESearchCase::IgnoreCase)
			&& (!Object || (Type.GetClass() && !Object->IsA(Type.GetClass()))))
			return SetError(FString::Printf(TEXT("'%s' is not compatible with parameter '%s'"), *Value, *Parameter.GetName().ToString()));
		Store.SetUObject(Object, Parameter);
		return true;
	}
	FNiagaraVariable Parsed;
	if (!ParseVariableValue(Type, Value, Parsed))
		return SetError(FString::Printf(TEXT("could not parse '%s' as Niagara type '%s'"), *Value, *Type.GetName()));
	return Store.SetParameterData(Parsed.GetData(), Parameter, false);
}

TArray<FBridgeNiagaraParameterInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraUserParameters(const FString& SystemPath)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraParameterInfo> Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return Result;
	FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
	TArray<FNiagaraVariable> Parameters;
	Store.GetParameters(Parameters);
	for (FNiagaraVariable& Parameter : Parameters)
	{
		FBridgeNiagaraParameterInfo Info;
		Info.Name = Parameter.GetName().ToString();
		Info.Type = Parameter.GetType().GetName();
		Info.bUserParameter = Info.Name.StartsWith(TEXT("User."), ESearchCase::IgnoreCase);
		Info.bDataInterface = Parameter.GetType().IsDataInterface();
		Info.bObject = Parameter.GetType().IsUObject();
		if (Info.bDataInterface)
		{
			if (UNiagaraDataInterface* Object = Store.GetDataInterface(Parameter)) Info.Value = Object->GetPathName();
		}
		else if (Info.bObject)
		{
			if (UObject* Object = Store.GetUObject(Parameter)) Info.Value = Object->GetPathName();
		}
		else if (const uint8* Data = Store.GetParameterData(Parameter))
		{
			Info.Value = Parameter.GetType().ToString(Data);
		}
		Result.Add(MoveTemp(Info));
	}
	Result.Sort([](const FBridgeNiagaraParameterInfo& A, const FBridgeNiagaraParameterInfo& B)
	{
		return A.Name < B.Name;
	});
	return Result;
}

bool UUnrealBridgeNiagaraLibrary::AddNiagaraUserParameter(const FString& SystemPath,
	const FString& Name, const FString& Type, const FString& DefaultValue, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraTypeDefinition TypeDefinition = ResolveType(Type);
	if (!System || !TypeDefinition.IsValid())
	{
		if (System) SetError(FString::Printf(TEXT("unknown Niagara parameter type '%s'"), *Type));
		return false;
	}
	FNiagaraVariable Existing;
	if (FindUserParameter(System, Name, Existing))
		return SetError(FString::Printf(TEXT("user parameter '%s' already exists"), *Name));
	System->Modify();
	FNiagaraVariable Parameter(TypeDefinition, MakeUserParameterName(Name));
	if (!System->GetExposedParameters().AddParameter(Parameter, true, true))
		return SetError(FString::Printf(TEXT("failed to add user parameter '%s'"), *Name));
	if (!DefaultValue.IsEmpty() && !SetUserParameterValue(System, Parameter, DefaultValue)) return false;
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraUserParameterDefault(const FString& SystemPath,
	const FString& Name, const FString& Value, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraVariable Parameter;
	if (!System || !FindUserParameter(System, Name, Parameter))
	{
		if (System) SetError(FString::Printf(TEXT("user parameter '%s' was not found"), *Name));
		return false;
	}
	System->Modify();
	return SetUserParameterValue(System, Parameter, Value) && FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::RenameNiagaraUserParameter(const FString& SystemPath,
	const FString& OldName, const FString& NewName, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraVariable Parameter;
	if (!System || !FindUserParameter(System, OldName, Parameter))
	{
		if (System) SetError(FString::Printf(TEXT("user parameter '%s' was not found"), *OldName));
		return false;
	}
	FNiagaraVariable Existing;
	if (FindUserParameter(System, NewName, Existing))
		return SetError(FString::Printf(TEXT("user parameter '%s' already exists"), *NewName));
	System->Modify();
	System->GetExposedParameters().RenameParameter(Parameter, MakeUserParameterName(NewName));
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::RemoveNiagaraUserParameter(
	const FString& SystemPath, const FString& Name, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraVariable Parameter;
	if (!System || !FindUserParameter(System, Name, Parameter))
	{
		if (System) SetError(FString::Printf(TEXT("user parameter '%s' was not found"), *Name));
		return false;
	}
	System->Modify();
	if (!System->GetExposedParameters().RemoveParameter(Parameter))
		return SetError(FString::Printf(TEXT("failed to remove user parameter '%s'"), *Name));
	return FinishSystemMutation(System, bSave);
}

static bool ApplyRendererMaterial(UNiagaraRendererProperties* Renderer, UMaterialInterface* Material, int32 MaterialIndex)
{
	if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
	{
		Sprite->Material = Material;
		return true;
	}
	if (UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
	{
		Ribbon->Material = Material;
		return true;
	}
	if (UNiagaraDecalRendererProperties* Decal = Cast<UNiagaraDecalRendererProperties>(Renderer))
	{
		Decal->Material = Material;
		return true;
	}
	if (UNiagaraMeshRendererProperties* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
	{
		MaterialIndex = FMath::Max(0, MaterialIndex);
		while (Mesh->OverrideMaterials.Num() <= MaterialIndex) Mesh->OverrideMaterials.AddDefaulted();
		Mesh->bOverrideMaterials = true;
		Mesh->OverrideMaterials[MaterialIndex].ExplicitMat = Material;
		return true;
	}
	return false;
}

TArray<FBridgeNiagaraRendererInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraRenderers(
	const FString& SystemPath, const FString& EmitterIdOrName)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraRendererInfo> Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return Result;
	TArray<FNiagaraEmitterHandle*> Handles;
	if (EmitterIdOrName.IsEmpty())
	{
		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles()) Handles.Add(&Handle);
	}
	else
	{
		FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
		if (Handle) Handles.Add(Handle);
	}
	for (FNiagaraEmitterHandle* Handle : Handles)
	{
		FVersionedNiagaraEmitterData* Data = Handle->GetEmitterData();
		if (!Data) continue;
		for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			if (!Renderer) continue;
			FBridgeNiagaraRendererInfo Info;
			Info.Id = Renderer->GetName();
			Info.EmitterId = Handle->GetId().ToString(EGuidFormats::DigitsWithHyphens);
			Info.Type = RendererTypeName(Renderer);
			Info.bEnabled = Renderer->GetIsEnabled();
			TArray<UMaterialInterface*> Materials;
			Renderer->GetUsedMaterials(nullptr, Materials);
			for (UMaterialInterface* Material : Materials)
			{
				if (Material) Info.MaterialPaths.Add(Material->GetPathName());
			}
			if (!Info.MaterialPaths.IsEmpty()) Info.MaterialPath = Info.MaterialPaths[0];
			if (UNiagaraMeshRendererProperties* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				for (const FNiagaraMeshRendererMeshProperties& Entry : Mesh->Meshes)
					if (Entry.Mesh) Info.MeshPaths.Add(Entry.Mesh->GetPathName());
			}
			for (TFieldIterator<FStructProperty> It(Renderer->GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
			{
				if (It->Struct == FNiagaraVariableAttributeBinding::StaticStruct())
				{
					const FNiagaraVariableAttributeBinding* Binding = It->ContainerPtrToValuePtr<FNiagaraVariableAttributeBinding>(Renderer);
					if (Binding && Binding->IsValid()) ++Info.BindingCount;
				}
			}
			Result.Add(MoveTemp(Info));
		}
	}
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::AddNiagaraRenderer(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& RendererType, const FString& Name,
	const FString& MaterialPath, const FString& MeshPath, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UClass* RendererClass = ResolveRendererClass(RendererType);
	if (!System || !Handle || !RendererClass)
	{
		if (System && Handle && !RendererClass) SetError(FString::Printf(TEXT("unknown Niagara renderer type '%s'"), *RendererType));
		Result.Message = LastError;
		return Result;
	}
	const FVersionedNiagaraEmitter Instance = Handle->GetInstance();
	if (!Instance.Emitter)
	{
		SetError(TEXT("emitter handle has no editable emitter instance"));
		Result.Message = LastError;
		return Result;
	}
	FName ObjectName = Name.IsEmpty() ? MakeUniqueObjectName(Instance.Emitter, RendererClass,
		FName(*(RendererType + TEXT("Renderer")))) : MakeUniqueObjectName(Instance.Emitter, RendererClass, FName(*Name));
	UNiagaraRendererProperties* Renderer = NewObject<UNiagaraRendererProperties>(
		Instance.Emitter, RendererClass, ObjectName, RF_Transactional);
	if (!Renderer)
	{
		SetError(TEXT("failed to create Niagara renderer properties"));
		Result.Message = LastError;
		return Result;
	}
	if (!MeshPath.IsEmpty())
	{
		if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
		{
			UStaticMesh* Mesh = LoadNiagaraAsset<UStaticMesh>(MeshPath, TEXT("Static Mesh"));
			if (!Mesh) { Result.Message = LastError; return Result; }
			FNiagaraMeshRendererMeshProperties Entry;
			Entry.Mesh = Mesh;
			MeshRenderer->Meshes = {Entry};
		}
		else Result.Warnings.Add(TEXT("MeshPath is ignored by non-mesh renderers"));
	}
	if (!MaterialPath.IsEmpty())
	{
		UMaterialInterface* Material = LoadNiagaraAsset<UMaterialInterface>(MaterialPath, TEXT("Material"));
		if (!Material) { Result.Message = LastError; return Result; }
		if (!ApplyRendererMaterial(Renderer, Material, 0))
			Result.Warnings.Add(TEXT("this renderer type does not expose a material slot"));
	}
	Instance.Emitter->Modify();
	Instance.Emitter->AddRenderer(Renderer, Instance.Version);
	if (!FinishSystemMutation(System, bSave)) { Result.Message = LastError; return Result; }
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = Renderer->GetName();
	Result.Message = TEXT("renderer added");
	return Result;
}

bool UUnrealBridgeNiagaraLibrary::RemoveNiagaraRenderer(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& RendererId, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UNiagaraRendererProperties* Renderer = RequireRenderer(Handle, RendererId);
	if (!System || !Handle || !Renderer) return false;
	const FVersionedNiagaraEmitter Instance = Handle->GetInstance();
	if (!Instance.Emitter) return SetError(TEXT("emitter handle has no editable emitter instance"));
	Instance.Emitter->Modify();
	Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraRendererEnabled(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& RendererId, bool bEnabled, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UNiagaraRendererProperties* Renderer = RequireRenderer(Handle, RendererId);
	if (!Renderer) return false;
	Renderer->Modify();
	Renderer->SetIsEnabled(bEnabled);
	Renderer->PostEditChange();
	return FinishSystemMutation(System, bSave);
}

TArray<FBridgeNiagaraPropertyInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraRendererProperties(
	const FString& SystemPath, const FString& EmitterIdOrName, const FString& RendererId, bool bIncludeAdvanced)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraPropertyInfo> Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UNiagaraRendererProperties* Renderer = RequireRenderer(Handle, RendererId);
	if (!Renderer) return Result;
	for (TFieldIterator<FProperty> It(Renderer->GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
		if (!bIncludeAdvanced && Property->HasMetaData(TEXT("AdvancedDisplay"))) continue;
		FBridgeNiagaraPropertyInfo Info;
		Info.Name = Property->GetName();
		Info.Type = Property->GetCPPType();
		Info.Value = ExportPropertyValue(Renderer, Property);
		Info.Category = Property->GetMetaData(TEXT("Category"));
		Info.bEditable = true;
		Result.Add(MoveTemp(Info));
	}
	return Result;
}

FString UUnrealBridgeNiagaraLibrary::GetNiagaraRendererProperty(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& RendererId, const FString& PropertyName)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UNiagaraRendererProperties* Renderer = RequireRenderer(Handle, RendererId);
	if (!Renderer) return FString();
	FProperty* Property = FindFProperty<FProperty>(Renderer->GetClass(), FName(*PropertyName));
	if (!Property)
	{
		SetError(FString::Printf(TEXT("renderer property '%s' was not found"), *PropertyName));
		return FString();
	}
	return ExportPropertyValue(Renderer, Property);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraRendererProperty(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& RendererId, const FString& PropertyName,
	const FString& Value, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UNiagaraRendererProperties* Renderer = RequireRenderer(Handle, RendererId);
	if (!Renderer) return false;
	FProperty* Property = FindFProperty<FProperty>(Renderer->GetClass(), FName(*PropertyName));
	if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit))
		return SetError(FString::Printf(TEXT("editable renderer property '%s' was not found"), *PropertyName));
	return ImportPropertyValue(Renderer, Property, Value) && FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraRendererMaterial(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& RendererId, const FString& MaterialPath,
	int32 MaterialIndex, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UNiagaraRendererProperties* Renderer = RequireRenderer(Handle, RendererId);
	UMaterialInterface* Material = MaterialPath.IsEmpty() ? nullptr
		: LoadNiagaraAsset<UMaterialInterface>(MaterialPath, TEXT("Material"));
	if (!Renderer || (!MaterialPath.IsEmpty() && !Material)) return false;
	Renderer->Modify();
	if (!ApplyRendererMaterial(Renderer, Material, MaterialIndex))
		return SetError(FString::Printf(TEXT("renderer '%s' does not expose a material slot"), *RendererId));
	Renderer->PostEditChange();
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraRendererBinding(const FString& SystemPath,
	const FString& EmitterIdOrName, const FString& RendererId, const FString& BindingProperty,
	const FString& VariableName, const FString& SourceMode, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	FNiagaraEmitterHandle* Handle = RequireEmitterHandle(System, EmitterIdOrName);
	UNiagaraRendererProperties* Renderer = RequireRenderer(Handle, RendererId);
	if (!Renderer || !Handle) return false;
	FStructProperty* Property = FindFProperty<FStructProperty>(Renderer->GetClass(), FName(*BindingProperty));
	if (!Property || Property->Struct != FNiagaraVariableAttributeBinding::StaticStruct())
		return SetError(FString::Printf(TEXT("binding property '%s' was not found on renderer '%s'"),
			*BindingProperty, *RendererId));
	const FString ModeToken = NormalizeToken(SourceMode);
	ENiagaraRendererSourceDataMode Mode;
	if (ModeToken == TEXT("particles") || ModeToken == TEXT("particle")) Mode = ENiagaraRendererSourceDataMode::Particles;
	else if (ModeToken == TEXT("emitter")) Mode = ENiagaraRendererSourceDataMode::Emitter;
	else return SetError(FString::Printf(TEXT("unknown renderer source mode '%s'"), *SourceMode));
	Renderer->Modify();
	FNiagaraVariableAttributeBinding* Binding = Property->ContainerPtrToValuePtr<FNiagaraVariableAttributeBinding>(Renderer);
	Binding->SetValue(FName(*VariableName), Handle->GetInstance().ToBase(), Mode);
	Renderer->PostEditChange();
	return FinishSystemMutation(System, bSave);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraSystemWarmup(
	const FString& SystemPath, float WarmupTime, float TickDelta, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return false;
	if (WarmupTime < 0.0f || TickDelta <= 0.0f)
		return SetError(TEXT("warmup time must be non-negative and tick delta must be positive"));
	System->Modify();
	System->SetWarmupTickDelta(TickDelta);
	System->SetWarmupTime(WarmupTime);
	return FinishSystemMutation(System, bSave, false);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraSystemFixedBounds(
	const FString& SystemPath, bool bEnabled, const FBox& Bounds, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return false;
	if (bEnabled && !Bounds.IsValid)
		return SetError(TEXT("fixed bounds must be valid when enabled"));
	System->Modify();
	if (!SetBoolProperty(System, TEXT("bFixedBounds"), bEnabled))
		return SetError(TEXT("Niagara System fixed-bounds property is unavailable"));
	if (bEnabled) System->SetFixedBounds(Bounds);
	return FinishSystemMutation(System, bSave, false);
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraSystemEffectType(
	const FString& SystemPath, const FString& EffectTypePath, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System) return false;
	UNiagaraEffectType* EffectType = EffectTypePath.IsEmpty() ? nullptr
		: LoadNiagaraAsset<UNiagaraEffectType>(EffectTypePath, TEXT("Niagara Effect Type"));
	if (!EffectTypePath.IsEmpty() && !EffectType) return false;
	System->Modify();
	System->SetEffectType(EffectType);
	return FinishSystemMutation(System, bSave, false);
}

static FString CompileSeverityToString(FNiagaraCompileEventSeverity Severity)
{
	switch (Severity)
	{
	case FNiagaraCompileEventSeverity::Error: return TEXT("Error");
	case FNiagaraCompileEventSeverity::Warning: return TEXT("Warning");
	case FNiagaraCompileEventSeverity::Display: return TEXT("Display");
	default: return TEXT("Log");
	}
}

static void AppendScriptDiagnostics(FBridgeNiagaraCompileResult& Result, UNiagaraScript* Script,
	const FString& EmitterId)
{
	if (!Script) return;
	const ENiagaraScriptCompileStatus Status = Script->GetLastCompileStatus();
	const TArray<FNiagaraCompileEvent>& Events = Script->GetVMExecutableData().LastCompileEvents;
	for (const FNiagaraCompileEvent& Event : Events)
	{
		FBridgeNiagaraCompileMessage Message;
		Message.Severity = CompileSeverityToString(Event.Severity);
		Message.Message = Event.Message;
		Message.ShortDescription = Event.ShortDescription;
		Message.EmitterId = EmitterId;
		Message.ScriptUsage = BridgeNiagaraImpl::UsageToString(Script->GetUsage());
		Message.NodeId = Event.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
		Message.PinId = Event.PinGuid.ToString(EGuidFormats::DigitsWithHyphens);
		if (Event.Severity == FNiagaraCompileEventSeverity::Error) ++Result.ErrorCount;
		else if (Event.Severity == FNiagaraCompileEventSeverity::Warning) ++Result.WarningCount;
		Result.Messages.Add(MoveTemp(Message));
	}
	if (Status == ENiagaraScriptCompileStatus::NCS_Error
		&& !Events.ContainsByPredicate([](const FNiagaraCompileEvent& Event)
		{
			return Event.Severity == FNiagaraCompileEventSeverity::Error;
		}))
	{
		FBridgeNiagaraCompileMessage Message;
		Message.Severity = TEXT("Error");
		Message.Message = FString::Printf(TEXT("script '%s' has compile status Error"), *Script->GetPathName());
		Message.EmitterId = EmitterId;
		Message.ScriptUsage = BridgeNiagaraImpl::UsageToString(Script->GetUsage());
		Result.Messages.Add(MoveTemp(Message));
		++Result.ErrorCount;
	}
}

static FBridgeNiagaraCompileResult BuildCompileDiagnostics(UNiagaraSystem* System)
{
	FBridgeNiagaraCompileResult Result;
	if (!System)
	{
		Result.Error = BridgeNiagaraImpl::LastError;
		return Result;
	}
	AppendScriptDiagnostics(Result, System->GetSystemSpawnScript(), FString());
	AppendScriptDiagnostics(Result, System->GetSystemUpdateScript(), FString());
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
		{
			TArray<UNiagaraScript*> Scripts;
			Data->GetScripts(Scripts, false, false);
			for (UNiagaraScript* Script : Scripts)
				AppendScriptDiagnostics(Result, Script, Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens));
		}
	}
	Result.bValid = System->IsValid();
	Result.bReadyToRun = System->IsReadyToRun();
	Result.bSuccess = Result.ErrorCount == 0 && Result.bValid && Result.bReadyToRun;
	if (!Result.bSuccess)
	{
		Result.Error = Result.ErrorCount > 0 ? TEXT("Niagara compilation reported errors")
			: (!Result.bValid ? TEXT("Niagara System is invalid") : TEXT("Niagara System is not ready to run"));
	}
	return Result;
}

FBridgeNiagaraCompileResult UUnrealBridgeNiagaraLibrary::CompileNiagaraSystem(
	const FString& SystemPath, bool bForce, bool bWaitForGpuShaders, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System)
	{
		FBridgeNiagaraCompileResult Result;
		Result.Error = LastError;
		return Result;
	}
	System->RequestCompile(bForce);
	System->WaitForCompilationComplete(bWaitForGpuShaders, false);
	FBridgeNiagaraCompileResult Result = BuildCompileDiagnostics(System);
	if (bSave && !SaveAsset(System))
	{
		Result.bSuccess = false;
		Result.Error = TEXT("Niagara System compiled but could not be saved");
		SetError(Result.Error);
	}
	return Result;
}

FBridgeNiagaraCompileResult UUnrealBridgeNiagaraLibrary::GetNiagaraCompileDiagnostics(const FString& SystemPath)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	return BuildCompileDiagnostics(System);
}

FBridgeNiagaraAuditResult UUnrealBridgeNiagaraLibrary::ValidateNiagaraSystem(const FString& SystemPath,
	bool bCheckMaterials, bool bCheckBounds, int32 MaxEmitters, int32 MaxRenderersPerEmitter,
	int32 MaxModulesPerEmitter)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraAuditResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (!System)
	{
		AddAuditIssue(Result, TEXT("Error"), LastError);
		return Result;
	}
	MaxEmitters = FMath::Max(1, MaxEmitters);
	MaxRenderersPerEmitter = FMath::Max(1, MaxRenderersPerEmitter);
	MaxModulesPerEmitter = FMath::Max(1, MaxModulesPerEmitter);
	Result.EmitterCount = System->GetEmitterHandles().Num();
	if (Result.EmitterCount == 0) AddAuditIssue(Result, TEXT("Error"), TEXT("system contains no emitters"));
	if (Result.EmitterCount > MaxEmitters)
		AddAuditIssue(Result, TEXT("Warning"), FString::Printf(TEXT("system has %d emitters; budget is %d"), Result.EmitterCount, MaxEmitters));
	const bool bSystemFixedBounds = GetBoolProperty(System, TEXT("bFixedBounds"));
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FString EmitterId = Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens);
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data)
		{
			AddAuditIssue(Result, TEXT("Error"), TEXT("emitter handle has no versioned data"), EmitterId);
			continue;
		}
		if (!Handle.GetIsEnabled()) AddAuditIssue(Result, TEXT("Warning"), TEXT("emitter is disabled"), EmitterId);
		if (Data->SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			++Result.GpuEmitterCount;
			if (bCheckBounds && !bSystemFixedBounds && Data->CalculateBoundsMode != ENiagaraEmitterCalculateBoundMode::Fixed)
				AddAuditIssue(Result, TEXT("Error"), TEXT("GPU emitter has no fixed system or emitter bounds"), EmitterId);
		}
		const int32 RendererCount = Data->GetRenderers().Num();
		Result.RendererCount += RendererCount;
		if (RendererCount == 0) AddAuditIssue(Result, TEXT("Warning"), TEXT("emitter has no renderer"), EmitterId);
		if (RendererCount > MaxRenderersPerEmitter)
			AddAuditIssue(Result, TEXT("Warning"), FString::Printf(TEXT("emitter has %d renderers; budget is %d"),
				RendererCount, MaxRenderersPerEmitter), EmitterId);
		for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			if (!Renderer) continue;
			if (!Renderer->GetIsEnabled()) AddAuditIssue(Result, TEXT("Warning"),
				FString::Printf(TEXT("renderer '%s' is disabled"), *Renderer->GetName()), EmitterId);
			if (bCheckMaterials)
			{
				TArray<UMaterialInterface*> Materials;
				Renderer->GetUsedMaterials(nullptr, Materials);
				if ((Cast<UNiagaraSpriteRendererProperties>(Renderer)
					|| Cast<UNiagaraRibbonRendererProperties>(Renderer)
					|| Cast<UNiagaraDecalRendererProperties>(Renderer)) && Materials.IsEmpty())
				{
					AddAuditIssue(Result, TEXT("Error"), FString::Printf(TEXT("renderer '%s' has no material"),
						*Renderer->GetName()), EmitterId);
				}
				for (UMaterialInterface* Material : Materials)
				{
					if (!Material)
					{
						AddAuditIssue(Result, TEXT("Error"), FString::Printf(TEXT("renderer '%s' contains a null material"),
							*Renderer->GetName()), EmitterId);
						continue;
					}
					const EBlendMode BlendMode = Material->GetBlendMode();
					if (BlendMode == BLEND_Translucent || BlendMode == BLEND_Additive || BlendMode == BLEND_Modulate
						|| BlendMode == BLEND_AlphaComposite || BlendMode == BLEND_AlphaHoldout)
						++Result.TranslucentRendererCount;
				}
			}
		}
		int32 EmitterModules = 0;
		if (UNiagaraGraph* Graph = GetEmitterGraph(&Handle))
		{
			TArray<UNiagaraNodeOutput*> Outputs;
			Graph->GetNodesOfClass(Outputs);
			for (UNiagaraNodeOutput* Output : Outputs)
			{
				TArray<UNiagaraNodeFunctionCall*> Modules;
				CollectModulesForOutput(Output, Modules);
				EmitterModules += Modules.Num();
				for (UNiagaraNodeFunctionCall* Module : Modules)
				{
					if (!Module->IsNodeEnabled()) AddAuditIssue(Result, TEXT("Warning"),
						FString::Printf(TEXT("module '%s' is disabled"), *Module->GetFunctionName()), EmitterId);
					if (Module->FunctionScript)
					{
						if (const FVersionedNiagaraScriptData* ScriptData = Module->FunctionScript->GetLatestScriptData())
							if (ScriptData->bDeprecated) AddAuditIssue(Result, TEXT("Warning"),
								FString::Printf(TEXT("module '%s' is deprecated"), *Module->GetFunctionName()), EmitterId);
					}
				}
			}
		}
		Result.ModuleCount += EmitterModules;
		if (EmitterModules > MaxModulesPerEmitter)
			AddAuditIssue(Result, TEXT("Warning"), FString::Printf(TEXT("emitter has %d modules; budget is %d"),
				EmitterModules, MaxModulesPerEmitter), EmitterId);
	}
	const FBridgeNiagaraCompileResult Compile = BuildCompileDiagnostics(System);
	for (const FBridgeNiagaraCompileMessage& Message : Compile.Messages)
	{
		if (Message.Severity == TEXT("Error") || Message.Severity == TEXT("Warning"))
		{
			Result.Issues.Add(Message);
			if (Message.Severity == TEXT("Error")) ++Result.ErrorCount;
			else ++Result.WarningCount;
		}
	}
	if (!Compile.bValid) AddAuditIssue(Result, TEXT("Error"), TEXT("Niagara System is invalid"));
	if (!Compile.bReadyToRun) AddAuditIssue(Result, TEXT("Error"), TEXT("Niagara System is not ready to run"));
	Result.bPassed = Result.ErrorCount == 0;
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::CreateNiagaraSystemFromRecipe(
	const FString& AssetPath, const FBridgeNiagaraSystemRecipe& Recipe, bool bCompile, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result = CreateNiagaraSystem(AssetPath, Recipe.TemplatePath, false);
	if (!Result.bSuccess) return Result;
	const FString SystemPath = Result.AssetPath;
	auto Fail = [&](const FString& Step) -> FBridgeNiagaraOperationResult
	{
		Result.bSuccess = false;
		Result.Message = FString::Printf(TEXT("recipe step '%s' failed: %s"), *Step, *GetLastNiagaraError());
		return Result;
	};
	if (!SetNiagaraSystemWarmup(SystemPath, Recipe.WarmupTime,
		Recipe.WarmupTickDelta > 0.0f ? Recipe.WarmupTickDelta : 1.0f / 30.0f, false)) return Fail(TEXT("warmup"));
	if (!SetNiagaraSystemFixedBounds(SystemPath, Recipe.bUseFixedBounds, Recipe.FixedBounds, false))
		return Fail(TEXT("fixed bounds"));
	if (!Recipe.EffectTypePath.IsEmpty() && !SetNiagaraSystemEffectType(SystemPath, Recipe.EffectTypePath, false))
		return Fail(TEXT("effect type"));
	for (const FBridgeNiagaraParameterSpec& Parameter : Recipe.UserParameters)
	{
		if (!AddNiagaraUserParameter(SystemPath, Parameter.Name, Parameter.Type, Parameter.DefaultValue, false))
			return Fail(FString::Printf(TEXT("user parameter %s"), *Parameter.Name));
	}
	for (const FBridgeNiagaraEmitterSpec& EmitterSpec : Recipe.Emitters)
	{
		FBridgeNiagaraOperationResult AddedEmitter = AddNiagaraEmitter(
			SystemPath, EmitterSpec.Name, EmitterSpec.TemplatePath, false);
		if (!AddedEmitter.bSuccess) return Fail(FString::Printf(TEXT("emitter %s"), *EmitterSpec.Name));
		const FString EmitterId = AddedEmitter.Id;
		if (!SetNiagaraEmitterEnabled(SystemPath, EmitterId, EmitterSpec.bEnabled, false))
			return Fail(FString::Printf(TEXT("emitter enabled %s"), *EmitterSpec.Name));
		if (!SetNiagaraEmitterProperties(SystemPath, EmitterId, EmitterSpec.bLocalSpace,
			EmitterSpec.SimTarget, EmitterSpec.bDeterministic, EmitterSpec.RandomSeed,
			EmitterSpec.bUseFixedBounds, EmitterSpec.FixedBounds, false))
			return Fail(FString::Printf(TEXT("emitter properties %s"), *EmitterSpec.Name));
		for (const FBridgeNiagaraModuleSpec& ModuleSpec : EmitterSpec.Modules)
		{
			FBridgeNiagaraOperationResult AddedModule = AddNiagaraModule(SystemPath, EmitterId,
				ModuleSpec.Usage, ModuleSpec.ScriptPath, ModuleSpec.Name, ModuleSpec.Index,
				ModuleSpec.bEnabled, false);
			if (!AddedModule.bSuccess) return Fail(FString::Printf(TEXT("module %s"), *ModuleSpec.Name));
			for (const FBridgeNiagaraInputValue& Input : ModuleSpec.Inputs)
			{
				const FString Mode = NormalizeToken(Input.Mode);
				bool bInputSuccess = false;
				if (Mode.IsEmpty() || Mode == TEXT("local") || Mode == TEXT("value"))
					bInputSuccess = SetNiagaraModuleInput(SystemPath, AddedModule.Id, Input.Name, Input.Value, false);
				else if (Mode == TEXT("linked") || Mode == TEXT("parameter"))
					bInputSuccess = LinkNiagaraModuleInput(SystemPath, AddedModule.Id, Input.Name, Input.Value, false);
				else if (Mode == TEXT("dynamic"))
				{
					FBridgeNiagaraOperationResult Dynamic = SetNiagaraModuleDynamicInput(SystemPath,
						AddedModule.Id, Input.Name, Input.SourcePath, FString(), false);
					bInputSuccess = Dynamic.bSuccess;
				}
				else if (Mode == TEXT("datainterface") || Mode == TEXT("di"))
				{
					FBridgeNiagaraOperationResult DataInterface = SetNiagaraModuleDataInterfaceInput(SystemPath,
						AddedModule.Id, Input.Name, Input.SourcePath, Input.Properties, false);
					bInputSuccess = DataInterface.bSuccess;
					Result.Warnings.Append(DataInterface.Warnings);
				}
				else if (Mode == TEXT("object") || Mode == TEXT("asset"))
					bInputSuccess = SetNiagaraModuleObjectInput(SystemPath, AddedModule.Id, Input.Name, Input.Value, false);
				else
					SetError(FString::Printf(TEXT("unknown recipe input mode '%s'"), *Input.Mode));
				if (!bInputSuccess) return Fail(FString::Printf(TEXT("input %s.%s"), *ModuleSpec.Name, *Input.Name));
			}
		}
		for (const FBridgeNiagaraRendererSpec& RendererSpec : EmitterSpec.Renderers)
		{
			FBridgeNiagaraOperationResult AddedRenderer = AddNiagaraRenderer(SystemPath, EmitterId,
				RendererSpec.Type, RendererSpec.Name, RendererSpec.MaterialPath, RendererSpec.MeshPath, false);
			Result.Warnings.Append(AddedRenderer.Warnings);
			if (!AddedRenderer.bSuccess) return Fail(FString::Printf(TEXT("renderer %s"), *RendererSpec.Name));
			if (!RendererSpec.bEnabled
				&& !SetNiagaraRendererEnabled(SystemPath, EmitterId, AddedRenderer.Id, false, false))
				return Fail(FString::Printf(TEXT("renderer enabled %s"), *RendererSpec.Name));
			for (const FBridgeNiagaraPropertyValue& Entry : RendererSpec.Properties)
				if (!SetNiagaraRendererProperty(SystemPath, EmitterId, AddedRenderer.Id, Entry.Name, Entry.Value, false))
					return Fail(FString::Printf(TEXT("renderer property %s.%s"), *RendererSpec.Name, *Entry.Name));
			for (const FBridgeNiagaraPropertyValue& Entry : RendererSpec.Bindings)
				if (!SetNiagaraRendererBinding(SystemPath, EmitterId, AddedRenderer.Id, Entry.Name, Entry.Value, TEXT("Particles"), false))
					return Fail(FString::Printf(TEXT("renderer binding %s.%s"), *RendererSpec.Name, *Entry.Name));
		}
	}
	if (bCompile)
	{
		FBridgeNiagaraCompileResult Compile = CompileNiagaraSystem(SystemPath, true, true, bSave);
		if (!Compile.bSuccess)
		{
			Result.bSuccess = false;
			Result.Message = Compile.Error;
			for (const FBridgeNiagaraCompileMessage& Message : Compile.Messages)
				if (Message.Severity == TEXT("Warning")) Result.Warnings.Add(Message.Message);
			return Result;
		}
	}
	else
	{
		UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
		if (!FinishSystemMutation(System, bSave, true)) return Fail(TEXT("final save"));
	}
	Result.bSuccess = true;
	Result.Message = TEXT("Niagara System recipe completed");
	return Result;
}

static FString ColorValue(const FLinearColor& Color)
{
	return FString::Printf(TEXT("(R=%g,G=%g,B=%g,A=%g)"), Color.R, Color.G, Color.B, Color.A);
}

static FString VectorValue(const FVector& Value)
{
	return FString::Printf(TEXT("(X=%g,Y=%g,Z=%g)"), Value.X, Value.Y, Value.Z);
}

static bool EnsurePresetParameter(const FString& SystemPath, const FString& Name,
	const FString& Type, const FString& Value, TArray<FString>& Warnings)
{
	FNiagaraVariable Existing;
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *BridgeNiagaraImpl::NormalizeObjectPath(SystemPath));
	if (FindUserParameter(System, Name, Existing))
	{
		if (Existing.GetType() != BridgeNiagaraImpl::ResolveType(Type))
		{
			Warnings.Add(FString::Printf(TEXT("existing user parameter '%s' has incompatible type"), *Name));
			return false;
		}
		return UUnrealBridgeNiagaraLibrary::SetNiagaraUserParameterDefault(SystemPath, Name, Value, false);
	}
	return UUnrealBridgeNiagaraLibrary::AddNiagaraUserParameter(SystemPath, Name, Type, Value, false);
}

static int32 BindPresetParameter(const FString& SystemPath, const FString& ParameterName,
	const TArray<FString>& ModuleTerms, const TArray<FString>& InputTerms,
	const FString& RequiredType, TArray<FString>& Warnings)
{
	int32 BoundCount = 0;
	const FNiagaraTypeDefinition RequiredTypeDefinition = BridgeNiagaraImpl::ResolveType(RequiredType);
	const TArray<FBridgeNiagaraModuleInfo> Modules = UUnrealBridgeNiagaraLibrary::ListNiagaraModules(SystemPath, FString(), TEXT("All"));
	for (const FBridgeNiagaraModuleInfo& Module : Modules)
	{
		bool bModuleMatches = ModuleTerms.IsEmpty();
		const FString ModuleToken = BridgeNiagaraImpl::NormalizeToken(Module.Name);
		for (const FString& Term : ModuleTerms)
			if (ModuleToken.Contains(BridgeNiagaraImpl::NormalizeToken(Term))) { bModuleMatches = true; break; }
		if (!bModuleMatches) continue;
		const TArray<FBridgeNiagaraModuleInputInfo> Inputs = UUnrealBridgeNiagaraLibrary::ListNiagaraModuleInputs(
			SystemPath, Module.Id, true);
		const FBridgeNiagaraModuleInputInfo* BestInput = nullptr;
		int32 BestScore = MIN_int32;
		for (const FBridgeNiagaraModuleInputInfo& Input : Inputs)
		{
			const FNiagaraTypeDefinition ActualTypeDefinition = BridgeNiagaraImpl::ResolveType(Input.Type);
			if (RequiredTypeDefinition.IsValid()
				&& (!ActualTypeDefinition.IsValid() || ActualTypeDefinition != RequiredTypeDefinition)) continue;
			const FString InputToken = BridgeNiagaraImpl::NormalizeToken(Input.Name);
			int32 Score = MIN_int32;
			for (int32 TermIndex = 0; TermIndex < InputTerms.Num(); ++TermIndex)
			{
				const FString TermToken = BridgeNiagaraImpl::NormalizeToken(InputTerms[TermIndex]);
				if (InputToken == TermToken) Score = FMath::Max(Score, 1000 - TermIndex);
				else if (InputToken.EndsWith(TermToken)) Score = FMath::Max(Score, 800 - TermIndex);
				else if (InputToken.Contains(TermToken)) Score = FMath::Max(Score, 500 - TermIndex);
			}
			if (Score == MIN_int32) continue;
			if (Input.bHidden) Score -= 25;
			if (InputToken.StartsWith(TEXT("localmodule")) || InputToken.StartsWith(TEXT("output"))) Score -= 250;
			if (Score > BestScore)
			{
				BestScore = Score;
				BestInput = &Input;
			}
		}
		if (BestInput && UUnrealBridgeNiagaraLibrary::LinkNiagaraModuleInput(SystemPath, Module.Id, BestInput->Name,
			TEXT("User.") + ParameterName, false)) ++BoundCount;
	}
	if (BoundCount == 0)
		Warnings.Add(FString::Printf(TEXT("preset parameter 'User.%s' had no compatible template input; it remains exposed for manual binding"),
			*ParameterName));
	return BoundCount;
}

static void ApplyPresetMaterial(const FString& SystemPath, const FString& MaterialPath, TArray<FString>& Warnings)
{
	if (MaterialPath.IsEmpty()) return;
	for (const FBridgeNiagaraRendererInfo& Renderer : UUnrealBridgeNiagaraLibrary::ListNiagaraRenderers(SystemPath, FString()))
	{
		if (Renderer.Type == TEXT("Sprite") || Renderer.Type == TEXT("Ribbon")
			|| Renderer.Type == TEXT("Mesh") || Renderer.Type == TEXT("Decal"))
		{
			if (!UUnrealBridgeNiagaraLibrary::SetNiagaraRendererMaterial(SystemPath, Renderer.EmitterId,
				Renderer.Id, MaterialPath, 0, false))
				Warnings.Add(FString::Printf(TEXT("material was not applied to renderer '%s'"), *Renderer.Id));
		}
	}
}

static FBridgeNiagaraOperationResult FinalizePreset(FBridgeNiagaraOperationResult Result, bool bSave)
{
	if (!Result.bSuccess) return Result;
	FBridgeNiagaraCompileResult Compile = UUnrealBridgeNiagaraLibrary::CompileNiagaraSystem(
		Result.AssetPath, true, true, bSave);
	for (const FBridgeNiagaraCompileMessage& Message : Compile.Messages)
		if (Message.Severity == TEXT("Warning")) Result.Warnings.Add(Message.Message);
	if (!Compile.bSuccess)
	{
		Result.bSuccess = false;
		Result.Message = Compile.Error;
	}
	return Result;
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::CreateWeaponTrailEffect(const FString& AssetPath,
	const FString& Style, const FString& MaterialPath, const FLinearColor& Color, float Width,
	float Lifetime, float SpawnRate, bool bLocalSpace, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	const FString StyleToken = NormalizeToken(Style);
	FString TemplatePath;
	const bool bRibbonStyle = StyleToken == TEXT("ribbon") || StyleToken == TEXT("trail")
		|| StyleToken == TEXT("locationbased");
	if (StyleToken == TEXT("beam") || StyleToken == TEXT("dynamicbeam"))
		TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam.DynamicBeam");
	else if (StyleToken == TEXT("staticbeam"))
		TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/StaticBeam.StaticBeam");
	else if (bRibbonStyle)
		TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain");
	else if (!bRibbonStyle)
	{
		FBridgeNiagaraOperationResult Invalid;
		Invalid.Message = FString::Printf(TEXT("unknown weapon-trail style '%s'; use Ribbon, Beam, or StaticBeam"), *Style);
		SetError(Invalid.Message);
		return Invalid;
	}
	FBridgeNiagaraOperationResult Result = CreateNiagaraSystem(AssetPath, FString(), false);
	if (!Result.bSuccess) return Result;
	FBridgeNiagaraOperationResult Emitter = AddNiagaraEmitter(Result.AssetPath, TEXT("WeaponTrail"), TemplatePath, false);
	if (!Emitter.bSuccess) { Result.bSuccess = false; Result.Message = Emitter.Message; return Result; }
	if (!SetNiagaraEmitterProperties(Result.AssetPath, Emitter.Id, bLocalSpace, TEXT("CPU"),
		false, 0, true, FBox(FVector(-2000.0f), FVector(2000.0f)), false))
	{
		Result.bSuccess = false; Result.Message = GetLastNiagaraError(); return Result;
	}
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeColor"), TEXT("Color"), ColorValue(Color), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeWidth"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.01f, Width)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeLifetime"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.01f, Lifetime)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeSpawnRate"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.0f, SpawnRate)), Result.Warnings);
	if (bRibbonStyle)
	{
		for (const FBridgeNiagaraModuleInfo& Module : ListNiagaraModules(Result.AssetPath, Emitter.Id, TEXT("All")))
		{
			const FString ModuleToken = NormalizeToken(Module.Name);
			if (ModuleToken.Contains(TEXT("shapelocation")) || ModuleToken.Contains(TEXT("addvelocity"))
				|| ModuleToken.Contains(TEXT("gravityforce")) || ModuleToken == TEXT("drag"))
			{
				if (!RemoveNiagaraModule(Result.AssetPath, Module.Id, false))
					Result.Warnings.Add(FString::Printf(TEXT("trail motion module '%s' could not be removed"), *Module.Name));
			}
		}
		for (const FBridgeNiagaraRendererInfo& Renderer : ListNiagaraRenderers(Result.AssetPath, Emitter.Id))
			RemoveNiagaraRenderer(Result.AssetPath, Emitter.Id, Renderer.Id, false);
		const FString RibbonMaterialPath = MaterialPath.IsEmpty()
			? TEXT("/Niagara/DefaultAssets/DefaultRibbonMaterial.DefaultRibbonMaterial") : MaterialPath;
		FBridgeNiagaraOperationResult RibbonRenderer = AddNiagaraRenderer(Result.AssetPath, Emitter.Id,
			TEXT("Ribbon"), TEXT("BridgeWeaponTrailRibbon"), RibbonMaterialPath, FString(), false);
		if (!RibbonRenderer.bSuccess)
		{
			Result.bSuccess = false;
			Result.Message = RibbonRenderer.Message;
			return Result;
		}
		BindPresetParameter(Result.AssetPath, TEXT("BridgeColor"),
			{TEXT("Initialize")}, {TEXT("Color")}, TEXT("Color"), Result.Warnings);
		BindPresetParameter(Result.AssetPath, TEXT("BridgeWidth"),
			{TEXT("Initialize")}, {TEXT("Ribbon Width"), TEXT("Width")}, TEXT("Float"), Result.Warnings);
		BindPresetParameter(Result.AssetPath, TEXT("BridgeLifetime"),
			{TEXT("Initialize")}, {TEXT("Lifetime")},
			TEXT("Float"), Result.Warnings);
		BindPresetParameter(Result.AssetPath, TEXT("BridgeSpawnRate"),
			{TEXT("SpawnRate")}, {TEXT("SpawnRate")}, TEXT("Float"), Result.Warnings);
	}
	else
	{
		BindPresetParameter(Result.AssetPath, TEXT("BridgeColor"),
			{TEXT("Color"), TEXT("Initialize")}, {TEXT("Color")}, TEXT("Color"), Result.Warnings);
		BindPresetParameter(Result.AssetPath, TEXT("BridgeWidth"),
			{TEXT("BeamWidth"), TEXT("Initialize")}, {TEXT("Beam Width"), TEXT("Width"), TEXT("Ribbon Width")},
			TEXT("Float"), Result.Warnings);
		BindPresetParameter(Result.AssetPath, TEXT("BridgeLifetime"),
			{TEXT("Initialize")}, {TEXT("Lifetime")}, TEXT("Float"), Result.Warnings);
		Result.Warnings.Add(TEXT("beam styles spawn one beam per activation; BridgeSpawnRate remains exposed for optional custom spawning"));
	}
	ApplyPresetMaterial(Result.AssetPath, MaterialPath, Result.Warnings);
	if (!SetNiagaraSystemFixedBounds(Result.AssetPath, true, FBox(FVector(-2000.0f), FVector(2000.0f)), false))
		Result.Warnings.Add(TEXT("system fixed bounds could not be enabled"));
	Result.Message = TEXT("deliverable weapon trail preset created");
	return FinalizePreset(Result, bSave);
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::CreateSparkEffect(const FString& AssetPath,
	const FString& Style, const FString& MaterialPath, const FLinearColor& Color, int32 Count,
	float Speed, float Lifetime, float Gravity, bool bCollision, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	const FString StyleToken = NormalizeToken(Style);
	FString TemplatePath;
	if (StyleToken == TEXT("directional") || StyleToken == TEXT("cone"))
		TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst");
	else if (StyleToken == TEXT("radial") || StyleToken == TEXT("omnidirectional"))
		TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst");
	else
	{
		FBridgeNiagaraOperationResult Invalid;
		Invalid.Message = FString::Printf(TEXT("unknown spark style '%s'; use Directional or Radial"), *Style);
		SetError(Invalid.Message);
		return Invalid;
	}
	FBridgeNiagaraOperationResult Result = CreateNiagaraSystem(AssetPath, TemplatePath, false);
	if (!Result.bSuccess) return Result;
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeColor"), TEXT("Color"), ColorValue(Color), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeCount"), TEXT("Int"), FString::FromInt(FMath::Max(1, Count)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeSpeed"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.0f, Speed)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeLifetime"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.01f, Lifetime)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeGravity"), TEXT("Vec3"), VectorValue(FVector(0.0, 0.0, Gravity)), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeColor"), {TEXT("Initialize"), TEXT("Color")}, {TEXT("Color")}, TEXT("Color"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeCount"), {TEXT("SpawnBurst")}, {TEXT("SpawnCount"), TEXT("Count")}, TEXT("Int"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeSpeed"), {TEXT("AddVelocity")},
		{TEXT("Velocity Strength"), TEXT("Velocity Speed"), TEXT("Speed")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeLifetime"), {TEXT("Initialize")}, {TEXT("Lifetime")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeGravity"), {TEXT("Gravity")}, {TEXT("Gravity"), TEXT("Acceleration")}, TEXT("Vec3"), Result.Warnings);
	if (bCollision)
	{
		bool bHasCollision = false;
		TArray<FBridgeNiagaraModuleInfo> Modules = ListNiagaraModules(Result.AssetPath, FString(), TEXT("All"));
		for (const FBridgeNiagaraModuleInfo& Module : Modules)
			if (NormalizeToken(Module.Name).Contains(TEXT("collision"))) { bHasCollision = true; break; }
		if (!bHasCollision)
		{
			const TArray<FBridgeNiagaraEmitterInfo> Emitters = ListNiagaraEmitters(Result.AssetPath);
			if (!Emitters.IsEmpty())
			{
				FBridgeNiagaraOperationResult Collision = AddNiagaraModule(Result.AssetPath, Emitters[0].Id,
					TEXT("ParticleUpdate"), TEXT("/Niagara/Modules/Collision/Collision.Collision"),
					TEXT("BridgeCollision"), -1, true, false);
				if (!Collision.bSuccess) Result.Warnings.Add(TEXT("collision module could not be added; template remains otherwise functional"));
			}
		}
	}
	ApplyPresetMaterial(Result.AssetPath, MaterialPath, Result.Warnings);
	SetNiagaraSystemFixedBounds(Result.AssetPath, true, FBox(FVector(-5000.0f), FVector(5000.0f)), false);
	Result.Message = TEXT("deliverable spark burst preset created");
	return FinalizePreset(Result, bSave);
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::CreateExplosionEffect(const FString& AssetPath,
	const FString& Style, const FString& MaterialPath, const FLinearColor& CoreColor, float Scale,
	float Duration, int32 DebrisCount, bool bShockwave, bool bLight, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	const FString StyleToken = NormalizeToken(Style);
	if (!(StyleToken.IsEmpty() || StyleToken == TEXT("layered") || StyleToken == TEXT("simple")
		|| StyleToken == TEXT("cinematic")))
	{
		FBridgeNiagaraOperationResult Invalid;
		Invalid.Message = FString::Printf(TEXT("unknown explosion style '%s'; use Layered, Simple, or Cinematic"), *Style);
		SetError(Invalid.Message);
		return Invalid;
	}
	FBridgeNiagaraOperationResult Result = CreateNiagaraSystem(AssetPath,
		TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion"), false);
	if (!Result.bSuccess) return Result;
	TArray<FBridgeNiagaraEmitterInfo> Emitters = ListNiagaraEmitters(Result.AssetPath);
	bool bFoundShockwave = false;
	for (const FBridgeNiagaraEmitterInfo& Emitter : Emitters)
	{
		const FString NameToken = NormalizeToken(Emitter.Name);
		if (NameToken.Contains(TEXT("shock")) || NameToken.Contains(TEXT("ring")))
		{
			bFoundShockwave = true;
			break;
		}
	}
	if (bShockwave && !bFoundShockwave)
	{
		FBridgeNiagaraOperationResult Shockwave = AddNiagaraEmitter(Result.AssetPath, TEXT("ShockwaveRibbon"),
			TEXT("/Niagara/DefaultAssets/Templates/BehaviorExamples/RibbonID.RibbonID"), false);
		if (Shockwave.bSuccess)
		{
			bFoundShockwave = true;
			Emitters = ListNiagaraEmitters(Result.AssetPath);
		}
		else Result.Warnings.Add(TEXT("shockwave ribbon emitter could not be added"));
	}
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeCoreColor"), TEXT("Color"), ColorValue(CoreColor), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeScale"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.01f, Scale)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeShockwaveRadius"), TEXT("Float"),
		FString::SanitizeFloat(250.0f * FMath::Max(0.01f, Scale)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeDuration"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.05f, Duration)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeDebrisCount"), TEXT("Int"), FString::FromInt(FMath::Max(0, DebrisCount)), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeCoreColor"), {TEXT("Initialize"), TEXT("Color")}, {TEXT("Color")}, TEXT("Color"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeScale"), {TEXT("Initialize")},
		{TEXT("Mesh Uniform Scale"), TEXT("Scale")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeShockwaveRadius"), {TEXT("ShapeLocation")},
		{TEXT("Sphere Radius"), TEXT("Ring Radius"), TEXT("Radius")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeDuration"), {TEXT("Initialize")}, {TEXT("Lifetime"), TEXT("Duration")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeDebrisCount"), {TEXT("SpawnBurst")}, {TEXT("SpawnCount"), TEXT("Count")}, TEXT("Int"), Result.Warnings);
	bool bFoundLight = false;
	for (const FBridgeNiagaraEmitterInfo& Emitter : Emitters)
	{
		const FString NameToken = NormalizeToken(Emitter.Name);
		if (NameToken.Contains(TEXT("shock")) || NameToken.Contains(TEXT("ring")))
		{
			SetNiagaraEmitterEnabled(Result.AssetPath, Emitter.Id, bShockwave, false);
			bFoundShockwave = true;
		}
		if (NameToken.Contains(TEXT("light")))
		{
			SetNiagaraEmitterEnabled(Result.AssetPath, Emitter.Id, bLight, false);
			bFoundLight = true;
		}
	}
	if (bLight && !bFoundLight && !Emitters.IsEmpty())
	{
		FBridgeNiagaraOperationResult Light = AddNiagaraRenderer(Result.AssetPath, Emitters[0].Id,
			TEXT("Light"), TEXT("BridgeExplosionLight"), FString(), FString(), false);
		if (!Light.bSuccess) Result.Warnings.Add(TEXT("explosion light renderer could not be added"));
	}
	ApplyPresetMaterial(Result.AssetPath, MaterialPath, Result.Warnings);
	SetNiagaraSystemFixedBounds(Result.AssetPath, true,
		FBox(FVector(-3000.0f * Scale), FVector(3000.0f * Scale)), false);
	Result.Message = TEXT("deliverable layered explosion preset created");
	return FinalizePreset(Result, bSave);
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::CreateDissolveEffect(const FString& AssetPath,
	const FString& Style, const FString& MaterialPath, const FLinearColor& Color, int32 Count,
	float Duration, float Radius, const FVector& Direction, bool bSave)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	const FString StyleToken = NormalizeToken(Style);
	if (!(StyleToken.IsEmpty() || StyleToken == TEXT("ash") || StyleToken == TEXT("energy")
		|| StyleToken == TEXT("dust") || StyleToken == TEXT("disintegrate")))
	{
		FBridgeNiagaraOperationResult Invalid;
		Invalid.Message = FString::Printf(TEXT("unknown dissolve style '%s'; use Ash, Dust, Energy, or Disintegrate"), *Style);
		SetError(Invalid.Message);
		return Invalid;
	}
	FBridgeNiagaraOperationResult Result = CreateNiagaraSystem(AssetPath, FString(), false);
	if (!Result.bSuccess) return Result;
	FBridgeNiagaraOperationResult Emitter = AddNiagaraEmitter(Result.AssetPath, TEXT("DissolveParticles"),
		TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles.BlowingParticles"), false);
	if (!Emitter.bSuccess) { Result.bSuccess = false; Result.Message = Emitter.Message; return Result; }
	SetNiagaraEmitterProperties(Result.AssetPath, Emitter.Id, false, TEXT("CPU"), false, 0, true,
		FBox(FVector(-FMath::Max(100.0f, Radius * 5.0f)), FVector(FMath::Max(100.0f, Radius * 5.0f))), false);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeColor"), TEXT("Color"), ColorValue(Color), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeCount"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(1, Count)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeDuration"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.05f, Duration)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeRadius"), TEXT("Float"), FString::SanitizeFloat(FMath::Max(0.0f, Radius)), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("BridgeDirection"), TEXT("Vec3"), VectorValue(Direction.GetSafeNormal()), Result.Warnings);
	EnsurePresetParameter(Result.AssetPath, TEXT("DissolveAmount"), TEXT("Float"), TEXT("0.0"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeColor"), {TEXT("Initialize"), TEXT("Color")}, {TEXT("Color")}, TEXT("Color"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeCount"), {TEXT("SpawnRate")}, {TEXT("SpawnRate")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeDuration"), {TEXT("Initialize")}, {TEXT("Lifetime"), TEXT("Duration")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeRadius"), {TEXT("ShapeLocation"), TEXT("Sphere")}, {TEXT("Sphere Radius"), TEXT("Radius")}, TEXT("Float"), Result.Warnings);
	BindPresetParameter(Result.AssetPath, TEXT("BridgeDirection"), {TEXT("Wind")},
		{TEXT("Wind Speed"), TEXT("Direction")}, TEXT("Vec3"), Result.Warnings);
	ApplyPresetMaterial(Result.AssetPath, MaterialPath, Result.Warnings);
	if (!SetNiagaraSystemFixedBounds(Result.AssetPath, true,
		FBox(FVector(-FMath::Max(100.0f, Radius * 5.0f)), FVector(FMath::Max(100.0f, Radius * 5.0f))), false))
		Result.Warnings.Add(TEXT("system fixed bounds could not be enabled"));
	Result.Warnings.Add(TEXT("DissolveAmount is exposed for synchronizing the Niagara breakup layer with a dissolve-capable mesh/UI material; bind the source mesh or skeletal-mesh data interface for asset-specific surface sampling"));
	Result.Message = TEXT("deliverable dissolve/disintegration particle layer created");
	return FinalizePreset(Result, bSave);
}

FBridgeNiagaraOperationResult UUnrealBridgeNiagaraLibrary::SpawnNiagaraPreview(
	const FString& SystemPath, const FTransform& Transform, bool bAutoActivate, bool bResetOnChange)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	FBridgeNiagaraOperationResult Result;
	UNiagaraSystem* System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	if (System && !System->IsReadyToRun())
	{
		const FBridgeNiagaraCompileResult Compile = CompileNiagaraSystem(SystemPath, true, true, false);
		if (!Compile.bSuccess)
		{
			Result.Message = Compile.Error.IsEmpty()
				? TEXT("Niagara System could not be made ready for preview") : Compile.Error;
			SetError(Result.Message);
			return Result;
		}
		System = LoadNiagaraAsset<UNiagaraSystem>(SystemPath, TEXT("Niagara System"));
	}
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!System || !World)
	{
		if (System && !World) SetError(TEXT("no editor world is available for Niagara preview"));
		Result.Message = LastError;
		return Result;
	}
	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System,
		Transform.GetLocation(), Transform.GetRotation().Rotator(), Transform.GetScale3D(),
		false, bAutoActivate, ENCPoolMethod::None, false);
	if (!Component)
	{
		SetError(TEXT("failed to spawn Niagara preview component"));
		Result.Message = LastError;
		return Result;
	}
	Component->SetAutoDestroy(false);
	Component->SetForceSolo(true);
	if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Component->GetClass(), TEXT("bResetOnChange")))
		Property->SetPropertyValue_InContainer(Component, bResetOnChange);
	const FString Handle = NewPreviewHandle();
	PreviewComponents.Add(Handle, Component);
	Result.bSuccess = true;
	Result.AssetPath = System->GetPathName();
	Result.Id = Handle;
	Result.Message = TEXT("transient Niagara preview spawned");
	return Result;
}

TArray<FBridgeNiagaraPreviewInfo> UUnrealBridgeNiagaraLibrary::ListNiagaraPreviews()
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	TArray<FBridgeNiagaraPreviewInfo> Result;
	for (auto It = PreviewComponents.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		Result.Add(BuildPreviewInfo(It.Key(), It.Value().Get()));
	}
	return Result;
}

FBridgeNiagaraPreviewInfo UUnrealBridgeNiagaraLibrary::GetNiagaraPreviewInfo(const FString& Handle)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	return BuildPreviewInfo(Handle, FindPreview(Handle));
}

bool UUnrealBridgeNiagaraLibrary::AdvanceNiagaraPreview(
	const FString& Handle, float Seconds, float TickDelta)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraComponent* Component = FindPreview(Handle);
	if (!Component) return false;
	if (Seconds < 0.0f || TickDelta <= 0.0f)
		return SetError(TEXT("preview advance seconds must be non-negative and tick delta must be positive"));
	const int32 TickCount = FMath::CeilToInt(Seconds / TickDelta);
	if (TickCount > 0) Component->AdvanceSimulation(TickCount, TickDelta);
	return true;
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraPreviewTransform(
	const FString& Handle, const FTransform& Transform, bool bTeleport)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraComponent* Component = FindPreview(Handle);
	if (!Component) return false;
	Component->SetWorldTransform(Transform, false, nullptr,
		bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None);
	return true;
}

bool UUnrealBridgeNiagaraLibrary::SetNiagaraPreviewVariable(const FString& Handle,
	const FString& Name, const FString& Type, const FString& Value)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraComponent* Component = FindPreview(Handle);
	if (!Component) return false;
	FNiagaraTypeDefinition TypeDefinition = ResolveType(Type);
	if (!TypeDefinition.IsValid()) return SetError(FString::Printf(TEXT("unknown Niagara variable type '%s'"), *Type));
	const FName VariableName = MakeUserParameterName(Name);
	if (TypeDefinition.IsUObject())
	{
		UObject* Object = Value.IsEmpty() || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase)
			? nullptr : LoadObject<UObject>(nullptr, *NormalizeObjectPath(Value));
		if (!Value.IsEmpty() && !Object) return SetError(FString::Printf(TEXT("object '%s' was not found"), *Value));
		if (TypeDefinition == FNiagaraTypeDefinition::GetUMaterialDef()) Component->SetVariableMaterial(VariableName, Cast<UMaterialInterface>(Object));
		else if (TypeDefinition == FNiagaraTypeDefinition::GetUStaticMeshDef()) Component->SetVariableStaticMesh(VariableName, Cast<UStaticMesh>(Object));
		else Component->SetVariableObject(VariableName, Object);
		return true;
	}
	if (TypeDefinition.IsDataInterface())
		return SetError(TEXT("runtime preview data-interface replacement is not supported by this generic setter; configure the system input object before spawning"));
	FNiagaraVariable Parsed;
	if (!ParseVariableValue(TypeDefinition, Value, Parsed))
		return SetError(FString::Printf(TEXT("could not parse '%s' as '%s'"), *Value, *Type));
	if (TypeDefinition == FNiagaraTypeDefinition::GetFloatDef()) Component->SetVariableFloat(VariableName, Parsed.GetValue<float>());
	else if (TypeDefinition == FNiagaraTypeDefinition::GetIntDef()) Component->SetVariableInt(VariableName, Parsed.GetValue<int32>());
	else if (TypeDefinition == FNiagaraTypeDefinition::GetBoolDef()) Component->SetVariableBool(VariableName, Parsed.GetValue<FNiagaraBool>().GetValue());
	else if (TypeDefinition == FNiagaraTypeDefinition::GetVec2Def())
	{
		const FVector2f V = Parsed.GetValue<FVector2f>();
		Component->SetVariableVec2(VariableName, FVector2D(V.X, V.Y));
	}
	else if (TypeDefinition == FNiagaraTypeDefinition::GetVec3Def())
	{
		const FVector3f V = Parsed.GetValue<FVector3f>();
		Component->SetVariableVec3(VariableName, FVector(V));
	}
	else if (TypeDefinition == FNiagaraTypeDefinition::GetPositionDef())
	{
		const FNiagaraPosition V = Parsed.GetValue<FNiagaraPosition>();
		Component->SetVariablePosition(VariableName, FVector(V));
	}
	else if (TypeDefinition == FNiagaraTypeDefinition::GetVec4Def())
	{
		const FVector4f V = Parsed.GetValue<FVector4f>();
		Component->SetVariableVec4(VariableName, FVector4(V.X, V.Y, V.Z, V.W));
	}
	else if (TypeDefinition == FNiagaraTypeDefinition::GetColorDef()) Component->SetVariableLinearColor(VariableName, Parsed.GetValue<FLinearColor>());
	else if (TypeDefinition == FNiagaraTypeDefinition::GetQuatDef())
	{
		const FQuat4f Q = Parsed.GetValue<FQuat4f>();
		Component->SetVariableQuat(VariableName, FQuat(Q.X, Q.Y, Q.Z, Q.W));
	}
	else return SetError(FString::Printf(TEXT("runtime preview setter does not support type '%s'"), *Type));
	return true;
}

bool UUnrealBridgeNiagaraLibrary::ControlNiagaraPreview(const FString& Handle, const FString& Action)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraComponent* Component = FindPreview(Handle);
	if (!Component) return false;
	const FString Token = NormalizeToken(Action);
	if (Token == TEXT("activate") || Token == TEXT("play")) Component->Activate(true);
	else if (Token == TEXT("deactivate") || Token == TEXT("stop")) Component->Deactivate();
	else if (Token == TEXT("stopimmediate") || Token == TEXT("deactivateimmediate")) Component->DeactivateImmediate();
	else if (Token == TEXT("reset")) Component->ResetSystem();
	else if (Token == TEXT("reinitialize") || Token == TEXT("restart")) Component->ReinitializeSystem();
	else if (Token == TEXT("pause")) Component->SetPaused(true);
	else if (Token == TEXT("resume")) Component->SetPaused(false);
	else return SetError(FString::Printf(TEXT("unknown Niagara preview action '%s'"), *Action));
	return true;
}

bool UUnrealBridgeNiagaraLibrary::RemoveNiagaraPreview(const FString& Handle)
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	UNiagaraComponent* Component = FindPreview(Handle);
	if (!Component) return false;
	PreviewComponents.Remove(Handle);
	Component->DeactivateImmediate();
	Component->DestroyComponent();
	return true;
}

int32 UUnrealBridgeNiagaraLibrary::RemoveAllNiagaraPreviews()
{
	using namespace BridgeNiagaraImpl;
	ClearError();
	int32 Removed = 0;
	for (auto& Pair : PreviewComponents)
	{
		if (UNiagaraComponent* Component = Pair.Value.Get())
		{
			Component->DeactivateImmediate();
			Component->DestroyComponent();
			++Removed;
		}
	}
	PreviewComponents.Reset();
	return Removed;
}

#undef LOCTEXT_NAMESPACE

#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)
