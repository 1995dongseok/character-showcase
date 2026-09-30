#include "PlayDemo/DemoGameMode.h"

#include "Character/CharacterProfileData.h"
#include "GameFramework/PlayerStart.h"
#include "PlayDemo/DemoCharacter.h"
#include "PlayDemo/DemoPlayerController.h"

ADemoGameMode::ADemoGameMode()
{
	DefaultPawnClass = ADemoCharacter::StaticClass();
	PlayerControllerClass = ADemoPlayerController::StaticClass();
}

void ADemoGameMode::RestartPlayer(AController* NewPlayer)
{
	// FindPlayerStart() never returns null (it falls back to the World Settings
	// actor at the origin), so ask ChoosePlayerStart() whether the level really has a PlayerStart.
	if (NewPlayer && !NewPlayer->GetPawn())
	{
		const AActor* Start = ChoosePlayerStart(NewPlayer);
		if (!Start || !Start->IsA<APlayerStart>())
		{
			// No PlayerStart in the level: spawn at origin, +Z 100.
			RestartPlayerAtTransform(NewPlayer, FTransform(FVector(0.f, 0.f, 100.f)));
			return;
		}
	}

	Super::RestartPlayer(NewPlayer);
}

void ADemoGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	if (ADemoCharacter* DemoCharacter = Cast<ADemoCharacter>(NewPlayer->GetPawn()))
	{
		if (DemoProfile)
		{
			DemoCharacter->ApplyProfile(DemoProfile);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ADemoGameMode: DemoProfile is not set; the play character has no mesh."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ADemoGameMode: the player has no ADemoCharacter pawn after starting."));
	}
}
