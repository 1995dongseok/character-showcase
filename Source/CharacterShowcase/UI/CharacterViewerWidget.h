#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterViewerWidget.generated.h"

class ACharacterViewerController;
class APortfolioCharacterActor;
class ACharacterViewerCameraPawn;

// One row for a VIEW/EXPRESSION/ANIMATION/APPEARANCE selection list in the WBP.
USTRUCT(BlueprintType)
struct FViewerListItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Viewer")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Viewer")
	FText DisplayName;

	// False for data that would be a no-op or unsafe to apply (null Sequence,
	// empty Id, a material variant with no valid slot). The WBP should
	// disable/hide such rows instead of calling the matching Request* function.
	UPROPERTY(BlueprintReadOnly, Category = "Viewer")
	bool bEnabled = true;
};

// C++ base for WBP_CharacterViewer (P0-5 / P1). Holds no character/camera
// state of its own; every getter reads live state from the bound Actor/Pawn/
// Controller so the widget and the gameplay objects can never disagree.
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API UCharacterViewerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Called by ACharacterViewerController after possessing/finding the
	// viewer actor and pawn, and again whenever the profile is switched.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void BindToViewer(ACharacterViewerController* InController, APortfolioCharacterActor* InActor, ACharacterViewerCameraPawn* InCameraPawn);

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FText GetDescription() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetCameraPresets() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetAnimations() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetExpressions() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetMaterialVariants() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	bool IsTurntableEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentAnimationId() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentExpressionId() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentMaterialVariantId() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentCameraPresetId() const { return CurrentCameraPresetId; }

	// True while the pointer is over this panel; the Controller uses this to
	// skip Orbit/Zoom when a press/drag/wheel started over UMG instead of the viewport.
	UFUNCTION(BlueprintPure, Category = "Viewer")
	bool IsPointerOverPanel() const;

	// Fired after BindToViewer() (initial bind and every profile switch) so the WBP graph can rebuild its lists.
	UFUNCTION(BlueprintImplementableEvent, Category = "Viewer")
	void OnViewerDataChanged();

	// Thin forwarding calls for WBP button/list click handlers; all real logic lives in the Controller/Actor/Pawn.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestCameraPreset(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestAnimation(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestExpression(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestMaterialVariant(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestToggleTurntable();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestToggleCleanView();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestResetCamera();

protected:
	virtual void NativeDestruct() override;

private:
	TWeakObjectPtr<ACharacterViewerController> WeakController;
	TWeakObjectPtr<APortfolioCharacterActor> WeakActor;
	TWeakObjectPtr<ACharacterViewerCameraPawn> WeakCameraPawn;

	// Tracked here (not on the Actor/Pawn) purely for UI highlight purposes; not gameplay state.
	FName CurrentCameraPresetId = NAME_None;
};
