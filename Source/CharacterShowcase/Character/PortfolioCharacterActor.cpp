#include "Character/PortfolioCharacterActor.h"

#include "Character/CharacterProfileData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

APortfolioCharacterActor::APortfolioCharacterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APortfolioCharacterActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyProfile(Profile.Get());
}

void APortfolioCharacterActor::ApplyProfile(UCharacterProfileData* NewProfile)
{
	Profile = IsValid(NewProfile) ? NewProfile : nullptr;
	Mesh->SetSkeletalMesh(Profile ? Profile->SkeletalMesh.Get() : nullptr, true);
}
