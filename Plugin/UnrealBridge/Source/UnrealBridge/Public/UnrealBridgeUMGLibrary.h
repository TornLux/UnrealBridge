#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealBridgeUMGLibrary.generated.h"

/** Describes a single widget in a Widget Blueprint hierarchy. */
USTRUCT(BlueprintType)
struct FBridgeWidgetInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString Name;

	/** Widget class, e.g. "CanvasPanel", "TextBlock", "Button" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString WidgetClass;

	/** Parent widget name (empty for root) */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString ParentName;

	/** Slot class if parented, e.g. "CanvasPanelSlot", "OverlaySlot" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString SlotType;

	/** Whether this widget is exposed as a variable in the Blueprint */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	bool bIsVariable = false;

	/** Visibility setting, e.g. "Visible", "Collapsed", "Hidden" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString Visibility;
};

/** Non-default property value on a widget. */
USTRUCT(BlueprintType)
struct FBridgeWidgetPropertyValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString Type;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString Value;
};

/** A track within a widget animation. */
USTRUCT(BlueprintType)
struct FBridgeWidgetAnimTrack
{
	GENERATED_BODY()

	/** The widget targeted by this track */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString WidgetName;

	/** Track type, e.g. "Color", "Transform", "Visibility" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString TrackType;

	/** Display name of the track */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString DisplayName;
};

/** Describes a widget animation. */
USTRUCT(BlueprintType)
struct FBridgeWidgetAnimationInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	float Duration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	TArray<FBridgeWidgetAnimTrack> Tracks;
};

/** A property binding on a widget. */
USTRUCT(BlueprintType)
struct FBridgeWidgetBindingInfo
{
	GENERATED_BODY()

	/** The widget this binding is on */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString WidgetName;

	/** The property being bound, e.g. "Text", "Visibility", "ColorAndOpacity" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString PropertyName;

	/** The function providing the value */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString FunctionName;

	/** "Function" or "Property" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString Kind;
};

/** An event binding on a widget (OnClicked, OnHovered, etc.). */
USTRUCT(BlueprintType)
struct FBridgeWidgetEventInfo
{
	GENERATED_BODY()

	/** The widget this event is on */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString WidgetName;

	/** Event name, e.g. "OnClicked", "OnHovered" */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString EventName;

	/** Bound function or node description */
	UPROPERTY(BlueprintReadOnly, Category = "UnrealBridge|UMG")
	FString HandlerName;
};

/**
 * UMG / Widget Blueprint introspection via UnrealBridge.
 */
UCLASS()
class UNREALBRIDGE_API UUnrealBridgeUMGLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Get the widget hierarchy of a Widget Blueprint.
	 * Returns a flat list with parent references to reconstruct the tree.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|UMG")
	static TArray<FBridgeWidgetInfo> GetWidgetTree(const FString& WidgetBlueprintPath);

	/**
	 * Get non-default property values for a specific widget.
	 * Only returns properties that differ from the widget class defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|UMG")
	static TArray<FBridgeWidgetPropertyValue> GetWidgetProperties(
		const FString& WidgetBlueprintPath, const FString& WidgetName);

	/**
	 * Get all widget animations with their tracks and durations.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|UMG")
	static TArray<FBridgeWidgetAnimationInfo> GetWidgetAnimations(const FString& WidgetBlueprintPath);

	/**
	 * Get all property bindings (e.g. Text bound to a function).
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|UMG")
	static TArray<FBridgeWidgetBindingInfo> GetWidgetBindings(const FString& WidgetBlueprintPath);

	/**
	 * Get widget event bindings (OnClicked, OnHovered, etc.) from the event graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|UMG")
	static TArray<FBridgeWidgetEventInfo> GetWidgetEvents(const FString& WidgetBlueprintPath);

	/**
	 * Search widgets by name or class substring.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|UMG")
	static TArray<FBridgeWidgetInfo> SearchWidgets(
		const FString& WidgetBlueprintPath, const FString& Query);

	/**
	 * Set a property on a widget by name. Value is parsed as text.
	 */
	UFUNCTION(BlueprintCallable, Category = "UnrealBridge|UMG")
	static bool SetWidgetProperty(
		const FString& WidgetBlueprintPath, const FString& WidgetName,
		const FString& PropertyName, const FString& Value);
};
