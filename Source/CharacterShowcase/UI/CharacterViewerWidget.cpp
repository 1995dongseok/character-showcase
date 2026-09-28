#include "UI/CharacterViewerWidget.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerController.h"

void UCharacterViewerWidget::BindToViewer(ACharacterViewerController* InController, APortfolioCharacterActor* InActor, ACharacterViewerCameraPawn* InCameraPawn)
{
	WeakController = InController;
	WeakActor = InActor;
	WeakCameraPawn = InCameraPawn;
	CurrentCameraPresetId = NAME_None;

	OnViewerDataChanged();
}

FText UCharacterViewerWidget::GetDisplayName() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return (Actor && Actor->Profile) ? Actor->Profile->DisplayName : FText::GetEmpty();
}

FText UCharacterViewerWidget::GetDescription() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return (Actor && Actor->Profile) ? Actor->Profile->Description : FText::GetEmpty();
}

TArray<FViewerListItem> UCharacterViewerWidget::GetCameraPresets() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerCameraPreset& Preset : Actor->Profile->CameraPresets)
	{
		FViewerListItem Item;
		Item.Id = Preset.Id;
		Item.DisplayName = Preset.DisplayName;
		Item.bEnabled = Preset.Id != NAME_None;
		Items.Add(Item);
	}

	return Items;
}

TArray<FViewerListItem> UCharacterViewerWidget::GetAnimations() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerAnimationEntry& Entry : Actor->Profile->Animations)
	{
		FViewerListItem Item;
		Item.Id = Entry.Id;
		Item.DisplayName = Entry.DisplayName;
		Item.bEnabled = Entry.Id != NAME_None && Entry.Sequence != nullptr;
		Items.Add(Item);
	}

	return Items;
}

TArray<FViewerListItem> UCharacterViewerWidget::GetExpressions() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerExpression& Expression : Actor->Profile->Expressions)
	{
		FViewerListItem Item;
		Item.Id = Expression.Id;
		Item.DisplayName = Expression.DisplayName;
		// An empty Morphs array is a valid Neutral expression, so only an empty Id is invalid here.
		Item.bEnabled = Expression.Id != NAME_None;
		Items.Add(Item);
	}

	return Items;
}

TArray<FViewerListItem> UCharacterViewerWidget::GetMaterialVariants() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerMaterialVariant& Variant : Actor->Profile->MaterialVariants)
	{
		FViewerListItem Item;
		Item.Id = Variant.Id;
		Item.DisplayName = Variant.DisplayName;

		bool bHasMaterial = false;
		for (const FViewerMaterialSlotOverride& SlotOverride : Variant.Slots)
		{
			if (SlotOverride.Material)
			{
				bHasMaterial = true;
				break;
			}
		}

		Item.bEnabled = Variant.Id != NAME_None && bHasMaterial;
		Items.Add(Item);
	}

	return Items;
}

bool UCharacterViewerWidget::IsTurntableEnabled() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor && Actor->IsTurntableEnabled();
}

FName UCharacterViewerWidget::GetCurrentAnimationId() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor ? Actor->GetCurrentAnimationId() : NAME_None;
}

FName UCharacterViewerWidget::GetCurrentExpressionId() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor ? Actor->GetCurrentExpressionId() : NAME_None;
}

FName UCharacterViewerWidget::GetCurrentMaterialVariantId() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor ? Actor->GetCurrentVariantId() : NAME_None;
}

bool UCharacterViewerWidget::IsPointerOverPanel() const
{
	return IsHovered();
}

void UCharacterViewerWidget::RequestCameraPreset(FName Id)
{
	CurrentCameraPresetId = Id;
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectCameraPreset(Id);
	}
}

void UCharacterViewerWidget::RequestAnimation(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectAnimation(Id);
	}
}

void UCharacterViewerWidget::RequestExpression(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectExpression(Id);
	}
}

void UCharacterViewerWidget::RequestMaterialVariant(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectMaterialVariant(Id);
	}
}

void UCharacterViewerWidget::RequestToggleTurntable()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ToggleTurntable();
	}
}

void UCharacterViewerWidget::RequestToggleCleanView()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ToggleCleanView();
	}
}

void UCharacterViewerWidget::RequestResetCamera()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ResetCamera();
	}
}

void UCharacterViewerWidget::NativeDestruct()
{
	WeakController.Reset();
	WeakActor.Reset();
	WeakCameraPawn.Reset();

	Super::NativeDestruct();
}
