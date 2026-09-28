#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"

// P2: bone-based part lookup and Wireframe/Variant material round-trip. Like
// Tests/CharacterViewerTests.cpp, none of these require any content asset
// (Skeletal Mesh, Material, etc.) -- see Docs/CHARACTER_VIEWER_SETUP.md
// section 10 and 13.11.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerPartLookupTest,
	"CharacterShowcase.Viewer.PartLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerPartLookupTest::RunTest(const FString& Parameters)
{
	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>();
	if (!TestNotNull(TEXT("Profile exists"), Profile))
	{
		return false;
	}

	FViewerPartInfo Head;
	Head.Id = FName(TEXT("Head"));
	Head.DisplayName = FText::FromString(TEXT("Head"));
	Head.BoneNames = { FName(TEXT("head")), FName(TEXT("neck_01")) };
	Profile->Parts.Add(Head);

	FViewerPartInfo Torso;
	Torso.Id = FName(TEXT("Torso"));
	Torso.DisplayName = FText::FromString(TEXT("Torso"));
	Torso.BoneNames = { FName(TEXT("pelvis")), FName(TEXT("spine_01")) };
	Profile->Parts.Add(Torso);

	// FindPart: exact id match, unknown id -> nullptr.
	const FViewerPartInfo* FoundHead = Profile->FindPart(FName(TEXT("Head")));
	if (TestNotNull(TEXT("FindPart finds 'Head'"), FoundHead))
	{
		TestEqual(TEXT("'Head' part display name"), FoundHead->DisplayName.ToString(), TEXT("Head"));
	}
	TestNull(TEXT("FindPart returns null for an unknown id"), Profile->FindPart(FName(TEXT("Unknown"))));
	TestNull(TEXT("FindPart returns null for NAME_None"), Profile->FindPart(NAME_None));

	// FindPartByBone: exact bone match (no parent walk -- that is the Controller's job), unknown bone -> nullptr.
	const FViewerPartInfo* FoundByBone = Profile->FindPartByBone(FName(TEXT("neck_01")));
	if (TestNotNull(TEXT("FindPartByBone finds the part containing 'neck_01'"), FoundByBone))
	{
		TestEqual(TEXT("'neck_01' resolves to the 'Head' part"), FoundByBone->Id, FName(TEXT("Head")));
	}
	const FViewerPartInfo* FoundTorsoByBone = Profile->FindPartByBone(FName(TEXT("spine_01")));
	if (TestNotNull(TEXT("FindPartByBone finds the part containing 'spine_01'"), FoundTorsoByBone))
	{
		TestEqual(TEXT("'spine_01' resolves to the 'Torso' part"), FoundTorsoByBone->Id, FName(TEXT("Torso")));
	}
	TestNull(TEXT("FindPartByBone returns null for a bone in no part (e.g. a finger)"), Profile->FindPartByBone(FName(TEXT("thumb_01_l"))));
	TestNull(TEXT("FindPartByBone returns null for NAME_None"), Profile->FindPartByBone(NAME_None));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerWireframeRestoreTest,
	"CharacterShowcase.Viewer.WireframeRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerWireframeRestoreTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());

	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	if (!TestNotNull(TEXT("Character actor exists"), Actor))
	{
		World->DestroyWorld(false);
		return false;
	}

	// --- A: no WireframeMaterial -> SetWireframeEnabled(true) is a safe no-op. ---
	UCharacterProfileData* NoWireframeProfile = NewObject<UCharacterProfileData>(World);
	Actor->ApplyProfile(NoWireframeProfile);
	TestFalse(TEXT("SetWireframeEnabled(true) fails without a WireframeMaterial"), Actor->SetWireframeEnabled(true));
	TestFalse(TEXT("IsWireframeEnabled() stays false without a WireframeMaterial"), Actor->IsWireframeEnabled());

	// --- B: Variant -> Wireframe -> off restores the variant material exactly. ---
	UMaterial* WireframeMat = NewObject<UMaterial>(World);
	UMaterial* VariantMat = NewObject<UMaterial>(World);

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->WireframeMaterial = WireframeMat;

	FViewerMaterialSlotOverride SlotOverride;
	SlotOverride.SlotIndex = 0;
	SlotOverride.Material = VariantMat;

	FViewerMaterialVariant GridVariant;
	GridVariant.Id = FName(TEXT("Grid"));
	GridVariant.Slots.Add(SlotOverride);
	Profile->MaterialVariants.Add(GridVariant);

	Actor->ApplyProfile(Profile);

	TestTrue(TEXT("SelectMaterialVariant('Grid') succeeds"), Actor->SetMaterialVariant(FName(TEXT("Grid"))));
	TestEqual(TEXT("Slot 0 material is the Grid variant material"), Actor->Mesh->GetMaterial(0), static_cast<UMaterialInterface*>(VariantMat));

	TestTrue(TEXT("SetWireframeEnabled(true) succeeds with a WireframeMaterial"), Actor->SetWireframeEnabled(true));
	TestTrue(TEXT("IsWireframeEnabled() reports true"), Actor->IsWireframeEnabled());
	TestEqual(TEXT("Slot 0 material is the wireframe material while Wireframe is on"), Actor->Mesh->GetMaterial(0), static_cast<UMaterialInterface*>(WireframeMat));

	TestTrue(TEXT("SetWireframeEnabled(false) succeeds"), Actor->SetWireframeEnabled(false));
	TestFalse(TEXT("IsWireframeEnabled() reports false"), Actor->IsWireframeEnabled());
	TestEqual(TEXT("Slot 0 material is restored to the Grid variant material exactly (not the raw override array)"), Actor->Mesh->GetMaterial(0), static_cast<UMaterialInterface*>(VariantMat));

	// --- C: ApplyProfile(nullptr) clears both Wireframe and the part selection. ---
	FViewerPartInfo SomePart;
	SomePart.Id = FName(TEXT("Torso"));
	Profile->Parts.Add(SomePart);
	Actor->SetSelectedPart(FName(TEXT("Torso")));
	Actor->SetWireframeEnabled(true);
	TestTrue(TEXT("Part is selected before ApplyProfile(nullptr)"), Actor->GetSelectedPartId() == FName(TEXT("Torso")));
	TestTrue(TEXT("Wireframe is on before ApplyProfile(nullptr)"), Actor->IsWireframeEnabled());
	if (Actor->HighlightOverlayMaterial)
	{
		TestNotNull(TEXT("Highlight overlay is set before ApplyProfile(nullptr)"), Actor->Mesh->GetOverlayMaterial());
	}

	Actor->ApplyProfile(nullptr);
	TestTrue(TEXT("ApplyProfile(nullptr) clears the part selection"), Actor->GetSelectedPartId() == NAME_None);
	TestFalse(TEXT("ApplyProfile(nullptr) clears Wireframe"), Actor->IsWireframeEnabled());
	TestEqual(TEXT("ApplyProfile(nullptr) leaves no material override (wireframe slots dropped)"), Actor->Mesh->GetNumOverrideMaterials(), 0);
	TestNull(TEXT("ApplyProfile(nullptr) removes the highlight overlay"), Actor->Mesh->GetOverlayMaterial());

	World->DestroyWorld(false);
	return true;
}

#endif
