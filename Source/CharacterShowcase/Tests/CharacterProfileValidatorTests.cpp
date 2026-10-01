#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Character/CharacterProfileData.h"
#include "Character/CharacterProfileValidator.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/Package.h"

// UCharacterProfileValidator (Docs/CHARACTER_VIEWER_SETUP.md section 2.10).
// Transient profiles/meshes/materials only, plus read-only loads of engine
// placeholder content (TutorialTPP, Tutorial_Idle), the third-person
// mannequin (MM_Idle, MI_Manny_01_New) and the three project profiles.
// Nothing is modified or saved.

namespace CharacterProfileValidatorTestsPrivate
{
	const TCHAR* TutorialMeshPath = TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP");
	const TCHAR* TutorialIdlePath = TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle");
	const TCHAR* MannyIdlePath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");
	const TCHAR* MannyMaterialPath = TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New");
	const TCHAR* MannyProfilePath = TEXT("/Game/Portfolio/Data/DA_Character_Manny.DA_Character_Manny");
	const TCHAR* TutorialProfilePath = TEXT("/Game/Portfolio/Data/DA_Character.DA_Character");
	const TCHAR* CubeProfilePath = TEXT("/Game/Portfolio/Data/DA_Character_Cube.DA_Character_Cube");

	using V = UCharacterProfileValidator;
	constexpr EViewerIssueSeverity Err = EViewerIssueSeverity::Error;
	constexpr EViewerIssueSeverity Warn = EViewerIssueSeverity::Warning;
	constexpr EViewerIssueSeverity Inf = EViewerIssueSeverity::Info;

	// Asserts the (Error, Warning, Info) counts of one category.
	void ExpectCategory(FAutomationTestBase& Test, const TArray<FViewerProfileIssue>& Issues, FName Category, int32 Errors, int32 Warnings, int32 Infos)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s errors"), *Category.ToString()), V::CountByCategory(Issues, Category, Err), Errors);
		Test.TestEqual(*FString::Printf(TEXT("%s warnings"), *Category.ToString()), V::CountByCategory(Issues, Category, Warn), Warnings);
		Test.TestEqual(*FString::Printf(TEXT("%s infos"), *Category.ToString()), V::CountByCategory(Issues, Category, Inf), Infos);
	}

	// The issue whose Field is exactly Field (and Severity), or nullptr.
	const FViewerProfileIssue* FindIssue(const TArray<FViewerProfileIssue>& Issues, const FString& Field, EViewerIssueSeverity Severity)
	{
		return Issues.FindByPredicate([&](const FViewerProfileIssue& Issue) { return Issue.Field == Field && Issue.Severity == Severity; });
	}

	void ExpectMessageContains(FAutomationTestBase& Test, const TArray<FViewerProfileIssue>& Issues, const FString& Field, EViewerIssueSeverity Severity, const TCHAR* Needle)
	{
		const FViewerProfileIssue* Issue = FindIssue(Issues, Field, Severity);
		if (Test.TestNotNull(*FString::Printf(TEXT("Issue at %s (%s)"), *Field, *V::SeverityToString(Severity)), Issue))
		{
			Test.TestTrue(*FString::Printf(TEXT("%s message mentions \"%s\": %s"), *Field, Needle, *Issue->Message), Issue->Message.Contains(Needle, ESearchCase::CaseSensitive));
		}
	}

	FViewerCameraPreset MakePreset(const TCHAR* Id)
	{
		FViewerCameraPreset Preset;
		Preset.Id = Id ? FName(Id) : NAME_None;
		return Preset;
	}

	FViewerAnimationEntry MakeAnimation(const TCHAR* Id, UAnimSequence* Sequence)
	{
		FViewerAnimationEntry Entry;
		Entry.Id = Id ? FName(Id) : NAME_None;
		Entry.Sequence = Sequence;
		return Entry;
	}

	FViewerMaterialSlotOverride MakeSlot(const TCHAR* SlotName, int32 SlotIndex, UMaterialInterface* Material)
	{
		FViewerMaterialSlotOverride Slot;
		Slot.SlotName = SlotName ? FName(SlotName) : NAME_None;
		Slot.SlotIndex = SlotIndex;
		Slot.Material = Material;
		return Slot;
	}

	FViewerPartInfo MakePart(const TCHAR* Id, TArray<FName> Bones, TArray<FName> Slots = {}, FName Tag = NAME_None)
	{
		FViewerPartInfo Part;
		Part.Id = FName(Id);
		Part.BoneNames = MoveTemp(Bones);
		Part.MaterialSlotNames = MoveTemp(Slots);
		Part.ComponentTag = Tag;
		return Part;
	}

	void LogIssues(FAutomationTestBase& Test, const FString& Name, const TArray<FViewerProfileIssue>& Issues)
	{
		Test.AddInfo(FString::Printf(TEXT("%s: %s"), *Name, *V::FormatReport(Issues)));
	}
}

