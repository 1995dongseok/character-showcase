#include "CharacterViewer/CharacterViewerGameMode.h"

#include "Character/CharacterProfileData.h"
#include "Character/CharacterProfileValidator.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "EngineUtils.h"
#include "UI/CharacterViewerWidget.h"

ACharacterViewerGameMode::ACharacterViewerGameMode()
{
	DefaultPawnClass = ACharacterViewerCameraPawn::StaticClass();
	PlayerControllerClass = ACharacterViewerController::StaticClass();
}

void ACharacterViewerGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APortfolioCharacterActor* ViewerActor = nullptr;
	int32 Count = 0;
	for (TActorIterator<APortfolioCharacterActor> It(World); It; ++It)
	{
		ViewerActor = *It;
		++Count;
	}

	if (Count != 1)
	{
		// 0 or 2+ viewer actors: do not touch any actor's Profile, and hand
		// the controller nullptr so it safely disables input (see
		// ACharacterViewerController::SetViewerActor()).
		ViewerActor = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("ACharacterViewerGameMode: expected exactly one APortfolioCharacterActor in the level, found %d. Viewer input disabled."), Count);
	}
	else if (DefaultProfile)
	{
		// Only overwrite the actor's Profile when a DefaultProfile was
		// explicitly assigned; a null DefaultProfile leaves whatever Profile
		// the level author already set on the placed actor instance alone
		// (see the PostLogin-timing note above ApplyProfile(nullptr) here
		// would otherwise clear it).
		ViewerActor->ApplyProfile(DefaultProfile);
	}

	// Profile problems (wrong bone/slot/morph names, bad camera ranges, ...)
	// are written to the log; see Docs/CHARACTER_VIEWER_SETUP.md section 2.10.
	if (ViewerActor)
	{
		UCharacterProfileValidator::LogProfileReport(ViewerActor->Profile, TEXT("PostLogin"));
	}

	if (ACharacterViewerController* ViewerController = Cast<ACharacterViewerController>(NewPlayer))
	{
		if (!ViewerController->WidgetClass && ViewerWidgetClass)
		{
			ViewerController->WidgetClass = ViewerWidgetClass;
		}
		ViewerController->SetViewerActor(ViewerActor);
	}
}

TArray<UCharacterProfileData*> ACharacterViewerGameMode::GetProfileLibrary() const
{
	TArray<UCharacterProfileData*> Result;
	Result.Reserve(ProfileLibrary.Num());
	for (const TObjectPtr<UCharacterProfileData>& LibraryProfile : ProfileLibrary)
	{
		if (LibraryProfile)
		{
			Result.Add(LibraryProfile);
		}
	}
	return Result;
}
