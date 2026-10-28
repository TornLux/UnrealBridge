#include "UnrealBridgeGeometryLibrary.h"

#include "Misc/EngineVersionComparison.h"

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Editor.h"                          // GEditor
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"                     // TActorIterator
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/MeshRemeshFunctions.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshBakeFunctions.h"
#include "Engine/Texture2D.h"
#include "Components/SplineComponent.h"

#define LOCTEXT_NAMESPACE "UnrealBridgeGeometry"

namespace BridgeGeometryImpl
{
	// Process-global handle pool. UDynamicMesh* held via TStrongObjectPtr so
	// it survives GC until the caller releases the handle (pit #7 in roadmap).
	// Indexed by monotonically-increasing int — never reused so stale handles
	// fail loudly via the existence check rather than aliasing a fresh mesh.
	struct FHandlePool
	{
		TMap<int32, TStrongObjectPtr<UDynamicMesh>> Map;
		int32 NextHandle = 1;
	};

	FHandlePool& GetPool()
	{
		static FHandlePool Pool;
		return Pool;
	}

	UDynamicMesh* ResolveHandle(int32 Handle)
	{
		FHandlePool& Pool = GetPool();
		TStrongObjectPtr<UDynamicMesh>* Found = Pool.Map.Find(Handle);
		if (!Found)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: handle %d is not registered"), Handle);
			return nullptr;
		}
		UDynamicMesh* Mesh = Found->Get();
		if (!Mesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: handle %d resolved to null (already collected)"), Handle);
			return nullptr;
		}
		return Mesh;
	}

	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	// Mirror UnrealBridgeLevelLibrary actor-lookup conventions: try FName first,
	// then user-visible label. Editor world only.
	AActor* FindActor(UWorld* World, const FString& NameOrLabel)
	{
		if (!World || NameOrLabel.IsEmpty())
		{
			return nullptr;
		}
		const FName AsName(*NameOrLabel);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A)
			{
				continue;
			}
			if (A->GetFName() == AsName || A->GetActorLabel() == NameOrLabel)
			{
				return A;
			}
		}
		return nullptr;
	}

	// Process-global selection pool, parallel to FHandlePool. Selections are
	// value-type structs so plain map storage suffices (no GC concern).
	struct FSelectionPool
	{
		TMap<int32, FGeometryScriptMeshSelection> Map;
		int32 NextId = 1;
	};

	FSelectionPool& GetSelectionPool()
	{
		static FSelectionPool Pool;
		return Pool;
	}

	FGeometryScriptMeshSelection* ResolveSelection(int32 SelectionId)
	{
		FSelectionPool& Pool = GetSelectionPool();
		FGeometryScriptMeshSelection* Found = Pool.Map.Find(SelectionId);
		if (!Found)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: selection %d is not registered"), SelectionId);
		}
		return Found;
	}

	USplineComponent* FindSpline(AActor* Actor, const FString& ComponentName)
	{
		if (!Actor)
		{
			return nullptr;
		}
		TArray<USplineComponent*> Splines;
		Actor->GetComponents<USplineComponent>(Splines);
		if (ComponentName.IsEmpty())
		{
			return Splines.Num() > 0 ? Splines[0] : nullptr;
		}
		const FName AsName(*ComponentName);
		for (USplineComponent* S : Splines)
		{
			if (S && (S->GetFName() == AsName || S->GetName() == ComponentName))
			{
				return S;
			}
		}
		return nullptr;
	}

	EGeometryScriptBakeResolution PixelsToBakeResolution(int32 Pixels)
	{
		switch (Pixels)
		{
			case 16:    return EGeometryScriptBakeResolution::Resolution16;
			case 32:    return EGeometryScriptBakeResolution::Resolution32;
			case 64:    return EGeometryScriptBakeResolution::Resolution64;
			case 128:   return EGeometryScriptBakeResolution::Resolution128;
			case 256:   return EGeometryScriptBakeResolution::Resolution256;
			case 512:   return EGeometryScriptBakeResolution::Resolution512;
			case 1024:  return EGeometryScriptBakeResolution::Resolution1024;
			case 2048:  return EGeometryScriptBakeResolution::Resolution2048;
			case 4096:  return EGeometryScriptBakeResolution::Resolution4096;
			case 8192:  return EGeometryScriptBakeResolution::Resolution8192;
			default:    return EGeometryScriptBakeResolution::Resolution256;
		}
	}

	bool ParseBooleanOp(const FString& Op, EGeometryScriptBooleanOperation& Out)
	{
		const FString Lower = Op.ToLower();
		if (Lower == TEXT("union"))                                  { Out = EGeometryScriptBooleanOperation::Union;        return true; }
		if (Lower == TEXT("intersect") || Lower == TEXT("intersection")) { Out = EGeometryScriptBooleanOperation::Intersection; return true; }
		if (Lower == TEXT("subtract") || Lower == TEXT("difference"))   { Out = EGeometryScriptBooleanOperation::Subtract;     return true; }
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge|Geometry: unknown boolean op '%s' (expected union / intersect / subtract)"),
			*Op);
		return false;
	}

	USceneComponent* FindComponent(AActor* Actor, const FString& ComponentName)
	{
		if (!Actor)
		{
			return nullptr;
		}
		if (ComponentName.IsEmpty())
		{
			return Actor->GetRootComponent();
		}
		const FName AsName(*ComponentName);
		TArray<USceneComponent*> Components;
		Actor->GetComponents<USceneComponent>(Components);
		for (USceneComponent* C : Components)
		{
			if (C && (C->GetFName() == AsName || C->GetName() == ComponentName))
			{
				return C;
			}
		}
		return nullptr;
	}
}