// Null profile and an empty (default-constructed) profile.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterProfileValidatorEmptyTest,
	"CharacterShowcase.Validator.NullAndEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterProfileValidatorEmptyTest::RunTest(const FString& Parameters)
{
	using namespace CharacterProfileValidatorTestsPrivate;

	const TArray<FViewerProfileIssue> NullIssues = V::ValidateProfile(nullptr);
	TestEqual(TEXT("Null profile: 1 issue"), NullIssues.Num(), 1);
	TestEqual(TEXT("Null profile: 1 error"), V::CountBySeverity(NullIssues, Err), 1);
	TestTrue(TEXT("FormatReport summary line"), V::FormatReport(NullIssues).StartsWith(TEXT("E=1 W=0 I=0\n[Error][Mesh] ")));

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(GetTransientPackage());
	const TArray<FViewerProfileIssue> Issues = V::ValidateProfile(Profile);
	LogIssues(*this, TEXT("Empty profile"), Issues);
	// No mesh (Error) + empty Display Name (Warning) + no Wireframe Material
	// (Info). Default camera framing and Walk/Run speeds are valid.
	TestEqual(TEXT("Empty profile: issue count"), Issues.Num(), 3);
	ExpectCategory(*this, Issues, V::CategoryMesh, 1, 1, 0);
	ExpectCategory(*this, Issues, V::CategoryMaterial, 0, 0, 1);
	TestNotNull(TEXT("SkeletalMesh error"), FindIssue(Issues, TEXT("SkeletalMesh"), Err));
	TestNotNull(TEXT("DisplayName warning"), FindIssue(Issues, TEXT("DisplayName"), Warn));
	TestNotNull(TEXT("WireframeMaterial info"), FindIssue(Issues, TEXT("WireframeMaterial"), Inf));
	return true;
}

