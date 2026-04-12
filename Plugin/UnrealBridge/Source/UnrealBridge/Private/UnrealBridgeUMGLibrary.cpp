#include "UnrealBridgeUMGLibrary.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePossessable.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_ComponentBoundEvent.h"

// ─── Helpers ────────────────────────────────────────────────

namespace
{
	UWidgetBlueprint* LoadWBP(const FString& Path)
	{
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *Path);
		if (!WBP)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load Widget Blueprint '%s'"), *Path);
		}
		return WBP;
	}

	FString VisibilityToString(ESlateVisibility V)
	{
		switch (V)
		{
		case ESlateVisibility::Visible:				return TEXT("Visible");
		case ESlateVisibility::Collapsed:			return TEXT("Collapsed");
		case ESlateVisibility::Hidden:				return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:	return TEXT("HitTestInvisible");
		case ESlateVisibility::SelfHitTestInvisible:return TEXT("SelfHitTestInvisible");
		default:									return TEXT("Unknown");
		}
	}

	FString PropertyTypeToString(const FProperty* Prop)
	{
		if (!Prop) return TEXT("Unknown");
		return Prop->GetCPPType();
	}

	void GatherWidgets(UWidget* Widget, const FString& ParentName, TArray<FBridgeWidgetInfo>& Out)
	{
		if (!Widget) return;

		FBridgeWidgetInfo Info;
		Info.Name = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.ParentName = ParentName;
		Info.bIsVariable = Widget->bIsVariable;
		Info.Visibility = VisibilityToString(Widget->GetVisibility());

		if (UPanelSlot* Slot = Widget->Slot)
		{
			Info.SlotType = Slot->GetClass()->GetName();
		}

		Out.Add(Info);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				GatherWidgets(Panel->GetChildAt(i), Info.Name, Out);
			}
		}
	}

	UWidget* FindWidgetByName(UWidgetBlueprint* WBP, const FString& WidgetName)
	{
		if (!WBP || !WBP->WidgetTree) return nullptr;

		UWidget* Found = nullptr;
		WBP->WidgetTree->ForEachWidget([&](UWidget* W)
		{
			if (W && W->GetName() == WidgetName)
			{
				Found = W;
			}
		});
		return Found;
	}
}

// ─── GetWidgetTree ──────────────────────────────────────────

TArray<FBridgeWidgetInfo> UUnrealBridgeUMGLibrary::GetWidgetTree(const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetInfo> Result;

	UWidgetBlueprint* WBP = LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	if (WBP->WidgetTree && WBP->WidgetTree->RootWidget)
	{
		GatherWidgets(WBP->WidgetTree->RootWidget, TEXT(""), Result);
	}

	return Result;
}

// ─── GetWidgetProperties ────────────────────────────────────

TArray<FBridgeWidgetPropertyValue> UUnrealBridgeUMGLibrary::GetWidgetProperties(
	const FString& WidgetBlueprintPath, const FString& WidgetName)
{
	TArray<FBridgeWidgetPropertyValue> Result;

	UWidgetBlueprint* WBP = LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	UWidget* Widget = FindWidgetByName(WBP, WidgetName);
	if (!Widget) return Result;

	UObject* CDO = Widget->GetClass()->GetDefaultObject();
	if (!CDO) return Result;

	for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		// Skip internal/transient properties
		if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated))
			continue;

		void* WidgetValue = Prop->ContainerPtrToValuePtr<void>(Widget);
		void* CDOValue = Prop->ContainerPtrToValuePtr<void>(CDO);

		if (!Prop->Identical(WidgetValue, CDOValue))
		{
			FBridgeWidgetPropertyValue PV;
			PV.Name = Prop->GetName();
			PV.Type = PropertyTypeToString(Prop);

			FString ExportedValue;
			Prop->ExportTextItem_Direct(ExportedValue, WidgetValue, CDOValue, Widget, PPF_None);
			PV.Value = ExportedValue;

			Result.Add(PV);
		}
	}

	return Result;
}

// ─── GetWidgetAnimations ────────────────────────────────────