int32 UUnrealBridgeGeometryLibrary::CreateDynamicMesh()
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* NewMesh = NewObject<UDynamicMesh>(GetTransientPackage());
	if (!NewMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealBridge|Geometry: NewObject<UDynamicMesh> returned null"));
		return 0;
	}
	FHandlePool& Pool = GetPool();
	const int32 Handle = Pool.NextHandle++;
	Pool.Map.Add(Handle, TStrongObjectPtr<UDynamicMesh>(NewMesh));
	return Handle;
}

bool UUnrealBridgeGeometryLibrary::ReleaseDynamicMesh(int32 Handle)
{
	using namespace BridgeGeometryImpl;
	FHandlePool& Pool = GetPool();
	return Pool.Map.Remove(Handle) > 0;
}

TArray<int32> UUnrealBridgeGeometryLibrary::ListDynamicMeshHandles()
{
	using namespace BridgeGeometryImpl;
	TArray<int32> Out;
	GetPool().Map.GenerateKeyArray(Out);
	Out.Sort();
	return Out;
}

bool UUnrealBridgeGeometryLibrary::LoadMeshFromStaticMesh(int32 Handle, const FString& AssetPath, int32 Lod)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Target = ResolveHandle(Handle);
	if (!Target)
	{
		return false;
	}
	UStaticMesh* Source = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!Source)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: could not load StaticMesh '%s'"), *AssetPath);
		return false;
	}

	FGeometryScriptCopyMeshFromAssetOptions Options;
	FGeometryScriptMeshReadLOD RequestedLOD;
	RequestedLOD.LODType  = EGeometryScriptLODType::SourceModel;
	RequestedLOD.LODIndex = Lod;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;

	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMeshV2(
		Source, Target, Options, RequestedLOD, Outcome,
		/*bUseSectionMaterials=*/true, /*Debug=*/nullptr);

	return Outcome == EGeometryScriptOutcomePins::Success;
}

