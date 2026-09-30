#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DemoGameMode.generated.h"

class UCharacterProfileData;

// D1: DefaultPawn = ADemoCharacter, Controller = ADemoPlayerController, and the
// DemoProfile is applied to the spawned character. Spawns at a PlayerStart if
// ChoosePlayerStart() finds one in the level, else at origin +Z 100
// (FindPlayerStart() alone never returns null, so it is not used for this check).
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API ADemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADemoGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Play")
	TObjectPtr<UCharacterProfileData> DemoProfile;

protected:
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