TArray<FBridgeWidgetAnimationInfo> UUnrealBridgeUMGLibrary::GetWidgetAnimations(
	const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetAnimationInfo> Result;

	UWidgetBlueprint* WBP = LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	for (UWidgetAnimation* Anim : WBP->Animations)
	{
		if (!Anim) continue;

		FBridgeWidgetAnimationInfo Info;
#if WITH_EDITOR
		Info.Name = Anim->GetDisplayLabel();
		if (Info.Name.IsEmpty())
#endif
		{
			Info.Name = Anim->GetName();
		}

		Info.Duration = Anim->GetEndTime() - Anim->GetStartTime();

		UMovieScene* Scene = Anim->GetMovieScene();
		if (Scene)
		{
			// Build Guid -> widget name map from animation bindings
			TMap<FGuid, FString> GuidToWidget;
			for (const FWidgetAnimationBinding& Binding : Anim->AnimationBindings)
			{
				GuidToWidget.Add(Binding.AnimationGuid, Binding.WidgetName.ToString());
			}

			const TArray<FMovieSceneBinding>& Bindings = const_cast<const UMovieScene*>(Scene)->GetBindings();
			for (const FMovieSceneBinding& Binding : Bindings)
			{
				FString TargetWidget;
				// Find possessable to get bound widget name
				FMovieScenePossessable* Possessable = Scene->FindPossessable(Binding.GetObjectGuid());
				if (Possessable)
				{
					TargetWidget = Possessable->GetName();
				}
				// Fallback: look up via animation binding guid
				if (TargetWidget.IsEmpty())
				{
					if (FString* Found = GuidToWidget.Find(Binding.GetObjectGuid()))
					{
						TargetWidget = *Found;
					}
				}

				for (UMovieSceneTrack* Track : Binding.GetTracks())
				{
					if (!Track) continue;

					FBridgeWidgetAnimTrack TrackInfo;
					TrackInfo.WidgetName = TargetWidget;
					TrackInfo.TrackType = Track->GetClass()->GetName();
					TrackInfo.DisplayName = Track->GetDisplayName().ToString();
					Info.Tracks.Add(TrackInfo);
				}
			}

			// Master/global tracks (not bound to a widget)
			for (UMovieSceneTrack* Track : Scene->GetTracks())
			{
				if (!Track) continue;

				FBridgeWidgetAnimTrack TrackInfo;
				TrackInfo.TrackType = Track->GetClass()->GetName();
				TrackInfo.DisplayName = Track->GetDisplayName().ToString();
				Info.Tracks.Add(TrackInfo);
			}
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── GetWidgetBindings ──────────────────────────────────────

TArray<FBridgeWidgetBindingInfo> UUnrealBridgeUMGLibrary::GetWidgetBindings(
	const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetBindingInfo> Result;

	UWidgetBlueprint* WBP = LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	for (const FDelegateEditorBinding& Binding : WBP->Bindings)
	{
		FBridgeWidgetBindingInfo Info;
		Info.WidgetName = Binding.ObjectName;
		Info.PropertyName = Binding.PropertyName.ToString();
		Info.FunctionName = Binding.FunctionName.ToString();

		if (Info.FunctionName.IsEmpty() && !Binding.SourceProperty.IsNone())
		{
			Info.FunctionName = Binding.SourceProperty.ToString();
		}

		Info.Kind = (Binding.Kind == EBindingKind::Function) ? TEXT("Function") : TEXT("Property");

		Result.Add(Info);
	}

	return Result;
}

// ─── GetWidgetEvents ────────────────────────────────────────

TArray<FBridgeWidgetEventInfo> UUnrealBridgeUMGLibrary::GetWidgetEvents(
	const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetEventInfo> Result;

	UWidgetBlueprint* WBP = LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	// Scan event graph for component bound event nodes (OnClicked, etc.)
	for (UEdGraph* Graph : WBP->UbergraphPages)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_ComponentBoundEvent* EventNode = Cast<UK2Node_ComponentBoundEvent>(Node))
			{
				FBridgeWidgetEventInfo Info;
				Info.WidgetName = EventNode->ComponentPropertyName.ToString();
				Info.EventName = EventNode->DelegatePropertyName.ToString();
				Info.HandlerName = EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
				Result.Add(Info);
			}
		}
	}

	return Result;
}

// ─── SearchWidgets ──────────────────────────────────────────

TArray<FBridgeWidgetInfo> UUnrealBridgeUMGLibrary::SearchWidgets(
	const FString& WidgetBlueprintPath, const FString& Query)
{
	TArray<FBridgeWidgetInfo> All = GetWidgetTree(WidgetBlueprintPath);
	TArray<FBridgeWidgetInfo> Result;

	FString Q = Query.ToLower();
	for (const FBridgeWidgetInfo& W : All)
	{
		if (W.Name.ToLower().Contains(Q) || W.WidgetClass.ToLower().Contains(Q))
		{
			Result.Add(W);
		}
	}

	return Result;
}

// ─── SetWidgetProperty ──────────────────────────────────────

bool UUnrealBridgeUMGLibrary::SetWidgetProperty(
	const FString& WidgetBlueprintPath, const FString& WidgetName,
	const FString& PropertyName, const FString& Value)
{
	UWidgetBlueprint* WBP = LoadWBP(WidgetBlueprintPath);
	if (!WBP) return false;

	UWidget* Widget = FindWidgetByName(WBP, WidgetName);
	if (!Widget) return false;

	FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop) return false;

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);
	if (!Prop->ImportText_Direct(*Value, ValuePtr, Widget, PPF_None))
		return false;

	WBP->MarkPackageDirty();
	return true;
}