bool UUnrealBridgeGeometryLibrary::LoadMeshFromComponent(const FString& ActorLabel, const FString& ComponentName, int32 Handle)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Target = ResolveHandle(Handle);
	if (!Target)
	{
		return false;
	}
	UWorld* World = GetEditorWorld();
	AActor* Actor = FindActor(World, ActorLabel);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: actor '%s' not found in editor world"), *ActorLabel);
		return false;
	}
	USceneComponent* Component = FindComponent(Actor, ComponentName);
	if (!Component)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: component '%s' not found on actor '%s'"),
			*ComponentName, *ActorLabel);
		return false;
	}

	FGeometryScriptCopyMeshFromComponentOptions Options;
	FTransform LocalToWorld = FTransform::Identity;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;

	UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(
		Component, Target, Options,
		/*bTransformToWorld=*/false, LocalToWorld, Outcome, /*Debug=*/nullptr);

	return Outcome == EGeometryScriptOutcomePins::Success;
}

FString UUnrealBridgeGeometryLibrary::SaveMeshToNewStaticMesh(int32 Handle, const FString& NewAssetPath, const TArray<UMaterialInterface*>& MaterialList)
{
#if WITH_EDITOR
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Source = ResolveHandle(Handle);
	if (!Source)
	{
		return FString{};
	}

	FGeometryScriptCreateNewStaticMeshAssetOptions Options;
	// Defaults are conservative: don't recompute normals/tangents (caller can use
	// M6-3 explicitly), Nanite off, collision on, default trace flag.
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;

	UStaticMesh* NewAsset = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(
		Source, NewAssetPath, Options, Outcome, /*Debug=*/nullptr);

	if (Outcome != EGeometryScriptOutcomePins::Success || !NewAsset)
	{
		return FString{};
	}

	// Apply user-supplied materials. CreateNewStaticMeshAssetFromMesh leaves the
	// MaterialSlot list with default material; we overwrite slot N with
	// MaterialList[N] for as many entries as the caller supplied.
	if (MaterialList.Num() > 0)
	{
		TArray<FStaticMaterial>& Slots = NewAsset->GetStaticMaterials();
		for (int32 i = 0; i < MaterialList.Num() && i < Slots.Num(); ++i)
		{
			if (MaterialList[i])
			{
				Slots[i].MaterialInterface = MaterialList[i];
			}
		}
		NewAsset->Modify();
		NewAsset->MarkPackageDirty();
	}

	return NewAsset->GetPathName();
#else
	(void)Handle; (void)NewAssetPath; (void)MaterialList;
	UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: SaveMeshToNewStaticMesh requires WITH_EDITOR"));
	return FString{};
#endif
}

bool UUnrealBridgeGeometryLibrary::SaveMeshToExistingStaticMesh(int32 Handle, const FString& ExistingAssetPath, bool bReplaceMaterials)
{
#if WITH_EDITOR
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Source = ResolveHandle(Handle);
	if (!Source)
	{
		return false;
	}
	UStaticMesh* Target = LoadObject<UStaticMesh>(nullptr, *ExistingAssetPath);
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: could not load existing StaticMesh '%s'"), *ExistingAssetPath);
		return false;
	}

	FGeometryScriptCopyMeshToAssetOptions Options;
	Options.bReplaceMaterials       = bReplaceMaterials;
	Options.bEmitTransaction        = true;
	Options.bDeferMeshPostEditChange = false;

	FGeometryScriptMeshWriteLOD TargetLOD;  // defaults: SourceModel LOD0
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;

	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		Source, Target, Options, TargetLOD, Outcome,
		/*bUseSectionMaterials=*/true, /*Debug=*/nullptr);

	if (Outcome != EGeometryScriptOutcomePins::Success)
	{
		return false;
	}

	Target->Modify();
	Target->MarkPackageDirty();
	return true;
#else
	(void)Handle; (void)ExistingAssetPath; (void)bReplaceMaterials;
	UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: SaveMeshToExistingStaticMesh requires WITH_EDITOR"));
	return false;
#endif
}