// One profile on TutorialTPP with a deliberate mistake for (almost) every check.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterProfileValidatorMistakesTest,
	"CharacterShowcase.Validator.DeliberateMistakes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterProfileValidatorMistakesTest::RunTest(const FString& Parameters)
{
	using namespace CharacterProfileValidatorTestsPrivate;

	USkeletalMesh* Tutorial = LoadObject<USkeletalMesh>(nullptr, TutorialMeshPath);
	UAnimSequence* TutorialIdle = LoadObject<UAnimSequence>(nullptr, TutorialIdlePath);
	UAnimSequence* MannyIdle = LoadObject<UAnimSequence>(nullptr, MannyIdlePath);
	UMaterialInterface* Grid = UMaterial::GetDefaultMaterial(MD_Surface);
	if (!TestNotNull(TEXT("TutorialTPP loads"), Tutorial) || !TestNotNull(TEXT("Tutorial_Idle loads"), TutorialIdle)
		|| !TestNotNull(TEXT("MM_Idle loads"), MannyIdle) || !TestNotNull(TEXT("Default material"), Grid))
	{
		return false;
	}

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(GetTransientPackage());
	Profile->DisplayName = FText::FromString(TEXT("Broken on purpose"));
	Profile->SkeletalMesh = Tutorial; // Skeleton + Physics Asset, ~192 cm, 1 slot "TutorialTPP_Mat", 0 morphs, 0 textures.

	// Camera: 3 errors (preset 0 range/pitch/FOV), 3 warnings (duplicate id, empty id, DefaultPresetId not found).
	FViewerCameraPreset BadRange = MakePreset(TEXT("Full"));
	BadRange.Framing.MinDistance = 500.f;
	BadRange.Framing.MaxDistance = 100.f;
	BadRange.Framing.MinPitch = 10.f;
	BadRange.Framing.MaxPitch = -10.f;
	BadRange.Framing.FOV = 150.f;
	Profile->CameraPresets.Add(BadRange);
	Profile->CameraPresets.Add(MakePreset(TEXT("Full")));
	Profile->CameraPresets.Add(MakePreset(nullptr));
	Profile->DefaultPresetId = FName(TEXT("Face"));

	// Animation: 5 errors (foreign skeleton, duplicate id, empty id, null sequence,
	// DefaultAnimationId not found), 1 warning (PoseTime past the end), 1 info (AnimClass + Id).
	FViewerAnimationEntry LatePose = MakeAnimation(TEXT("Idle"), TutorialIdle);
	LatePose.bIsPose = true;
	LatePose.PoseTime = 100.f;
	Profile->Animations.Add(LatePose);
	Profile->Animations.Add(MakeAnimation(TEXT("MannyIdle"), MannyIdle));
	Profile->Animations.Add(MakeAnimation(TEXT("Idle"), TutorialIdle));
	Profile->Animations.Add(MakeAnimation(nullptr, nullptr));
	Profile->DefaultAnimationId = FName(TEXT("Run"));
	Profile->DefaultAnimClass = UAnimInstance::StaticClass();

	// Expression: the mesh has no Morph Targets -> 1 warning for the whole list.
	FViewerExpression Smile;
	Smile.Id = FName(TEXT("Smile"));
	FViewerMorphWeight SmileMorph;
	SmileMorph.MorphName = FName(TEXT("Smile_L"));
	Smile.Morphs.Add(SmileMorph);
	Profile->Expressions.Add(Smile);
	FViewerExpression Neutral;
	Neutral.Id = FName(TEXT("Neutral"));
	Profile->Expressions.Add(Neutral);

	// Material: 2 errors (unknown slot name without fallback, index out of range),
	// 3 warnings (no slot at all, null material, unknown name with valid index fallback),
	// 1 info (no Wireframe Material).
	FViewerMaterialVariant Bad;
	Bad.Id = FName(TEXT("Bad"));
	Bad.Slots.Add(MakeSlot(TEXT("Body"), INDEX_NONE, Grid));
	Bad.Slots.Add(MakeSlot(nullptr, 5, Grid));
	Bad.Slots.Add(MakeSlot(nullptr, INDEX_NONE, Grid));
	Bad.Slots.Add(MakeSlot(TEXT("TutorialTPP_Mat"), INDEX_NONE, nullptr));
	Bad.Slots.Add(MakeSlot(TEXT("Wrong"), 0, Grid));
	Profile->MaterialVariants.Add(Bad);
	Profile->WireframeMaterial = nullptr;

	// Part: 4 errors ("Spine01" separator, "hand" side, "upperarm_left" side, slot "Sleeve"),
	// 3 warnings (neck_01 in two parts, duplicate id "Head", a part with nothing to click).
	// "HEAD" is fine: FName (and the engine's bone lookup) ignores case.
	Profile->Parts.Add(MakePart(TEXT("Head"), { FName(TEXT("Spine01")), FName(TEXT("neck_01")), FName(TEXT("HEAD")) }));
	Profile->Parts.Add(MakePart(TEXT("Arm"), { FName(TEXT("hand")), FName(TEXT("neck_01")) }, { FName(TEXT("Sleeve")) }));
	Profile->Parts.Add(MakePart(TEXT("Head"), { FName(TEXT("upperarm_left")) }));
	Profile->Parts.Add(MakePart(TEXT("Empty"), {}));
	Profile->Parts.Add(MakePart(TEXT("Tagged"), {}, {}, FName(TEXT("Hair")))); // ComponentTag only: valid.

	// Play: 1 warning.
	Profile->WalkSpeed = 700.f;
	Profile->RunSpeed = 600.f;

	const TArray<FViewerProfileIssue> Issues = V::ValidateProfile(Profile);
	LogIssues(*this, TEXT("Deliberate mistakes"), Issues);

	ExpectCategory(*this, Issues, V::CategoryMesh, 0, 0, 0);
	ExpectCategory(*this, Issues, V::CategoryCamera, 3, 3, 0);
	ExpectCategory(*this, Issues, V::CategoryAnimation, 5, 1, 1);
	ExpectCategory(*this, Issues, V::CategoryExpression, 0, 1, 0);
	ExpectCategory(*this, Issues, V::CategoryMaterial, 2, 3, 1);
	ExpectCategory(*this, Issues, V::CategoryPart, 4, 3, 0);
	ExpectCategory(*this, Issues, V::CategoryPlay, 0, 1, 0);
	TestEqual(TEXT("Total errors"), V::CountBySeverity(Issues, Err), 14);
	TestEqual(TEXT("Total warnings"), V::CountBySeverity(Issues, Warn), 12);
	TestEqual(TEXT("Total infos"), V::CountBySeverity(Issues, Inf), 2);
	TestEqual(TEXT("Total issues"), Issues.Num(), 28);

	// Where each finding points, and that the message tells how to fix it.
	TestNotNull(TEXT("Preset 0 distance range"), FindIssue(Issues, TEXT("CameraPresets[0].Framing.MinDistance"), Err));
	TestNotNull(TEXT("Preset 0 pitch range"), FindIssue(Issues, TEXT("CameraPresets[0].Framing.MinPitch"), Err));
	TestNotNull(TEXT("Preset 0 FOV"), FindIssue(Issues, TEXT("CameraPresets[0].Framing.FOV"), Err));
	TestNotNull(TEXT("Preset 1 duplicate id"), FindIssue(Issues, TEXT("CameraPresets[1].Id"), Warn));
	TestNotNull(TEXT("Preset 2 empty id"), FindIssue(Issues, TEXT("CameraPresets[2].Id"), Warn));
	ExpectMessageContains(*this, Issues, TEXT("DefaultPresetId"), Warn, TEXT("Full"));

	ExpectMessageContains(*this, Issues, TEXT("Animations[1].Sequence"), Err, TEXT("IK Retargeter"));
	ExpectMessageContains(*this, Issues, TEXT("Animations[1].Sequence"), Err, TEXT("TutorialTPP_Skeleton"));
	TestNotNull(TEXT("Animations[0] pose time"), FindIssue(Issues, TEXT("Animations[0].PoseTime"), Warn));
	TestNotNull(TEXT("Animations[2] duplicate id"), FindIssue(Issues, TEXT("Animations[2].Id"), Err));
	TestNotNull(TEXT("Animations[3] empty id"), FindIssue(Issues, TEXT("Animations[3].Id"), Err));
	TestNotNull(TEXT("Animations[3] null sequence"), FindIssue(Issues, TEXT("Animations[3].Sequence"), Err));
	ExpectMessageContains(*this, Issues, TEXT("DefaultAnimationId"), Err, TEXT("Idle, MannyIdle"));
	TestNotNull(TEXT("AnimClass wins info"), FindIssue(Issues, TEXT("DefaultAnimClass"), Inf));

	ExpectMessageContains(*this, Issues, TEXT("Expressions"), Warn, TEXT("Smile_L"));

	ExpectMessageContains(*this, Issues, TEXT("MaterialVariants[0].Slots[0].SlotName"), Err, TEXT("TutorialTPP_Mat(0)"));
	ExpectMessageContains(*this, Issues, TEXT("MaterialVariants[0].Slots[1].SlotIndex"), Err, TEXT("TutorialTPP_Mat(0)"));
	TestNotNull(TEXT("Variant slot without name/index"), FindIssue(Issues, TEXT("MaterialVariants[0].Slots[2]"), Warn));
	TestNotNull(TEXT("Variant slot null material"), FindIssue(Issues, TEXT("MaterialVariants[0].Slots[3].Material"), Warn));
	TestNotNull(TEXT("Variant slot name fallback to index"), FindIssue(Issues, TEXT("MaterialVariants[0].Slots[4].SlotName"), Warn));

	ExpectMessageContains(*this, Issues, TEXT("Parts[0].BoneNames[0]"), Err, TEXT("'spine_01'"));
	TestNull(TEXT("'HEAD' matches bone 'head' (case-insensitive)"), FindIssue(Issues, TEXT("Parts[0].BoneNames[2]"), Err));
	ExpectMessageContains(*this, Issues, TEXT("Parts[1].BoneNames[0]"), Err, TEXT("'hand_l'"));
	ExpectMessageContains(*this, Issues, TEXT("Parts[1].BoneNames[0]"), Err, TEXT("'hand_r'"));
	ExpectMessageContains(*this, Issues, TEXT("Parts[1].BoneNames[1]"), Warn, TEXT("neck_01"));
	ExpectMessageContains(*this, Issues, TEXT("Parts[1].MaterialSlotNames[0]"), Err, TEXT("TutorialTPP_Mat(0)"));
	TestNotNull(TEXT("Parts[2] duplicate id"), FindIssue(Issues, TEXT("Parts[2].Id"), Warn));
	ExpectMessageContains(*this, Issues, TEXT("Parts[2].BoneNames[0]"), Err, TEXT("'upperarm_l'"));
	TestNotNull(TEXT("Parts[3] nothing to click"), FindIssue(Issues, TEXT("Parts[3]"), Warn));
	TestNull(TEXT("Parts[4] (ComponentTag only) is fine"), FindIssue(Issues, TEXT("Parts[4]"), Warn));

	TestNotNull(TEXT("Walk >= Run"), FindIssue(Issues, TEXT("WalkSpeed"), Warn));
	return true;
}

