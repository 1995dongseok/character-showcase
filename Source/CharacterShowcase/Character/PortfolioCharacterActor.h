#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortfolioCharacterActor.generated.h"

class UCharacterProfileData;
class USkeletalMeshComponent;

UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API APortfolioCharacterActor : public AActor
{
	GENERATED_BODY()

public:
	APortfolioCharacterActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UCharacterProfileData> Profile = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Character")
	void ApplyProfile(UCharacterProfileData* NewProfile);

protected:
	virtual void BeginPlay() override;
};