FBridgeMeshInfo UUnrealBridgeGeometryLibrary::GetMeshInfo(int32 Handle)
{
	using namespace BridgeGeometryImpl;
	FBridgeMeshInfo Info;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return Info;
	}
	Info.NumTriangles    = Mesh->GetTriangleCount();
	Info.NumVertices     = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
	Info.NumUVLayers     = UGeometryScriptLibrary_MeshQueryFunctions::GetNumUVSets(Mesh);
	Info.bHasNormals     = UGeometryScriptLibrary_MeshQueryFunctions::GetHasTriangleNormals(Mesh);
	Info.bHasVertexColors = UGeometryScriptLibrary_MeshQueryFunctions::GetHasVertexColors(Mesh);
	Info.Bounds          = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
	return Info;
}

TArray<FVector> UUnrealBridgeGeometryLibrary::GetMeshTriangleVertices(int32 Handle, int32 MaxTriangles)
{
	TArray<FVector> Vertices;
	UDynamicMesh* Mesh = BridgeGeometryImpl::ResolveHandle(Handle);
	if (!Mesh || MaxTriangles < 1 || MaxTriangles > 1000000)
	{
		return Vertices;
	}
	Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh)
	{
		if (ReadMesh.TriangleCount() > MaxTriangles)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: triangle read exceeds limit %d"), MaxTriangles);
			return;
		}
		Vertices.Reserve(ReadMesh.TriangleCount() * 3);
		for (int32 TriangleId : ReadMesh.TriangleIndicesItr())
		{
			const UE::Geometry::FIndex3i Triangle = ReadMesh.GetTriangle(TriangleId);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Vertices.Add(FVector(ReadMesh.GetVertex(Triangle[Corner])));
			}
		}
	});
	return Vertices;
}

bool UUnrealBridgeGeometryLibrary::AppendDynamicMesh(int32 TargetHandle, int32 SourceHandle, FTransform Transform)
{
	using namespace BridgeGeometryImpl;
	if (TargetHandle == SourceHandle || !Transform.IsValid()) return false;
	UDynamicMesh* Target = ResolveHandle(TargetHandle);
	UDynamicMesh* Source = ResolveHandle(SourceHandle);
	if (!Target || !Source) return false;
	return UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
		Target, Source, Transform, false, FGeometryScriptAppendMeshOptions(), nullptr) == Target;
}

bool UUnrealBridgeGeometryLibrary::MeshBoolean(int32 HandleA, int32 HandleB, const FString& Op)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Target = ResolveHandle(HandleA);
	UDynamicMesh* Tool   = ResolveHandle(HandleB);
	if (!Target || !Tool)
	{
		return false;
	}
	EGeometryScriptBooleanOperation Operation;
	if (!ParseBooleanOp(Op, Operation))
	{
		return false;
	}

	FGeometryScriptMeshBooleanOptions Options;  // bFillHoles=true, bSimplifyOutput=true (engine defaults)
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		Target, FTransform::Identity,
		Tool,   FTransform::Identity,
		Operation, Options, /*Debug=*/nullptr);

	// ApplyMeshBoolean returns its TargetMesh on success; null/empty result is the failure marker.
	return Result == Target && Target->GetTriangleCount() > 0;
}