// A transient mesh (no Skeleton, no Physics Asset, 12 Morph Targets, one slot
// whose Material Instance uses a non-power-of-two and a > 4096 texture).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterProfileValidatorTransientMeshTest,
	"CharacterShowcase.Validator.MorphsTexturesSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterProfileValidatorTransientMeshTest::RunTest(const FString& Parameters)
{
	using namespace CharacterProfileValidatorTestsPrivate;

	UMaterialInstance* MannyMaterial = LoadObject<UMaterialInstance>(nullptr, MannyMaterialPath);
	if (!TestNotNull(TEXT("MI_Manny_01_New loads"), MannyMaterial)
		|| !TestTrue(TEXT("MI_Manny_01_New has >= 2 texture parameters"), MannyMaterial->TextureParameterValues.Num() >= 2))
	{
		return false;
	}

	USkeletalMesh* Mesh = NewObject<USkeletalMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), USkeletalMesh::StaticClass(), TEXT("SK_ValidatorTestMesh")));
	TArray<FName> MorphNames = { FName(TEXT("Smile_L")), FName(TEXT("Blink_L")) };
	for (int32 Index = 0; Index < 10; ++Index)
	{
		MorphNames.Add(FName(*FString::Printf(TEXT("Extra_%02d"), Index)));
	}
	for (const FName& MorphName : MorphNames)
	{
		Mesh->GetMorphTargets().Add(NewObject<UMorphTarget>(Mesh, MorphName));
	}
	Mesh->InitMorphTargets(/*bInKeepEmptyMorphTargets=*/true);
	if (!TestNotNull(TEXT("Transient mesh finds its morph 'Smile_L'"), Mesh->FindMorphTarget(FName(TEXT("Smile_L")))))
	{
		return false;
	}

	// A transient child of MI_Manny_01_New that swaps two of its textures for
	// a 300x200 (not a power of two) and an 8192x16 (> 4096) texture.
	UPackage* Transient = GetTransientPackage();
	UTexture2D* OddTexture = UTexture2D::CreateTransient(300, 200, PF_B8G8R8A8, MakeUniqueObjectName(Transient, UTexture2D::StaticClass(), TEXT("T_ValidatorOdd")));
	UTexture2D* WideTexture = UTexture2D::CreateTransient(8192, 16, PF_B8G8R8A8, MakeUniqueObjectName(Transient, UTexture2D::StaticClass(), TEXT("T_ValidatorWide")));
	if (!TestNotNull(TEXT("Odd texture"), OddTexture) || !TestNotNull(TEXT("Wide texture"), WideTexture))
	{
		return false;
	}
	UMaterialInstanceConstant* Material = NewObject<UMaterialInstanceConstant>(Transient, MakeUniqueObjectName(Transient, UMaterialInstanceConstant::StaticClass(), TEXT("MI_ValidatorTest")));
	Material->Parent = MannyMaterial;
	FTextureParameterValue OddValue;
	OddValue.ParameterInfo = MannyMaterial->TextureParameterValues[0].ParameterInfo;
	OddValue.ParameterValue = OddTexture;
	Material->TextureParameterValues.Add(OddValue);
	FTextureParameterValue WideValue;
	WideValue.ParameterInfo = MannyMaterial->TextureParameterValues[1].ParameterInfo;
	WideValue.ParameterValue = WideTexture;
	Material->TextureParameterValues.Add(WideValue);
	Mesh->GetMaterials().Add(FSkeletalMaterial(Material, FName(TEXT("Body"))));

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(GetTransientPackage());
	Profile->DisplayName = FText::FromString(TEXT("Transient"));
	Profile->SkeletalMesh = Mesh;
	Profile->WireframeMaterial = MannyMaterial;
	FViewerExpression Smile;
	Smile.Id = FName(TEXT("Smile"));
	for (const TCHAR* Name : { TEXT("Smile_L"), TEXT("smile_r") })
	{
		FViewerMorphWeight Weight;
		Weight.MorphName = FName(Name);
		Smile.Morphs.Add(Weight);
	}
	Profile->Expressions.Add(Smile);
	FViewerExpression Blink;
	Blink.Id = FName(TEXT("Blink"));
	FViewerMorphWeight BlinkWeight;
	BlinkWeight.MorphName = FName(TEXT("Blink_R"));
	Blink.Morphs.Add(BlinkWeight);
	Profile->Expressions.Add(Blink);

	const TArray<FViewerProfileIssue> Issues = V::ValidateProfile(Profile);
	LogIssues(*this, TEXT("Transient mesh"), Issues);

	// Mesh: no Skeleton (Error), no Physics Asset with no Parts (Info), and in
	// the editor the 0 cm import-scale hint (Warning).
	ExpectCategory(*this, Issues, V::CategoryMesh, 1, WITH_EDITOR ? 1 : 0, 1);
	TestNotNull(TEXT("No Skeleton error"), FindIssue(Issues, TEXT("SkeletalMesh.Skeleton"), Err));
	TestNotNull(TEXT("No Physics Asset info (no parts)"), FindIssue(Issues, TEXT("SkeletalMesh.PhysicsAsset"), Inf));
