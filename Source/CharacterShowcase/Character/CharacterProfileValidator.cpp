#include "Character/CharacterProfileValidator.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Animation/Skeleton.h"
#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ReferenceSkeleton.h"

const FName UCharacterProfileValidator::CategoryMesh(TEXT("Mesh"));
const FName UCharacterProfileValidator::CategoryCamera(TEXT("Camera"));
const FName UCharacterProfileValidator::CategoryAnimation(TEXT("Animation"));
const FName UCharacterProfileValidator::CategoryExpression(TEXT("Expression"));
const FName UCharacterProfileValidator::CategoryMaterial(TEXT("Material"));
const FName UCharacterProfileValidator::CategoryPart(TEXT("Part"));
const FName UCharacterProfileValidator::CategoryPlay(TEXT("Play"));

namespace CharacterProfileValidatorPrivate
{
	using FIssues = TArray<FViewerProfileIssue>;

	void Add(FIssues& Out, EViewerIssueSeverity Severity, FName Category, const FString& Field, const FString& Message)
	{
		FViewerProfileIssue& Issue = Out.AddDefaulted_GetRef();
		Issue.Severity = Severity;
		Issue.Category = Category;
		Issue.Field = Field;
		Issue.Message = Message;
	}

	// "a, b, c" (at most MaxListedNames, then "외 N개"), or "(없음)".
	FString JoinNames(const TArray<FString>& Names)
	{
		if (Names.Num() == 0)
		{
			return TEXT("(없음)");
		}
		const int32 Shown = FMath::Min(Names.Num(), UCharacterProfileValidator::MaxListedNames);
		FString Result;
		for (int32 Index = 0; Index < Shown; ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(", ");
			}
			Result += Names[Index];
		}
		if (Names.Num() > Shown)
		{
			Result += FString::Printf(TEXT(" 외 %d개"), Names.Num() - Shown);
		}
		return Result;
	}

	// '<display name>' for messages: "Id" plus the display name when it adds something.
	FString Label(FName Id)
	{
		return Id.IsNone() ? FString(TEXT("Id 없음")) : FString::Printf(TEXT("'%s'"), *Id.ToString());
	}

	TArray<FString> GetSlotNames(const USkeletalMesh* Mesh)
	{
		TArray<FString> Names;
		if (Mesh)
		{
			const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
			for (int32 Index = 0; Index < Materials.Num(); ++Index)
			{
				Names.Add(FString::Printf(TEXT("%s(%d)"), *Materials[Index].MaterialSlotName.ToString(), Index));
			}
		}
		return Names;
	}

	int32 FindSlotIndex(const USkeletalMesh* Mesh, FName SlotName)
	{
		if (Mesh && !SlotName.IsNone())
		{
			const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
			for (int32 Index = 0; Index < Materials.Num(); ++Index)
			{
				if (Materials[Index].MaterialSlotName == SlotName)
				{
					return Index;
				}
			}
		}
		return INDEX_NONE;
	}

	// Side/separator-normalized bone name (FName comparison, and so the
	// engine's own bone lookup, is already case-insensitive): lower case,
	// "left"/"right" -> "l"/"r", a leading "l_"/"r_" moved to the end, then
	// every separator removed ("L_Hand", "hand_left", "Hand.L" -> "handl").
	FString NormalizeBoneName(const FString& Name, bool* bOutHasSide = nullptr)
	{
		FString Result = Name.ToLower();
		Result.ReplaceInline(TEXT("-"), TEXT("_"));
		Result.ReplaceInline(TEXT(" "), TEXT("_"));
		Result.ReplaceInline(TEXT("."), TEXT("_"));
		Result.ReplaceInline(TEXT("left"), TEXT("l"));
		Result.ReplaceInline(TEXT("right"), TEXT("r"));
		if (Result.StartsWith(TEXT("l_")) || Result.StartsWith(TEXT("r_")))
		{
			Result = Result.Mid(2) + TEXT("_") + Result.Left(1);
		}
		if (bOutHasSide)
		{
			*bOutHasSide = Result.EndsWith(TEXT("_l")) || Result.EndsWith(TEXT("_r"));
		}
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		return Result;
	}

	// The nearest existing bone name(s) for a missing Bone, or empty: a
	// separator/side-normalized match (spine01 -> spine_01, hand_left /
	// L_hand -> hand_l), then a missing side suffix (hand -> hand_l, hand_r).
	FString SuggestBoneName(const FReferenceSkeleton& RefSkeleton, FName Bone)
	{
		bool bWantedHasSide = false;
		const FString WantedNormalized = NormalizeBoneName(Bone.ToString(), &bWantedHasSide);
		const int32 NumBones = RefSkeleton.GetNum();
		for (int32 Index = 0; Index < NumBones; ++Index)
		{
			const FString Candidate = RefSkeleton.GetBoneName(Index).ToString();
			if (NormalizeBoneName(Candidate) == WantedNormalized)
			{
				return FString::Printf(TEXT("'%s'"), *Candidate);
			}
		}
		if (!bWantedHasSide)
		{
			TArray<FString> Sided;
			for (int32 Index = 0; Index < NumBones; ++Index)
			{
				const FString Candidate = RefSkeleton.GetBoneName(Index).ToString();
				bool bCandidateHasSide = false;
				const FString CandidateNormalized = NormalizeBoneName(Candidate, &bCandidateHasSide);
				if (bCandidateHasSide && (CandidateNormalized == WantedNormalized + TEXT("l") || CandidateNormalized == WantedNormalized + TEXT("r")))
				{
					Sided.Add(FString::Printf(TEXT("'%s'"), *Candidate));
				}
			}
			if (Sided.Num() > 0)
			{
				return FString::Join(Sided, TEXT(" 또는 "));
			}
		}
		return FString();
	}

	// Same texture collection as APortfolioCharacterActor's measured stats
	// (PortfolioCharacterActor.cpp, CollectTextures): compiled material
	// resources first; without them (-NullRHI editor, commandlet) the material
	// graph's referenced textures with each Material Instance's texture
	// parameter overrides applied root-most first.
	void CollectTextures(const UMaterialInterface* Material, TSet<UTexture*>& OutTextures)
	{
		if (!Material)
		{
			return;
		}
		TArray<UTexture*> Used;
		Material->GetUsedTextures(Used, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);
		Used.Remove(nullptr);
		if (Used.Num() > 0)
		{
			OutTextures.Append(Used);
			return;
		}

		TSet<UTexture*> Textures;
		for (UObject* Referenced : Material->GetReferencedTextures())
		{
			if (UTexture* Texture = Cast<UTexture>(Referenced))
			{
				Textures.Add(Texture);
			}
		}
		TArray<const UMaterialInstance*> InstanceChain;
		for (const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material); Instance && !InstanceChain.Contains(Instance); Instance = Cast<UMaterialInstance>(Instance->Parent))
		{
			InstanceChain.Add(Instance);
		}
		for (int32 Index = InstanceChain.Num() - 1; Index >= 0; --Index)
		{
			const UMaterialInstance* Instance = InstanceChain[Index];
			for (const FTextureParameterValue& Override : Instance->TextureParameterValues)
			{
				UTexture* ParentValue = nullptr;
				if (Instance->Parent && Instance->Parent->GetTextureParameterValue(Override.ParameterInfo, ParentValue) && ParentValue && ParentValue != Override.ParameterValue)
				{
					Textures.Remove(ParentValue);
				}
				if (Override.ParameterValue)
				{
					Textures.Add(Override.ParameterValue);
				}
			}
		}
		OutTextures.Append(Textures);
	}

	// Artist-facing texture size: platform data, or the imported source size
	// in the editor when platform data is not built yet (reports 0x0).
	FIntPoint GetTextureSize(const UTexture* Texture)
	{
		int32 Width = FMath::RoundToInt(Texture->GetSurfaceWidth());
		int32 Height = FMath::RoundToInt(Texture->GetSurfaceHeight());
#if WITH_EDITORONLY_DATA
		if ((Width <= 0 || Height <= 0) && Texture->Source.IsValid())
		{
			Width = Texture->Source.GetSizeX();
			Height = Texture->Source.GetSizeY();
		}
#endif
		return FIntPoint(Width, Height);
	}

	// ---------------------------------------------------------------- Mesh

	void CheckMesh(const UCharacterProfileData& Profile, FIssues& Out)
	{
		const FName Cat = UCharacterProfileValidator::CategoryMesh;
		const USkeletalMesh* Mesh = Profile.SkeletalMesh;

		if (Profile.DisplayName.IsEmptyOrWhitespace())
		{
			Add(Out, EViewerIssueSeverity::Warning, Cat, TEXT("DisplayName"),
				TEXT("Display Name이 비어 있습니다. 패널 제목과 CHARACTER 목록 버튼에 이름이 표시되지 않습니다. Details → Character → Display Name에 캐릭터 이름을 입력하세요."));
		}

		if (!Mesh)
		{
			Add(Out, EViewerIssueSeverity::Error, Cat, TEXT("SkeletalMesh"),
				TEXT("Skeletal Mesh가 비어 있습니다. 화면에 캐릭터가 보이지 않고 메시를 쓰는 검사(본·슬롯·Morph·텍스처)도 건너뜁니다. Details → Character → Skeletal Mesh에 Import한 캐릭터 메시를 지정하세요."));
			return;
		}

		const FString MeshName = Mesh->GetName();
		if (!Mesh->GetSkeleton())
		{
			Add(Out, EViewerIssueSeverity::Error, Cat, TEXT("SkeletalMesh.Skeleton"),
				FString::Printf(TEXT("Skeletal Mesh '%s'에 Skeleton이 없습니다. 애니메이션과 본 기반 파츠가 동작하지 않습니다. 메시를 다시 Import하면서 Skeleton을 만들거나 기존 Skeleton을 지정하세요."), *MeshName));
		}

		if (!Mesh->GetPhysicsAsset())
		{
			if (Profile.Parts.Num() > 0)
			{
				Add(Out, EViewerIssueSeverity::Warning, Cat, TEXT("SkeletalMesh.PhysicsAsset"),
					FString::Printf(TEXT("Skeletal Mesh '%s'에 Physics Asset이 없어 파츠 %d개를 클릭으로 선택할 수 없습니다(Inspection 클릭은 Physics Asset의 본 충돌체에 맞습니다). Content Browser에서 메시 우클릭 → Create → Physics Asset으로 만들고, 메시 에디터의 Physics Asset 칸에 지정하세요."), *MeshName, Profile.Parts.Num()));
			}
			else
			{
				Add(Out, EViewerIssueSeverity::Info, Cat, TEXT("SkeletalMesh.PhysicsAsset"),
					FString::Printf(TEXT("Skeletal Mesh '%s'에 Physics Asset이 없습니다. Parts가 비어 있어 지금은 문제없지만, 파츠를 추가하면 클릭 선택에 Physics Asset이 필요합니다."), *MeshName));
			}
		}

#if WITH_EDITOR
		// Editor-only hint: a character imported in the wrong unit (m vs cm)
		// shows up as a tiny or huge mesh and breaks every camera preset.
		const float HeightCm = Mesh->GetImportedBounds().BoxExtent.Z * 2.f;
		if (HeightCm < UCharacterProfileValidator::MinMeshHeightCm || HeightCm > UCharacterProfileValidator::MaxMeshHeightCm)
		{
			Add(Out, EViewerIssueSeverity::Warning, Cat, TEXT("SkeletalMesh"),
				FString::Printf(TEXT("스케일 확인: 높이 %.0f cm (메시 '%s', 권장 %.0f~%.0f cm). 사람 크기 캐릭터라면 Import 단위가 틀렸을 수 있습니다. FBX Import 옵션의 Import Uniform Scale(m 단위 원본이면 100)을 확인해 다시 Import하세요. 의도한 크기라면 무시해도 됩니다."),
					HeightCm, *MeshName, UCharacterProfileValidator::MinMeshHeightCm, UCharacterProfileValidator::MaxMeshHeightCm));
		}
#endif
	}

	// -------------------------------------------------------------- Camera

	void CheckFraming(const FViewerCameraFraming& Framing, const FString& Field, FIssues& Out)
	{
		const FName Cat = UCharacterProfileValidator::CategoryCamera;
		if (Framing.MinDistance >= Framing.MaxDistance)
		{
			Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".MinDistance"),
				FString::Printf(TEXT("%s: Min Distance(%.1f)이(가) Max Distance(%.1f)보다 크거나 같아 Zoom이 동작하지 않습니다. Min Distance < Max Distance가 되도록 고치세요."), *Field, Framing.MinDistance, Framing.MaxDistance));
		}
		if (Framing.MinPitch >= Framing.MaxPitch)
		{
			Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".MinPitch"),
				FString::Printf(TEXT("%s: Min Pitch(%.1f)이(가) Max Pitch(%.1f)보다 크거나 같아 위아래 회전이 막힙니다. Min Pitch < Max Pitch가 되도록 고치세요(보통 -80 ~ 10)."), *Field, Framing.MinPitch, Framing.MaxPitch));
		}
		if (Framing.FOV < UCharacterProfileValidator::MinFOV || Framing.FOV > UCharacterProfileValidator::MaxFOV)
		{
			Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".FOV"),
				FString::Printf(TEXT("%s: FOV %.1f이(가) 허용 범위(%.0f~%.0f) 밖이라 화면이 심하게 왜곡되거나 잘립니다. 캐릭터 촬영에는 보통 30~60을 씁니다."), *Field, Framing.FOV, UCharacterProfileValidator::MinFOV, UCharacterProfileValidator::MaxFOV));
		}
	}

	void CheckCamera(const UCharacterProfileData& Profile, FIssues& Out)
	{
		const FName Cat = UCharacterProfileValidator::CategoryCamera;
		CheckFraming(Profile.DefaultFraming, TEXT("DefaultFraming"), Out);

		TMap<FName, int32> SeenIds;
		TArray<FString> Ids;
		for (int32 Index = 0; Index < Profile.CameraPresets.Num(); ++Index)
		{
			const FViewerCameraPreset& Preset = Profile.CameraPresets[Index];
			const FString Field = FString::Printf(TEXT("CameraPresets[%d]"), Index);
			if (Preset.Id.IsNone())
			{
				Add(Out, EViewerIssueSeverity::Warning, Cat, Field + TEXT(".Id"),
					FString::Printf(TEXT("Camera Presets[%d]의 Id가 비어 있습니다. 버튼과 Default Preset Id는 Id로 프리셋을 찾습니다. 'Face'처럼 고유한 Id를 입력하세요."), Index));
			}
			else if (const int32* First = SeenIds.Find(Preset.Id))
			{
				Add(Out, EViewerIssueSeverity::Warning, Cat, Field + TEXT(".Id"),
					FString::Printf(TEXT("Camera Presets[%d]의 Id '%s'이(가) Camera Presets[%d]과(와) 중복됩니다. 같은 Id는 항상 첫 번째 프리셋이 선택됩니다. 다른 Id로 바꾸세요."), Index, *Preset.Id.ToString(), *First));
			}
			else
			{
				SeenIds.Add(Preset.Id, Index);
				Ids.Add(Preset.Id.ToString());
			}
			CheckFraming(Preset.Framing, Field + TEXT(".Framing"), Out);
		}

		if (!Profile.DefaultPresetId.IsNone() && !Profile.FindPreset(Profile.DefaultPresetId))
		{
			Add(Out, EViewerIssueSeverity::Warning, Cat, TEXT("DefaultPresetId"),
				FString::Printf(TEXT("Default Preset Id '%s'인 프리셋이 Camera Presets에 없어 Reset(R)이 Default Framing으로 돌아갑니다. 있는 Id 중 하나를 입력하세요: %s"), *Profile.DefaultPresetId.ToString(), *JoinNames(Ids)));
		}
	}

	// ----------------------------------------------------------- Animation

	void CheckAnimation(const UCharacterProfileData& Profile, FIssues& Out)
	{
		const FName Cat = UCharacterProfileValidator::CategoryAnimation;
		const USkeletalMesh* Mesh = Profile.SkeletalMesh;

		TMap<FName, int32> SeenIds;
		TArray<FString> Ids;
		for (int32 Index = 0; Index < Profile.Animations.Num(); ++Index)
		{
			const FViewerAnimationEntry& Entry = Profile.Animations[Index];
			const FString Field = FString::Printf(TEXT("Animations[%d]"), Index);
			if (Entry.Id.IsNone())
			{
				Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".Id"),
					FString::Printf(TEXT("Animations[%d]의 Id가 비어 있어 버튼으로 선택할 수 없습니다. 'Idle'처럼 고유한 Id를 입력하세요."), Index));
			}
			else if (const int32* First = SeenIds.Find(Entry.Id))
			{
				Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".Id"),
					FString::Printf(TEXT("Animations[%d]의 Id '%s'이(가) Animations[%d]과(와) 중복되어 이 항목은 재생되지 않습니다(항상 첫 번째 항목이 재생됨). 다른 Id로 바꾸세요."), Index, *Entry.Id.ToString(), *First));
			}
			else
			{
				SeenIds.Add(Entry.Id, Index);
				Ids.Add(Entry.Id.ToString());
			}

			if (!Entry.Sequence)
			{
				Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".Sequence"),
					FString::Printf(TEXT("Animations[%d] (%s)의 Sequence가 비어 있어 이 버튼은 아무 동작도 하지 않습니다. 이 메시 Skeleton의 Animation Sequence를 지정하거나 행을 지우세요."), Index, *Label(Entry.Id)));
				continue;
			}

			// Skeleton check only when the mesh itself is usable (a missing
			// mesh/Skeleton is already reported as a Mesh error).
			if (Mesh && Mesh->GetSkeleton() && !APortfolioCharacterActor::IsAnimationSkeletonCompatible(Entry.Sequence->GetSkeleton(), Mesh))
			{
				Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".Sequence"),
					FString::Printf(TEXT("Animations[%d] (%s)의 Sequence '%s'은(는) Skeleton '%s'용이라 메시 '%s'(Skeleton '%s')에서 재생이 거부됩니다. 같은 Skeleton의 애니메이션을 쓰거나 IK Retargeter로 이 메시에 리타겟한 Sequence를 지정하세요."),
						Index, *Label(Entry.Id), *Entry.Sequence->GetName(), *GetNameSafe(Entry.Sequence->GetSkeleton()), *Mesh->GetName(), *GetNameSafe(Mesh->GetSkeleton())));
			}

			if (Entry.bIsPose)
			{
				const float Length = Entry.Sequence->GetPlayLength();
				if (Entry.PoseTime < 0.f || Entry.PoseTime > Length + KINDA_SMALL_NUMBER)
				{
					Add(Out, EViewerIssueSeverity::Warning, Cat, Field + TEXT(".PoseTime"),
						FString::Printf(TEXT("Animations[%d] (%s)의 Pose Time %.2f초가 Sequence '%s'의 길이(0 ~ %.2f초) 밖입니다. 의도한 프레임이 아니라 처음/끝 프레임에 고정됩니다. 범위 안의 시간을 입력하세요."),
							Index, *Label(Entry.Id), Entry.PoseTime, *Entry.Sequence->GetName(), Length));
				}
			}
		}

		if (!Profile.DefaultAnimationId.IsNone() && !Profile.FindAnimation(Profile.DefaultAnimationId))
		{
			Add(Out, EViewerIssueSeverity::Error, Cat, TEXT("DefaultAnimationId"),
				FString::Printf(TEXT("Default Animation Id '%s'인 항목이 Animations에 없어 시작 시 기본 애니메이션이 재생되지 않습니다(기본 자세로 정지). 있는 Id 중 하나를 입력하거나 비우세요: %s"), *Profile.DefaultAnimationId.ToString(), *JoinNames(Ids)));
		}

		if (Profile.DefaultAnimClass && !Profile.DefaultAnimationId.IsNone())
		{
			Add(Out, EViewerIssueSeverity::Info, Cat, TEXT("DefaultAnimClass"),
				FString::Printf(TEXT("Default Anim Class '%s'과(와) Default Animation Id '%s'이(가) 둘 다 지정되어 있습니다. 시작 상태는 Default Anim Class(AnimBP)가 우선이고 Default Animation Id는 쓰이지 않습니다. 의도한 것이 아니면 하나를 비우세요."),
					*Profile.DefaultAnimClass->GetName(), *Profile.DefaultAnimationId.ToString()));
		}
	}

	// ---------------------------------------------------------- Expression

	void CheckExpression(const UCharacterProfileData& Profile, FIssues& Out)
	{
		const FName Cat = UCharacterProfileValidator::CategoryExpression;
		const USkeletalMesh* Mesh = Profile.SkeletalMesh;
		if (!Mesh)
		{
			return;
		}

		const TArray<TObjectPtr<UMorphTarget>>& MorphTargets = Mesh->GetMorphTargets();
		if (MorphTargets.Num() == 0)
		{
			TArray<FString> Used;
			for (const FViewerExpression& Expression : Profile.Expressions)
			{
				for (const FViewerMorphWeight& Morph : Expression.Morphs)
				{
					Used.AddUnique(Morph.MorphName.ToString());
				}
			}
			if (Used.Num() > 0)
			{
				Add(Out, EViewerIssueSeverity::Warning, Cat, TEXT("Expressions"),
					FString::Printf(TEXT("메시 '%s'에 Morph Target이 하나도 없는데 Expressions가 Morph %d개(%s)를 사용합니다. 표정 버튼을 눌러도 얼굴이 바뀌지 않습니다. Morph Target이 들어 있는 메시를 Import(FBX Import 옵션 Import Morph Targets 체크)하거나 Expressions의 Morphs를 비우세요."),
						*Mesh->GetName(), Used.Num(), *JoinNames(Used)));
			}
			return;
		}

		TArray<FString> Available;
		for (const UMorphTarget* MorphTarget : MorphTargets)
		{
			if (MorphTarget)
			{
				Available.Add(MorphTarget->GetName());
			}
		}

		for (int32 ExpressionIndex = 0; ExpressionIndex < Profile.Expressions.Num(); ++ExpressionIndex)
		{
			const FViewerExpression& Expression = Profile.Expressions[ExpressionIndex];
			for (int32 MorphIndex = 0; MorphIndex < Expression.Morphs.Num(); ++MorphIndex)
			{
				const FName MorphName = Expression.Morphs[MorphIndex].MorphName;
				if (!Mesh->FindMorphTarget(MorphName))
				{
					Add(Out, EViewerIssueSeverity::Error, Cat, FString::Printf(TEXT("Expressions[%d].Morphs[%d].MorphName"), ExpressionIndex, MorphIndex),
						FString::Printf(TEXT("Expressions[%d] (%s)의 Morphs[%d] Morph Name '%s'이(가) 메시 '%s'에 없어 무시됩니다. 메시에 있는 Morph 이름으로 고치세요: %s"),
							ExpressionIndex, *Label(Expression.Id), MorphIndex, *MorphName.ToString(), *Mesh->GetName(), *JoinNames(Available)));
				}
			}
		}
	}

	// ------------------------------------------------------------ Material

	void CheckMaterial(const UCharacterProfileData& Profile, FIssues& Out)
	{
		const FName Cat = UCharacterProfileValidator::CategoryMaterial;
		const USkeletalMesh* Mesh = Profile.SkeletalMesh;
		const TArray<FString> SlotNames = GetSlotNames(Mesh);
		const int32 NumSlots = Mesh ? Mesh->GetMaterials().Num() : 0;

		for (int32 VariantIndex = 0; VariantIndex < Profile.MaterialVariants.Num(); ++VariantIndex)
		{
			const FViewerMaterialVariant& Variant = Profile.MaterialVariants[VariantIndex];
			for (int32 SlotIndex = 0; SlotIndex < Variant.Slots.Num(); ++SlotIndex)
			{
				const FViewerMaterialSlotOverride& Slot = Variant.Slots[SlotIndex];
				const FString Field = FString::Printf(TEXT("MaterialVariants[%d].Slots[%d]"), VariantIndex, SlotIndex);
				const FString Where = FString::Printf(TEXT("Material Variants[%d] (%s)의 Slots[%d]"), VariantIndex, *Label(Variant.Id), SlotIndex);

				// Same resolution order as APortfolioCharacterActor::ApplyVariantOverrides():
				// Slot Name first, then Slot Index.
				if (Slot.SlotName.IsNone() && Slot.SlotIndex == INDEX_NONE)
				{
					Add(Out, EViewerIssueSeverity::Warning, Cat, Field,
						FString::Printf(TEXT("%s: Slot Name과 Slot Index가 모두 비어 있어 어느 슬롯에도 적용되지 않습니다. Slot Name에 메시 슬롯 이름을 입력하세요: %s"), *Where, *JoinNames(SlotNames)));
				}
				else if (Mesh)
				{
					const bool bIndexValid = Slot.SlotIndex >= 0 && Slot.SlotIndex < NumSlots;
					if (!Slot.SlotName.IsNone() && FindSlotIndex(Mesh, Slot.SlotName) == INDEX_NONE)
					{
						if (bIndexValid)
						{
							Add(Out, EViewerIssueSeverity::Warning, Cat, Field + TEXT(".SlotName"),
								FString::Printf(TEXT("%s: Slot Name '%s'이(가) 메시 '%s'에 없어 Slot Index %d(으)로 대신 적용됩니다. Slot Name을 메시 슬롯 이름으로 고치세요: %s"), *Where, *Slot.SlotName.ToString(), *Mesh->GetName(), Slot.SlotIndex, *JoinNames(SlotNames)));
						}
						else
						{
							Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".SlotName"),
								FString::Printf(TEXT("%s: Slot Name '%s'이(가) 메시 '%s'에 없어 이 재질은 적용되지 않습니다. Skeletal Mesh 에디터의 Material Slots에 보이는 이름으로 고치세요: %s"), *Where, *Slot.SlotName.ToString(), *Mesh->GetName(), *JoinNames(SlotNames)));
						}
					}
					else if (Slot.SlotName.IsNone() && !bIndexValid)
					{
						Add(Out, EViewerIssueSeverity::Error, Cat, Field + TEXT(".SlotIndex"),
							FString::Printf(TEXT("%s: Slot Index %d이(가) 메시 '%s'의 슬롯 범위(0 ~ %d) 밖이라 이 재질은 적용되지 않습니다. Slot Name으로 지정하는 것을 권장합니다: %s"), *Where, Slot.SlotIndex, *Mesh->GetName(), NumSlots - 1, *JoinNames(SlotNames)));
					}
				}

				if (!Slot.Material)
				{
					Add(Out, EViewerIssueSeverity::Warning, Cat, Field + TEXT(".Material"),
						FString::Printf(TEXT("%s: Material이 비어 있어 이 슬롯은 바뀌지 않습니다. 적용할 Material(Instance)을 지정하거나 이 행을 지우세요."), *Where));
				}
			}
		}

		if (Mesh)
		{
			// Textures of the mesh's own slot materials, reported once each in a stable (name) order.
			TSet<UTexture*> Textures;
			TMap<const UTexture*, FString> TextureMaterial;
			for (const FSkeletalMaterial& SlotMaterial : Mesh->GetMaterials())
			{
				TSet<UTexture*> SlotTextures;
				CollectTextures(SlotMaterial.MaterialInterface, SlotTextures);
				for (UTexture* Texture : SlotTextures)
				{
					if (Texture && !TextureMaterial.Contains(Texture))
					{
						TextureMaterial.Add(Texture, GetNameSafe(SlotMaterial.MaterialInterface));
						Textures.Add(Texture);
					}
				}
			}
			TArray<UTexture*> Sorted = Textures.Array();
			Sorted.Sort([](const UTexture& A, const UTexture& B) { return A.GetName() < B.GetName(); });
			for (const UTexture* Texture : Sorted)
			{
				const FIntPoint Size = GetTextureSize(Texture);
				if (Size.X <= 0 || Size.Y <= 0)
				{
					continue;
				}
				const FString& MaterialName = TextureMaterial.FindChecked(Texture);
				if (!FMath::IsPowerOfTwo(Size.X) || !FMath::IsPowerOfTwo(Size.Y))
				{
					Add(Out, EViewerIssueSeverity::Warning, Cat, TEXT("SkeletalMesh.Materials"),
						FString::Printf(TEXT("텍스처 '%s'(%dx%d, 재질 '%s')의 크기가 2의 거듭제곱이 아닙니다. 밉맵·텍스처 스트리밍이 동작하지 않아 멀리서 지글거리고 메모리를 더 씁니다. 1024·2048처럼 2의 거듭제곱 크기로 다시 저장해 Import하세요."),
							*Texture->GetName(), Size.X, Size.Y, *MaterialName));
				}
				if (FMath::Max(Size.X, Size.Y) > UCharacterProfileValidator::LargeTextureSize)
				{
					Add(Out, EViewerIssueSeverity::Info, Cat, TEXT("SkeletalMesh.Materials"),
						FString::Printf(TEXT("텍스처 '%s'(%dx%d, 재질 '%s')이(가) %d보다 큽니다. 뷰어에는 보통 %d 이하로 충분합니다(메모리·로딩 시간). 필요하면 텍스처 에디터의 Maximum Texture Size로 줄이세요."),
							*Texture->GetName(), Size.X, Size.Y, *MaterialName, UCharacterProfileValidator::LargeTextureSize, UCharacterProfileValidator::LargeTextureSize));
				}
			}
		}

		if (!Profile.WireframeMaterial)
		{
			Add(Out, EViewerIssueSeverity::Info, Cat, TEXT("WireframeMaterial"),
				TEXT("Wireframe Material이 비어 있어 이 캐릭터에서는 Wireframe(W) 버튼이 비활성화됩니다. 쓰려면 Details → Part → Wireframe Material에 /Game/Portfolio/Materials/M_Wireframe를 지정하세요."));
		}
	}

	// ---------------------------------------------------------------- Part

	void CheckParts(const UCharacterProfileData& Profile, FIssues& Out)
	{
		const FName Cat = UCharacterProfileValidator::CategoryPart;
		const USkeletalMesh* Mesh = Profile.SkeletalMesh;
		const TArray<FString> SlotNames = GetSlotNames(Mesh);

		TMap<FName, int32> SeenIds;
		TMap<FName, int32> BoneOwner;
		for (int32 PartIndex = 0; PartIndex < Profile.Parts.Num(); ++PartIndex)
		{
			const FViewerPartInfo& Part = Profile.Parts[PartIndex];
			const FString Field = FString::Printf(TEXT("Parts[%d]"), PartIndex);
			const FString Where = FString::Printf(TEXT("Parts[%d] (%s)"), PartIndex, *Label(Part.Id));

			if (Part.Id.IsNone())
			{
				Add(Out, EViewerIssueSeverity::Warning, Cat, Field + TEXT(".Id"),
					FString::Printf(TEXT("Parts[%d]의 Id가 비어 있어 클릭해도 선택 상태가 되지 않습니다. 'Head'처럼 고유한 Id를 입력하세요."), PartIndex));
			}
			else if (const int32* First = SeenIds.Find(Part.Id))
			{
				Add(Out, EViewerIssueSeverity::Warning, Cat, Field + TEXT(".Id"),
					FString::Printf(TEXT("Parts[%d]의 Id '%s'이(가) Parts[%d]과(와) 중복됩니다. 선택하면 항상 Parts[%d]의 정보가 표시됩니다. 다른 Id로 바꾸세요."), PartIndex, *Part.Id.ToString(), *First, *First));
			}
			else
			{
				SeenIds.Add(Part.Id, PartIndex);
			}

			if (Part.BoneNames.Num() == 0 && Part.MaterialSlotNames.Num() == 0 && Part.ComponentTag.IsNone())
			{
				Add(Out, EViewerIssueSeverity::Warning, Cat, Field,
					FString::Printf(TEXT("%s에 Bone Names, Material Slot Names, Component Tag가 모두 비어 있어 이 파츠는 클릭으로 선택되지 않습니다. 파츠를 이루는 본 이름을 Bone Names에 입력하세요."), *Where));
			}

			for (int32 BoneIndex = 0; BoneIndex < Part.BoneNames.Num(); ++BoneIndex)
			{
				const FName Bone = Part.BoneNames[BoneIndex];
				const FString BoneField = FString::Printf(TEXT("%s.BoneNames[%d]"), *Field, BoneIndex);
				if (Mesh && Mesh->GetRefSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
				{
					const FString Suggestion = SuggestBoneName(Mesh->GetRefSkeleton(), Bone);
					const FString Hint = Suggestion.IsEmpty()
						? FString(TEXT("Skeleton 에디터의 Skeleton Tree에 보이는 본 이름을 그대로 입력하세요(대소문자는 구분하지 않지만 밑줄·좌우 표기는 같아야 합니다)."))
						: FString::Printf(TEXT("비슷한 본: %s — 이 이름으로 고치세요."), *Suggestion);
					Add(Out, EViewerIssueSeverity::Error, Cat, BoneField,
						FString::Printf(TEXT("%s의 Bone Names[%d] '%s'이(가) 메시 '%s'의 본 목록에 없어 이 본으로는 파츠가 선택되지 않습니다. %s"), *Where, BoneIndex, *Bone.ToString(), *Mesh->GetName(), *Hint));
				}

				if (const int32* Owner = BoneOwner.Find(Bone))
				{
					if (*Owner != PartIndex)
					{
						Add(Out, EViewerIssueSeverity::Warning, Cat, BoneField,
							FString::Printf(TEXT("본 '%s'이(가) Parts[%d] (%s)과(와) %s에 모두 들어 있습니다. 이 본을 클릭하면 항상 앞쪽 Parts[%d]이(가) 선택됩니다. 한쪽 파츠에서 지우세요."),
								*Bone.ToString(), *Owner, *Label(Profile.Parts[*Owner].Id), *Where, *Owner));
					}
				}
				else
				{
					BoneOwner.Add(Bone, PartIndex);
				}
			}

			if (Mesh)
			{
				for (int32 SlotIndex = 0; SlotIndex < Part.MaterialSlotNames.Num(); ++SlotIndex)
				{
					const FName SlotName = Part.MaterialSlotNames[SlotIndex];
					if (FindSlotIndex(Mesh, SlotName) == INDEX_NONE)
					{
						Add(Out, EViewerIssueSeverity::Error, Cat, FString::Printf(TEXT("%s.MaterialSlotNames[%d]"), *Field, SlotIndex),
							FString::Printf(TEXT("%s의 Material Slot Names[%d] '%s'이(가) 메시 '%s'에 없어 슬롯 강조와 실측 수치에 쓰이지 않습니다. 메시 슬롯 이름으로 고치세요: %s"), *Where, SlotIndex, *SlotName.ToString(), *Mesh->GetName(), *JoinNames(SlotNames)));
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------- Play

	void CheckPlay(const UCharacterProfileData& Profile, FIssues& Out)
	{
		if (Profile.WalkSpeed >= Profile.RunSpeed)
		{
			Add(Out, EViewerIssueSeverity::Warning, UCharacterProfileValidator::CategoryPlay, TEXT("WalkSpeed"),
				FString::Printf(TEXT("Walk Speed(%.0f)이(가) Run Speed(%.0f)보다 크거나 같아 플레이 데모에서 달리기가 걷기보다 빠르지 않습니다. Walk Speed < Run Speed로 고치세요(예: 300 / 600)."), Profile.WalkSpeed, Profile.RunSpeed));
		}
	}
}

TArray<FViewerProfileIssue> UCharacterProfileValidator::ValidateProfile(const UCharacterProfileData* Profile)
{
	using namespace CharacterProfileValidatorPrivate;

	FIssues Issues;
	if (!Profile)
	{
		Add(Issues, EViewerIssueSeverity::Error, CategoryMesh, TEXT("Profile"),
			TEXT("프로필(CharacterProfileData)이 없습니다. GameMode의 Default Profile/Profile Library 또는 PortfolioCharacter Actor의 Profile에 Data Asset을 지정하세요."));
		return Issues;
	}

	CheckMesh(*Profile, Issues);
	CheckCamera(*Profile, Issues);
	CheckAnimation(*Profile, Issues);
	CheckExpression(*Profile, Issues);
	CheckMaterial(*Profile, Issues);
	CheckParts(*Profile, Issues);
	CheckPlay(*Profile, Issues);
	return Issues;
}

FString UCharacterProfileValidator::SeverityToString(EViewerIssueSeverity Severity)
{
	switch (Severity)
	{
	case EViewerIssueSeverity::Error:
		return TEXT("Error");
	case EViewerIssueSeverity::Warning:
		return TEXT("Warning");
	default:
		return TEXT("Info");
	}
}

FString UCharacterProfileValidator::FormatReport(const TArray<FViewerProfileIssue>& Issues)
{
	FString Report = FString::Printf(TEXT("E=%d W=%d I=%d"),
		CountBySeverity(Issues, EViewerIssueSeverity::Error),
		CountBySeverity(Issues, EViewerIssueSeverity::Warning),
		CountBySeverity(Issues, EViewerIssueSeverity::Info));
	for (const FViewerProfileIssue& Issue : Issues)
	{
		Report += FString::Printf(TEXT("\n[%s][%s] %s (%s)"), *SeverityToString(Issue.Severity), *Issue.Category.ToString(), *Issue.Message, *Issue.Field);
	}
	return Report;
}

int32 UCharacterProfileValidator::CountBySeverity(const TArray<FViewerProfileIssue>& Issues, EViewerIssueSeverity Severity)
{
	int32 Count = 0;
	for (const FViewerProfileIssue& Issue : Issues)
	{
		Count += Issue.Severity == Severity ? 1 : 0;
	}
	return Count;
}

int32 UCharacterProfileValidator::CountByCategory(const TArray<FViewerProfileIssue>& Issues, FName Category, EViewerIssueSeverity Severity)
{
	int32 Count = 0;
	for (const FViewerProfileIssue& Issue : Issues)
	{
		Count += (Issue.Category == Category && Issue.Severity == Severity) ? 1 : 0;
	}
	return Count;
}

void UCharacterProfileValidator::LogProfileReport(const UCharacterProfileData* Profile, const TCHAR* Context)
{
	const TArray<FViewerProfileIssue> Issues = ValidateProfile(Profile);
	UE_LOG(LogTemp, Display, TEXT("[ProfileValidator] %s: %s E=%d W=%d I=%d"), Context, *GetNameSafe(Profile),
		CountBySeverity(Issues, EViewerIssueSeverity::Error),
		CountBySeverity(Issues, EViewerIssueSeverity::Warning),
		CountBySeverity(Issues, EViewerIssueSeverity::Info));
	for (const FViewerProfileIssue& Issue : Issues)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProfileValidator]   [%s][%s] %s (%s)"), *SeverityToString(Issue.Severity), *Issue.Category.ToString(), *Issue.Message, *Issue.Field);
	}
}