bool UUnrealBridgeGeometryLibrary::MeshSmooth(int32 Handle, int32 Iterations, float Strength)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	FGeometryScriptIterativeMeshSmoothingOptions Options;
	Options.NumIterations = FMath::Max(1, Iterations);
	Options.Alpha         = FMath::Clamp(Strength, 0.0f, 1.0f);

	FGeometryScriptMeshSelection EmptySelection;  // empty = whole mesh
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(
		Mesh, EmptySelection, Options, /*Debug=*/nullptr);

	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::MeshDecimate(int32 Handle, int32 TargetTris)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	const int32 ClampedTarget = FMath::Max(4, TargetTris);
	FGeometryScriptSimplifyMeshOptions Options;  // AttributeAware default
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(
		Mesh, ClampedTarget, Options, /*Debug=*/nullptr);

	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::AppendBox(int32 Handle, FVector Origin, FVector Size)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	FGeometryScriptPrimitiveOptions Options;  // PolyGroupMode::PerFace, etc. — engine defaults.
	const FTransform Xform(Origin);
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		Mesh, Options, Xform,
		FMath::Max(0.f, Size.X), FMath::Max(0.f, Size.Y), FMath::Max(0.f, Size.Z),
		/*StepsX=*/0, /*StepsY=*/0, /*StepsZ=*/0,
		EGeometryScriptPrimitiveOriginMode::Center,
		/*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::AppendSphere(int32 Handle, FVector Origin, float Radius, int32 ResolutionUV)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	FGeometryScriptPrimitiveOptions Options;
	const FTransform Xform(Origin);
	const int32 StepsTheta = FMath::Max(3, ResolutionUV);          // longitude segments
	const int32 StepsPhi   = FMath::Max(2, ResolutionUV / 2);      // latitude rings
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(
		Mesh, Options, Xform,
		FMath::Max(0.f, Radius), StepsPhi, StepsTheta,
		EGeometryScriptPrimitiveOriginMode::Center,
		/*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::AppendCylinder(int32 Handle, FVector Origin, float Radius, float Height, int32 RadialSegments)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	FGeometryScriptPrimitiveOptions Options;
	const FTransform Xform(Origin);
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
		Mesh, Options, Xform,
		FMath::Max(0.f, Radius), FMath::Max(0.f, Height),
		FMath::Max(3, RadialSegments), /*HeightSteps=*/0, /*bCapped=*/true,
		EGeometryScriptPrimitiveOriginMode::Base,
		/*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::AppendCone(int32 Handle, FVector Origin, float BaseRadius, float Height, int32 RadialSegments)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	FGeometryScriptPrimitiveOptions Options;
	const FTransform Xform(Origin);
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(
		Mesh, Options, Xform,
		FMath::Max(0.f, BaseRadius), /*TopRadius=*/0.f,
		FMath::Max(0.f, Height),
		FMath::Max(3, RadialSegments), /*HeightSteps=*/4,
		/*bCapped=*/true,
		EGeometryScriptPrimitiveOriginMode::Base,
		/*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::MeshTransform(int32 Handle, FTransform Transform)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(
		Mesh, Transform, /*bFixOrientationForNegativeScale=*/true, /*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::MeshUniformRemesh(int32 Handle, int32 TargetTriCount)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	FGeometryScriptRemeshOptions          RemeshOptions;       // bReprojectToInputMesh=true, SmoothingRate=0.25 (defaults)
	FGeometryScriptUniformRemeshOptions   UniformOptions;
	UniformOptions.TargetType          = EGeometryScriptUniformRemeshTargetType::TriangleCount;
	UniformOptions.TargetTriangleCount = FMath::Max(4, TargetTriCount);

	UDynamicMesh* Result = UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(
		Mesh, RemeshOptions, UniformOptions, /*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::MeshDisplaceFromTexture(int32 Handle, const FString& TexturePath, float Magnitude, int32 UVChannel)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!Tex)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: could not load Texture2D '%s'"), *TexturePath);
		return false;
	}

	FGeometryScriptDisplaceFromTextureOptions Options;
	Options.Magnitude = Magnitude;
	// UVScale, UVOffset, Center, ImageChannel — engine defaults (1, 0, 0.5, 0).

	FGeometryScriptMeshSelection EmptySelection;  // empty = whole mesh
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
	// 5.8 inserted FGeometryScriptAdaptiveTessellationOptions before UVChannel.
	FGeometryScriptAdaptiveTessellationOptions TessellationOptions{};
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshDeformFunctions::ApplyDisplaceFromTextureMap(
		Mesh, Tex, EmptySelection, Options, TessellationOptions, FMath::Max(0, UVChannel), /*Debug=*/nullptr);
#else
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshDeformFunctions::ApplyDisplaceFromTextureMap(
		Mesh, Tex, EmptySelection, Options, FMath::Max(0, UVChannel), /*Debug=*/nullptr);
