#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CharacterViewerGameMode.generated.h"

class UCharacterProfileData;
class UCharacterViewerWidget;

// P0-1: start-up wiring only (Default Profile, DefaultPawnClass/
// PlayerControllerClass). See Docs/CHARACTER_VIEWER_SETUP.md section 4.
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API ACharacterViewerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACharacterViewerGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viewer")
	TObjectPtr<UCharacterProfileData> DefaultProfile;

	// Passed to the controller only if the controller's own WidgetClass is unset.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Viewer")
	TSubclassOf<UCharacterViewerWidget> ViewerWidgetClass;

protected:
	// NOTE: PostLogin actually runs BEFORE the level's World::BeginPlay pass
	// in both LoadMap and PIE (SpawnPlayActor happens first), i.e. BEFORE the
	// PlayerController's own BeginPlay/OnPossess, not after. Because of that
	// ordering this function only calls ApplyProfile() when DefaultProfile is
	// explicitly set (non-null) instead of unconditionally, so it never wipes
	// a Profile a level author placed directly on the actor instance. See
	// Docs/CHARACTER_VIEWER_SETUP.md section 13 remaining risks.
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
