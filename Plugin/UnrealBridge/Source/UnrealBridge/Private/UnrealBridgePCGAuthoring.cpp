#include "UnrealBridgePCGLibrary.h"
#include "UnrealBridgeEditorLibrary.h"
#include "Misc/EngineVersionComparison.h"

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGSettings.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace BridgePCGAuthoring
{
	FBridgePCGAuthoringResult Fail(const FString& Error)
	{
		FBridgePCGAuthoringResult Result;
		Result.Error = Error;
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge PCG: %s"), *Error);
		return Result;
	}

	UWorld* World()
	{
		return GEditor && !GEditor->PlayWorld ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	AActor* FindActor(const FString& Name)
	{
		UWorld* EditorWorld = World();
		if (!EditorWorld) return nullptr;
		for (TActorIterator<AActor> It(EditorWorld); It; ++It)
		{
			if (It->GetName() == Name || It->GetActorLabel() == Name) return *It;
		}
		return nullptr;
	}

	UPCGGraph* Graph(const FString& Path)
	{
		return LoadObject<UPCGGraph>(nullptr, *FPackageName::ExportTextPathToObjectPath(Path));
	}

	bool WritablePackage(const FString& Package)
	{
		FString Filename;
		return FPackageName::TryConvertLongPackageNameToFilename(Package, Filename)
			&& FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Filename), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	}

	bool Editable(UPCGGraph* InGraph)
	{
		if (!InGraph || !World()) return false;
		const FString Package = InGraph->GetOutermost()->GetName();
		return WritablePackage(Package);
	}

	UPCGNode* Node(UPCGGraph* InGraph, const FString& Path)
	{
		UPCGNode* Found = FindObject<UPCGNode>(nullptr, *Path);
		return Found && Found->GetGraph() == InGraph ? Found : nullptr;
	}

	bool ValidPoints(const TArray<FVector>& Points)
	{
		return Points.Num() >= 2 && Points.Num() <= 10000 && !Points.ContainsByPredicate([](const FVector& P) { return P.ContainsNaN(); });
	}

	template<typename T>
	T* Component(AActor* Actor, const FString& Name)
	{
		if (!Actor) return nullptr;
		TInlineComponentArray<T*> Components(Actor);
		for (T* C : Components) if (Name.IsEmpty() || C->GetName() == Name) return C;
		return nullptr;
	}
}

FBridgePCGAuthoringResult UUnrealBridgePCGLibrary::CreatePCGGraph(const FString& AssetPath, bool bSave)
{
	using namespace BridgePCGAuthoring;
	if (!World()) return Fail(TEXT("An editor world without PIE is required."));
	if (!FPackageName::IsValidLongPackageName(AssetPath) || AssetPath.Contains(TEXT(".")) || !WritablePackage(AssetPath))
		return Fail(TEXT("Use a writable project/plugin package path without an object suffix."));
	if (FPackageName::DoesPackageExist(AssetPath) || FindPackage(nullptr, *AssetPath)) return Fail(TEXT("Target package already exists; no overwrite performed."));
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "CreatePCGGraph", "Create PCG Graph"));
	UPackage* Package = CreatePackage(*AssetPath);
	UPCGGraph* NewGraph = NewObject<UPCGGraph>(Package, *FPackageName::GetLongPackageAssetName(AssetPath), RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(NewGraph);
	NewGraph->MarkPackageDirty();
	FBridgePCGAuthoringResult Result;
	Result.ObjectPath = NewGraph->GetPathName();
	Result.bSuccess = !bSave || UUnrealBridgeEditorLibrary::SaveAsset(Result.ObjectPath);
	if (!Result.bSuccess) Result.Error = TEXT("Graph created in memory but save failed; preserve the returned object.");
	return Result;
}

FBridgePCGAuthoringResult UUnrealBridgePCGLibrary::AddPCGGraphNode(const FString& GraphPath, const FString& SettingsClassPath, const FString& Title, int32 PositionX, int32 PositionY)
{
	using namespace BridgePCGAuthoring;
	UPCGGraph* TargetGraph = Graph(GraphPath);
	UClass* Class = LoadObject<UClass>(nullptr, *SettingsClassPath);
	if (!Editable(TargetGraph)) return Fail(TEXT("Graph must be writable project/plugin content in the editor."));
	if (!Class || !Class->IsChildOf(UPCGSettings::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) return Fail(TEXT("SettingsClassPath must name a concrete PCG settings class."));
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "AddPCGNode", "Add PCG Node"));
	TargetGraph->Modify();
	UPCGSettings* Settings = nullptr;
	UPCGNode* Added = TargetGraph->AddNodeOfType(Class, Settings);
	if (!Added || !Settings) return Fail(TEXT("Native PCG node creation failed."));
	Added->SetNodePosition(PositionX, PositionY);
	if (!Title.IsEmpty()) Added->SetNodeTitle(FName(*Title));
	TargetGraph->MarkPackageDirty();
	FBridgePCGAuthoringResult Result;
	Result.bSuccess = true;
	Result.ObjectPath = Added->GetPathName();
	Result.SettingsPath = Settings->GetPathName();
	return Result;
}