#endif
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::MeshVoxelMerge(const TArray<int32>& Handles, float CellSizeCm)
{
	using namespace BridgeGeometryImpl;
	if (Handles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: MeshVoxelMerge requires ≥1 handle"));
		return false;
	}
	UDynamicMesh* Target = ResolveHandle(Handles[0]);
	if (!Target)
	{
		return false;
	}

	// Append every subsequent handle's geometry into the target (identity transform).
	for (int32 i = 1; i < Handles.Num(); ++i)
	{
		UDynamicMesh* Source = ResolveHandle(Handles[i]);
		if (!Source)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: MeshVoxelMerge skipping invalid handle %d at index %d"), Handles[i], i);
			continue;
		}
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			Target, Source, FTransform::Identity,
			/*bDeferChangeNotifications=*/true,
			FGeometryScriptAppendMeshOptions(),
			/*Debug=*/nullptr);
	}

	// Solidify the merged result.
	FGeometryScriptSolidifyOptions Options;
	Options.GridParameters.SizeMethod   = EGeometryScriptGridSizingMethod::GridCellSize;
	Options.GridParameters.GridCellSize = FMath::Max(0.001f, CellSizeCm);
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshSolidify(
		Target, Options, /*Debug=*/nullptr);
	return Result == Target;
}

int32 UUnrealBridgeGeometryLibrary::SelectByNormalDirection(int32 Handle, FVector Normal, float MaxAngleDeg)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return 0;
	}
	if (!Normal.Normalize())
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: SelectByNormalDirection received zero normal"));
		return 0;
	}

	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(
		Mesh, Selection, Normal, FMath::Max(0.0f, MaxAngleDeg),
		EGeometryScriptMeshSelectionType::Triangles,
		/*bInvert=*/false,
		/*MinNumTrianglePoints=*/3);

	if (Selection.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: SelectByNormalDirection produced empty selection"));
		return 0;
	}

	FSelectionPool& Pool = GetSelectionPool();
	const int32 Id = Pool.NextId++;
	Pool.Map.Add(Id, Selection);
	return Id;
}

bool UUnrealBridgeGeometryLibrary::ReleaseSelection(int32 SelectionId)
{
	using namespace BridgeGeometryImpl;
	return GetSelectionPool().Map.Remove(SelectionId) > 0;
}

TArray<int32> UUnrealBridgeGeometryLibrary::ListSelections()
{
	using namespace BridgeGeometryImpl;
	TArray<int32> Out;
	GetSelectionPool().Map.GenerateKeyArray(Out);
	Out.Sort();
	return Out;
}

bool UUnrealBridgeGeometryLibrary::ExtrudeSelection(int32 Handle, int32 SelectionId, float Distance)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	FGeometryScriptMeshSelection* Selection = ResolveSelection(SelectionId);
	if (!Selection)
	{
		return false;
	}

	FGeometryScriptMeshLinearExtrudeOptions Options;
	Options.Distance      = Distance;
	Options.DirectionMode = EGeometryScriptLinearExtrudeDirection::AverageFaceNormal;
	// Direction (FixedDirection mode), AreaMode (EntireSelection), GroupOptions (defaults), UVScale=1, bSolidsToShells=true — engine defaults.

	UDynamicMesh* Result = UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(
		Mesh, Options, *Selection, /*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::SweepAlongSpline(
	int32 Handle,
	const TArray<FVector2D>& ProfileXY,
	const FString& ActorLabel,
	const FString& ComponentName,
	int32 NumPathSamples)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	if (ProfileXY.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: SweepAlongSpline requires ≥3 profile vertices"));
		return false;
	}

	UWorld* World = GetEditorWorld();
	AActor* Actor = FindActor(World, ActorLabel);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: SweepAlongSpline could not find actor '%s'"), *ActorLabel);
		return false;
	}
	USplineComponent* Spline = FindSpline(Actor, ComponentName);
	if (!Spline)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge|Geometry: SweepAlongSpline could not find spline component '%s' on actor '%s'"),
			*ComponentName, *ActorLabel);
		return false;
	}

	const int32 SampleCount = FMath::Max(2, NumPathSamples);
	const float TotalLength = Spline->GetSplineLength();
	if (TotalLength <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: SweepAlongSpline got zero-length spline"));
		return false;
	}

	TArray<FTransform> SweepPath;
	SweepPath.Reserve(SampleCount);
	for (int32 i = 0; i < SampleCount; ++i)
	{
		const float Distance = (TotalLength * i) / (SampleCount - 1);
		FTransform T = Spline->GetTransformAtDistanceAlongSpline(
			Distance, ESplineCoordinateSpace::Local, /*bUseScale=*/true);
		SweepPath.Add(T);
	}

	FGeometryScriptPrimitiveOptions Options;
	UDynamicMesh* Result = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
		Mesh, Options, FTransform::Identity,
		ProfileXY, SweepPath,
		/*bLoop=*/Spline->IsClosedLoop(),
		/*bCapped=*/!Spline->IsClosedLoop(),
		/*StartScale=*/1.0f, /*EndScale=*/1.0f,
		/*RotationAngleDeg=*/0.0f, /*MiterLimit=*/1.0f,
		/*Debug=*/nullptr);

	return Result == Mesh;
}

