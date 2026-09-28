#include "Character/CharacterProfileData.h"

const FViewerCameraPreset* UCharacterProfileData::FindPreset(FName Id) const
{
	if (Id == NAME_None)
	{
		return nullptr;
	}

	for (const FViewerCameraPreset& Preset : CameraPresets)
	{
		if (Preset.Id == Id)
		{
			return &Preset;
		}
	}

	return nullptr;
}

const FViewerAnimationEntry* UCharacterProfileData::FindAnimation(FName Id) const
{
	if (Id == NAME_None)
	{
		return nullptr;
	}

	for (const FViewerAnimationEntry& Entry : Animations)
	{
		if (Entry.Id == Id)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FViewerExpression* UCharacterProfileData::FindExpression(FName Id) const
{
	if (Id == NAME_None)
	{
		return nullptr;
	}

	for (const FViewerExpression& Expression : Expressions)
	{
		if (Expression.Id == Id)
		{
			return &Expression;
		}
	}

	return nullptr;
}

const FViewerMaterialVariant* UCharacterProfileData::FindMaterialVariant(FName Id) const
{
	if (Id == NAME_None)
	{
		return nullptr;
	}

	for (const FViewerMaterialVariant& Variant : MaterialVariants)
	{
		if (Variant.Id == Id)
		{
			return &Variant;
		}
	}

	return nullptr;
}

FViewerCameraFraming UCharacterProfileData::GetResetFraming() const
{
	if (const FViewerCameraPreset* Preset = FindPreset(DefaultPresetId))
	{
		return Preset->Framing;
	}

	return DefaultFraming;
}