FBridgePCGAuthoringResult UUnrealBridgePCGLibrary::ConnectPCGGraphNodes(const FString& GraphPath, const FString& SourceNodePath, const FString& SourcePin, const FString& TargetNodePath, const FString& TargetPin)
{
	using namespace BridgePCGAuthoring;
	UPCGGraph* TargetGraph = Graph(GraphPath);
	if (!Editable(TargetGraph)) return Fail(TEXT("Graph must be writable project/plugin content in the editor."));
	UPCGNode* Source = Node(TargetGraph, SourceNodePath);
	UPCGNode* Target = Node(TargetGraph, TargetNodePath);
	if (!Source || !Target || Source == Target || !Source->GetOutputPin(FName(*SourcePin)) || !Target->GetInputPin(FName(*TargetPin))) return Fail(TEXT("Nodes must belong to this graph and pin labels/directions must match."));
	UPCGPin* Output = Source->GetOutputPin(FName(*SourcePin));
	UPCGPin* Input = Target->GetInputPin(FName(*TargetPin));
	for (const UPCGEdge* Edge : Output->Edges)
	{
		if (Edge && Edge->OutputPin == Input)
		{
			FBridgePCGAuthoringResult Existing;
			Existing.bSuccess = true;
			Existing.ObjectPath = TargetGraph->GetPathName();
			return Existing;
		}
	}
	if (!Input->Properties.AllowsMultipleConnections() && !Input->Edges.IsEmpty()) return Fail(TEXT("Input already has a connection; no existing edge was replaced."));
	TSet<UPCGNode*> Visited;
	TArray<UPCGNode*> Pending{Target};
	while (!Pending.IsEmpty())
	{
		UPCGNode* Current = Pending.Pop(EAllowShrinking::No);
		if (Current == Source) return Fail(TEXT("Connection would create a graph cycle."));
		if (Visited.Contains(Current)) continue;
		Visited.Add(Current);
		for (UPCGPin* Pin : Current->GetOutputPins())
			for (UPCGEdge* Edge : Pin->Edges)
				if (Edge && Edge->OutputPin && Edge->OutputPin->Node) Pending.Add(Edge->OutputPin->Node);
	}
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "ConnectPCGNodes", "Connect PCG Nodes"));
	TargetGraph->Modify();
	Source->Modify();
	Target->Modify();
	if (!TargetGraph->AddEdge(Source, FName(*SourcePin), Target, FName(*TargetPin))) return Fail(TEXT("Native PCG rejected the connection."));
	TargetGraph->MarkPackageDirty();
	FBridgePCGAuthoringResult Result;
	Result.bSuccess = true;
	Result.ObjectPath = TargetGraph->GetPathName();
	return Result;
}

TArray<FBridgePCGNodeInfo> UUnrealBridgePCGLibrary::GetPCGGraphNodes(const FString& GraphPath)
{
	TArray<FBridgePCGNodeInfo> Result;
	UPCGGraph* Graph = BridgePCGAuthoring::Graph(GraphPath);
	if (!Graph) return Result;
	TArray<UPCGNode*> Nodes = Graph->GetNodes();
	Nodes.AddUnique(Graph->GetInputNode());
	Nodes.AddUnique(Graph->GetOutputNode());
	for (UPCGNode* Node : Nodes)
	{
		if (!Node) continue;
		FBridgePCGNodeInfo Info;
		Info.NodePath = Node->GetPathName();
		Info.Title = Node->GetAuthoredTitleName().ToString();
		if (UPCGSettings* Settings = Node->GetSettings())
		{
			Info.SettingsPath = Settings->GetPathName();
			Info.SettingsClassPath = Settings->GetClass()->GetPathName();
		}
		for (const UPCGPin* Pin : Node->GetInputPins()) if (Pin) Info.InputPins.Add(Pin->Properties.Label.ToString());
		for (const UPCGPin* Pin : Node->GetOutputPins()) if (Pin) Info.OutputPins.Add(Pin->Properties.Label.ToString());
		Node->GetNodePosition(Info.PositionX, Info.PositionY);
		Result.Add(MoveTemp(Info));
	}
	return Result;
}