FString UUnrealBridgeGeometryLibrary::BakeNormalsToTexture(int32 Handle, const FString& NewTexturePath, int32 Resolution)
{
#if WITH_EDITOR
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return FString{};
	}

	// Bake target needs tangents on the source UVs. ComputeTangents is idempotent
	// (no-op if tangents already exist; silent no-op if no UV channel — caller's
	// responsibility to ensure the mesh has UVs via mesh_uv_unwrap or load).
	{
		FGeometryScriptTangentsOptions TangentOpts;
		UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(Mesh, TangentOpts, /*Debug=*/nullptr);
	}

	FGeometryScriptBakeTargetMeshOptions TargetOpts;        // TargetUVLayer = 0 (default)
	FGeometryScriptBakeSourceMeshOptions SourceOpts;        // SourceNormalMap = nullptr → use mesh normals
	FGeometryScriptBakeTextureOptions    TextureOpts;
	TextureOpts.Resolution      = PixelsToBakeResolution(Resolution);
	TextureOpts.BitDepth        = EGeometryScriptBakeBitDepth::ChannelBits8;
	TextureOpts.SamplesPerPixel = EGeometryScriptBakeSamplesPerPixel::Sample1;
	TextureOpts.FilteringType   = EGeometryScriptBakeFilteringType::BSpline;

	TArray<FGeometryScriptBakeTypeOptions> BakeTypes;
	BakeTypes.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeTangentNormal());

	const TArray<UTexture2D*> Baked = UGeometryScriptLibrary_MeshBakeFunctions::BakeTexture(
		Mesh, FTransform::Identity, TargetOpts,
		Mesh, FTransform::Identity, SourceOpts,
		BakeTypes, TextureOpts, /*Debug=*/nullptr);

	if (Baked.Num() == 0 || !Baked[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: BakeNormalsToTexture produced no texture"));
		return FString{};
	}

	FGeometryScriptCreateNewTexture2DAssetOptions AssetOpts;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UTexture2D* Saved = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewTexture2DAsset(
		Baked[0], NewTexturePath, AssetOpts, Outcome, /*Debug=*/nullptr);

	if (Outcome != EGeometryScriptOutcomePins::Success || !Saved)
	{
		return FString{};
	}
	return Saved->GetPathName();
#else
	(void)Handle; (void)NewTexturePath; (void)Resolution;
	UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: BakeNormalsToTexture requires WITH_EDITOR"));
	return FString{};
#endif
}

