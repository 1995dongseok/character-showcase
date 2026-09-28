#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterProfileNullSafetyTest,
	"CharacterShowcase.Profile.NullSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterProfileNullSafetyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}

	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	if (!TestNotNull(TEXT("Character actor exists"), Actor))
	{
		World->DestroyWorld(false);
		return false;
	}

	Actor->ApplyProfile(nullptr);
	TestNull(TEXT("Missing profile leaves no mesh"), Actor->Mesh->GetSkeletalMeshAsset());

	UCharacterProfileData* EmptyProfile = NewObject<UCharacterProfileData>(World);
	Actor->ApplyProfile(EmptyProfile);
	TestTrue(TEXT("Profile is assigned"), Actor->Profile.Get() == EmptyProfile);
	TestNull(TEXT("Missing mesh is accepted"), Actor->Mesh->GetSkeletalMeshAsset());

	Actor->ApplyProfile(nullptr);
	TestNull(TEXT("Profile is cleared"), Actor->Profile.Get());
	TestNull(TEXT("Cleared profile leaves no mesh"), Actor->Mesh->GetSkeletalMeshAsset());

	World->DestroyWorld(false);
	return true;
}

#endif