FBridgePCGAuthoringResult UUnrealBridgePCGLibrary::ConfigurePCGStaticMeshSpawner(const FString& GraphPath, const FString& NodePath, const TArray<FString>& MeshPaths, const TArray<int32>& Weights, const TArray<FString>& MaterialOverrides, const FString& CollisionProfileName, bool bCastShadow)
{
	using namespace BridgePCGAuthoring;
	UPCGGraph* TargetGraph = Graph(GraphPath);
	UPCGNode* TargetNode = TargetGraph ? Node(TargetGraph, NodePath) : nullptr;
	UPCGStaticMeshSpawnerSettings* Settings = TargetNode ? Cast<UPCGStaticMeshSpawnerSettings>(TargetNode->GetSettings()) : nullptr;
	if (!Editable(TargetGraph) || !Settings) return Fail(TEXT("A mesh-spawner node in a writable graph is required."));
	if (MeshPaths.IsEmpty() || MeshPaths.Num() > 256 || (!Weights.IsEmpty() && Weights.Num() != MeshPaths.Num())) return Fail(TEXT("Supply 1..256 meshes and either no weights or one positive weight per mesh."));
	FCollisionResponseTemplate Profile;
	if (!UCollisionProfile::Get()->GetProfileTemplate(FName(*CollisionProfileName), Profile)) return Fail(TEXT("Collision profile does not exist."));
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
	for (const FString& Path : MaterialOverrides)
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Path);
		if (!Material) return Fail(FString::Printf(TEXT("Material cannot be loaded: %s"), *Path));
		Materials.Add(Material);
	}
	TArray<FPCGMeshSelectorWeightedEntry> Entries;
	for (int32 Index = 0; Index < MeshPaths.Num(); ++Index)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPaths[Index]);
		const int32 Weight = Weights.IsEmpty() ? 1 : Weights[Index];
		if (!Mesh || Weight <= 0) return Fail(FString::Printf(TEXT("Mesh/weight is invalid at index %d."), Index));
		FPCGMeshSelectorWeightedEntry& Entry = Entries.Emplace_GetRef(Mesh, Weight);
		Entry.Descriptor.OverrideMaterials = Materials;
		Entry.Descriptor.BodyInstance.SetCollisionProfileName(FName(*CollisionProfileName));
		Entry.Descriptor.bCastShadow = bCastShadow;
		Entry.Descriptor.Mobility = EComponentMobility::Static;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "ConfigurePCGSpawner", "Configure PCG Mesh Spawner"));
	TargetGraph->Modify();
	Settings->Modify();
	Settings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
	UPCGMeshSelectorWeighted* Selector = Cast<UPCGMeshSelectorWeighted>(Settings->MeshSelectorParameters);
	if (!Selector) return Fail(TEXT("Native weighted mesh selector could not be created."));
	Selector->Modify();
	Selector->MeshEntries = MoveTemp(Entries);
	Selector->RefreshDisplayNames();
	Settings->PostEditChange();
	TargetGraph->PostEditChange();
	TargetGraph->MarkPackageDirty();
	FBridgePCGAuthoringResult Result;
	Result.bSuccess = true;
	Result.ObjectPath = TargetNode->GetPathName();
	Result.SettingsPath = Settings->GetPathName();
	return Result;
}

FBridgePCGAuthoringResult UUnrealBridgePCGLibrary::SetPCGComponentGraph(const FString& ActorLabel, const FString& ComponentName, const FString& GraphPath)
{
	using namespace BridgePCGAuthoring;
	AActor* Actor = FindActor(ActorLabel);
	UPCGComponent* PCG = Component<UPCGComponent>(Actor, ComponentName);
	UPCGGraph* NewGraph = Graph(GraphPath);
	if (!PCG || !NewGraph) return Fail(TEXT("Editor actor, PCG component or graph was not found."));
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "SetPCGGraph", "Set PCG Component Graph"));
	Actor->Modify();
	PCG->Modify();
	PCG->SetGraph(NewGraph);
	Actor->MarkPackageDirty();
	FBridgePCGAuthoringResult Result;
	Result.bSuccess = true;
	Result.ObjectPath = PCG->GetPathName();
	Result.ActorLabel = Actor->GetActorLabel();
	Result.ComponentName = PCG->GetName();
	return Result;
}

