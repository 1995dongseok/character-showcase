#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterProfileData.generated.h"

class USkeletalMesh;

UCLASS(BlueprintType)
class CHARACTERSHOWCASE_API UCharacterProfileData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;
};