bool UUnrealBridgeGeometryLibrary::BakeOcclusionToVertexColor(int32 Handle, int32 OcclusionRays)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}

	FGeometryScriptBakeTargetMeshOptions TargetOpts;
	FGeometryScriptBakeSourceMeshOptions SourceOpts;
	FGeometryScriptBakeVertexOptions     VertexOpts;        // TopologyMode=CreateNew (default)

	FGeometryScriptBakeOutputType Output;
	Output.OutputMode = EGeometryScriptBakeOutputMode::RGBA;
	Output.RGBA = UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeAmbientOcclusion(
		FMath::Max(1, OcclusionRays),
		/*MaxDistance=*/0.f,
		/*SpreadAngle=*/180.f,
		/*BiasAngle=*/15.f);

	UDynamicMesh* Result = UGeometryScriptLibrary_MeshBakeFunctions::BakeVertex(
		Mesh, FTransform::Identity, TargetOpts,
		Mesh, FTransform::Identity, SourceOpts,
		Output, VertexOpts, /*Debug=*/nullptr);
	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::MeshUVUnwrap(int32 Handle, const FString& Method, float ProjectionSizeCm)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}
	if (!FMath::IsFinite(ProjectionSizeCm) || ProjectionSizeCm < 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: projection size must be finite and nonnegative"));
		return false;
	}

	const FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
	if (!Bounds.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealBridge|Geometry: MeshUVUnwrap on empty mesh"));
		return false;
	}

	// Ensure UV channel 0 exists.
	UGeometryScriptLibrary_MeshUVFunctions::SetNumUVSets(Mesh, FMath::Max(1, 1), /*Debug=*/nullptr);

	const FVector  Center = Bounds.GetCenter();
	const FVector  Size   = ProjectionSizeCm > 0.f ? FVector(ProjectionSizeCm) : Bounds.GetSize();
	const FTransform PlaneXform(FRotator::ZeroRotator, Center, Size);  // FTransform(rot, loc, scale)
	FGeometryScriptMeshSelection EmptySelection;
	UDynamicMesh* Result = nullptr;
	const FString Lower = Method.ToLower();

	if (Lower == TEXT("box"))
	{
		Result = UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
			Mesh, /*UVSetIndex=*/0, PlaneXform, EmptySelection,
			/*MinIslandTriCount=*/2, /*Debug=*/nullptr);
	}
	else if (Lower == TEXT("cylinder"))
	{
		Result = UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromCylinderProjection(
			Mesh, /*UVSetIndex=*/0, PlaneXform, EmptySelection,
			/*SplitAngle=*/45.0f, /*Debug=*/nullptr);
	}
	else if (Lower == TEXT("plane"))
	{
		Result = UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(
			Mesh, /*UVSetIndex=*/0, PlaneXform, EmptySelection, /*Debug=*/nullptr);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnrealBridge|Geometry: unknown UV unwrap method '%s' (expected box / cylinder / plane)"),
			*Method);
		return false;
	}

	return Result == Mesh;
}

bool UUnrealBridgeGeometryLibrary::RecomputeNormalsAndTangents(int32 Handle, float AngleThresholdDeg)
{
	using namespace BridgeGeometryImpl;
	UDynamicMesh* Mesh = ResolveHandle(Handle);
	if (!Mesh)
	{
		return false;
	}

	FGeometryScriptCalculateNormalsOptions CalcOpts;  // bAngleWeighted=true, bAreaWeighted=true (defaults)
	UDynamicMesh* Result = nullptr;

	if (AngleThresholdDeg > 0.0f)
	{
		FGeometryScriptSplitNormalsOptions SplitOpts;
		SplitOpts.bSplitByOpeningAngle = true;
		SplitOpts.OpeningAngleDeg      = AngleThresholdDeg;
		// Other defaults: bSplitByFaceGroup=true (cheap, harmless when no groups exist).
		Result = UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(
			Mesh, SplitOpts, CalcOpts, /*Debug=*/nullptr);
	}
	else
	{
		Result = UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(
			Mesh, CalcOpts, /*bDeferChangeNotifications=*/false, /*Debug=*/nullptr);
	}

	if (Result != Mesh)
	{
		return false;
	}

	// Tangents always second pass — RecomputeNormals doesn't touch the tangent overlay.
	FGeometryScriptTangentsOptions TangentOpts;  // defaults
	UDynamicMesh* TangResult = UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(
		Mesh, TangentOpts, /*Debug=*/nullptr);
	return TangResult == Mesh;
}

#undef LOCTEXT_NAMESPACE

#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)