#if WITH_EDITOR
	ExpectMessageContains(*this, Issues, TEXT("SkeletalMesh"), Warn, TEXT("스케일 확인: 높이 0 cm"));
#endif

	// Expression: 'smile_r' and 'Blink_R' are missing (2 errors); the message
	// lists at most 10 available names, then "외 2개".
	ExpectCategory(*this, Issues, V::CategoryExpression, 2, 0, 0);
	ExpectMessageContains(*this, Issues, TEXT("Expressions[0].Morphs[1].MorphName"), Err, TEXT("Smile_L, Blink_L, Extra_00"));
	ExpectMessageContains(*this, Issues, TEXT("Expressions[0].Morphs[1].MorphName"), Err, TEXT("외 2개"));
	TestNotNull(TEXT("Blink_R missing"), FindIssue(Issues, TEXT("Expressions[1].Morphs[0].MorphName"), Err));
	TestNull(TEXT("Smile_L exists"), FindIssue(Issues, TEXT("Expressions[0].Morphs[0].MorphName"), Err));

	// Material: one non-power-of-two texture (Warning) and one > 4096 (Info).
	ExpectCategory(*this, Issues, V::CategoryMaterial, 0, 1, 1);
	ExpectMessageContains(*this, Issues, TEXT("SkeletalMesh.Materials"), Warn, TEXT("(300x200"));
	ExpectMessageContains(*this, Issues, TEXT("SkeletalMesh.Materials"), Inf, TEXT("(8192x16"));

	ExpectCategory(*this, Issues, V::CategoryCamera, 0, 0, 0);
	ExpectCategory(*this, Issues, V::CategoryAnimation, 0, 0, 0);
	ExpectCategory(*this, Issues, V::CategoryPart, 0, 0, 0);
	ExpectCategory(*this, Issues, V::CategoryPlay, 0, 0, 0);
	return true;
}