FBridgePCGAuthoringResult UUnrealBridgePCGLibrary::CreatePCGSplineActor(const FString& GraphPath, const FString& ActorLabel, const TArray<FVector>& Points, int32 Seed)
{
	using namespace BridgePCGAuthoring;
	UWorld* EditorWorld = World();
	UPCGGraph* TargetGraph = Graph(GraphPath);
	if (!EditorWorld || !TargetGraph || ActorLabel.TrimStartAndEnd().IsEmpty() || FindActor(ActorLabel) || !ValidPoints(Points)) return Fail(TEXT("Requires an editor world, graph, unique nonempty label and 2..10000 finite world-space points."));
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "CreatePCGSplineActor", "Create PCG Spline Actor"));
	EditorWorld->Modify();
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transactional;
	AActor* Actor = EditorWorld->SpawnActor<AActor>(Spawn);
	if (!Actor) return Fail(TEXT("Actor creation failed."));
	Actor->SetActorLabel(ActorLabel);
	USplineComponent* Spline = NewObject<USplineComponent>(Actor, TEXT("Spline"), RF_Transactional);
	Actor->AddInstanceComponent(Spline);
	Actor->SetRootComponent(Spline);
	Spline->RegisterComponent();
	Spline->SetSplinePoints(Points, ESplineCoordinateSpace::World, false);
	for (int32 Index = 0; Index < Points.Num(); ++Index) Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	Spline->SetOverrideConstructionScript(true);
	Spline->UpdateSpline();
	UPCGComponent* PCG = NewObject<UPCGComponent>(Actor, TEXT("PCG"), RF_Transactional);
	Actor->AddInstanceComponent(PCG);
	PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	PCG->Seed = Seed;
	PCG->RegisterComponent();
	PCG->SetGraph(TargetGraph);
	Actor->MarkPackageDirty();
	FBridgePCGAuthoringResult Result;
	Result.bSuccess = true;
	Result.ObjectPath = Actor->GetPathName();
	Result.ActorLabel = Actor->GetActorLabel();
	Result.ComponentName = PCG->GetName();
	return Result;
}

FBridgePCGAuthoringResult UUnrealBridgePCGLibrary::SetPCGSplinePoints(const FString& ActorLabel, const FString& ComponentName, const TArray<FVector>& Points, bool bClosedLoop)
{
	using namespace BridgePCGAuthoring;
	AActor* Actor = FindActor(ActorLabel);
	USplineComponent* Spline = Component<USplineComponent>(Actor, ComponentName);
	if (!Spline || !ValidPoints(Points)) return Fail(TEXT("Requires an editor spline component and 2..10000 finite world-space points."));
	const FScopedTransaction Transaction(NSLOCTEXT("UnrealBridge", "SetPCGSpline", "Set PCG Spline Points"));
	Actor->Modify();
	Spline->Modify();
	Spline->SetSplinePoints(Points, ESplineCoordinateSpace::World, false);
	for (int32 Index = 0; Index < Points.Num(); ++Index) Spline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	Spline->SetClosedLoop(bClosedLoop, false);
	Spline->SetOverrideConstructionScript(true);
	Spline->UpdateSpline();
	TInlineComponentArray<UPCGComponent*> Components(Actor);
	// Spline data is cached in Actor and aggregate PCG data, not only Input.
	// 同时失效 Actor 与聚合缓存，否则强制生成仍可能读取旧曲线。
	for (UPCGComponent* PCG : Components) PCG->DirtyGenerated(EPCGComponentDirtyFlag::All);
	Actor->MarkPackageDirty();
	FBridgePCGAuthoringResult Result;
	Result.bSuccess = true;
	Result.ObjectPath = Spline->GetPathName();
	Result.ActorLabel = Actor->GetActorLabel();
	Result.ComponentName = Spline->GetName();
	return Result;
}

TArray<FBridgePCGMeshOutput> UUnrealBridgePCGLibrary::GetPCGActorMeshOutputs(const FString& ActorLabel)
{
	TArray<FBridgePCGMeshOutput> Result;
	AActor* Actor = BridgePCGAuthoring::FindActor(ActorLabel);
	if (!Actor) return Result;
	TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
	for (UStaticMeshComponent* C : Components)
	{
		UStaticMesh* Mesh = C->GetStaticMesh();
		if (!Mesh) continue;
		FBridgePCGMeshOutput Item;
		Item.ComponentName = C->GetName();
		Item.MeshPath = Mesh->GetPathName();
		const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(C);
		Item.InstanceCount = ISM ? ISM->GetInstanceCount() : 1;
		Item.CollisionProfileName = C->GetCollisionProfileName().ToString();
		const FBox Bounds = C->Bounds.GetBox();
		Item.BoundsMin = Bounds.Min;
		Item.BoundsMax = Bounds.Max;
		for (int32 I = 0; I < C->GetNumMaterials(); ++I) Item.MaterialPaths.Add(C->GetMaterial(I) ? C->GetMaterial(I)->GetPathName() : FString());
		Result.Add(MoveTemp(Item));
	}
	return Result;
}

#endif