// The project's own profiles: Manny (the default) must have no Error; the
// Skeletal Cube has no Physics Asset, so its part is reported as not clickable.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterProfileValidatorContentTest,
	"CharacterShowcase.Validator.ContentProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterProfileValidatorContentTest::RunTest(const FString& Parameters)
{
	using namespace CharacterProfileValidatorTestsPrivate;

	UCharacterProfileData* Manny = LoadObject<UCharacterProfileData>(nullptr, MannyProfilePath);
	UCharacterProfileData* Tutorial = LoadObject<UCharacterProfileData>(nullptr, TutorialProfilePath);
	UCharacterProfileData* Cube = LoadObject<UCharacterProfileData>(nullptr, CubeProfilePath);
	if (!TestNotNull(TEXT("DA_Character_Manny loads"), Manny) || !TestNotNull(TEXT("DA_Character loads"), Tutorial)
		|| !TestNotNull(TEXT("DA_Character_Cube loads"), Cube))
	{
		return false;
	}

	const TArray<FViewerProfileIssue> MannyIssues = V::ValidateProfile(Manny);
	LogIssues(*this, TEXT("DA_Character_Manny"), MannyIssues);
	TestEqual(TEXT("DA_Character_Manny has 0 errors"), V::CountBySeverity(MannyIssues, Err), 0);

	const TArray<FViewerProfileIssue> TutorialIssues = V::ValidateProfile(Tutorial);
	LogIssues(*this, TEXT("DA_Character"), TutorialIssues);
	TestEqual(TEXT("DA_Character has 0 errors"), V::CountBySeverity(TutorialIssues, Err), 0);

	const TArray<FViewerProfileIssue> CubeIssues = V::ValidateProfile(Cube);
	LogIssues(*this, TEXT("DA_Character_Cube"), CubeIssues);
	ExpectMessageContains(*this, CubeIssues, TEXT("SkeletalMesh.PhysicsAsset"), Warn, TEXT("Physics Asset이 없어 파츠 1개를 클릭으로 선택할 수 없습니다"));
	TestEqual(TEXT("DA_Character_Cube has 0 errors"), V::CountBySeverity(CubeIssues, Err), 0);
	return true;
}

#endif
