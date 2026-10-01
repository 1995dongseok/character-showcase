#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "UI/CharacterViewerWidget.h"

// P2: bone-based part lookup and Wireframe/Variant material round-trip. The
// first two tests need no content asset. The per-part highlight / measured
// stats / skeleton compatibility tests below (Docs/CHARACTER_VIEWER_SETUP.md
// section 6.11) load engine placeholder content (TutorialTPP, Tutorial_Idle)
// and the project's third-person mannequin (SKM_Manny_Simple, 2 material
// slots, MM_Idle) read-only; nothing is modified or saved.

namespace CharacterViewerInspectionTestsPrivate
{
	const TCHAR* TutorialMeshPath = TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP");
	const TCHAR* TutorialIdlePath = TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle");
	const TCHAR* MannyMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* MannyIdlePath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");

	FString ModeName(EViewerHighlightMode Mode)
	{
		return UEnum::GetValueAsString(Mode);
	}

	// A transient game world with actors initialized (so PostInitializeComponents runs), like the tests above.
	UWorld* CreateTestWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			World->InitializeActorsForPlay(FURL());
		}
		return World;
	}
}

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

	// --- A: neither the overlay material nor a profile WireframeMaterial ->
	// SetWireframeEnabled(true) is a safe no-op. (The default overlay material
	// alone is enough: see Viewer.WireframeOverlay.)
	UCharacterProfileData* NoWireframeProfile = NewObject<UCharacterProfileData>(World);
	Actor->ApplyProfile(NoWireframeProfile);
	UMaterialInterface* DefaultOverlay = Actor->WireframeOverlayMaterial;
	Actor->WireframeOverlayMaterial = nullptr;
	TestFalse(TEXT("IsWireframeAvailable() is false with no wireframe material at all"), Actor->IsWireframeAvailable());
	TestFalse(TEXT("SetWireframeEnabled(true) fails with no wireframe material at all"), Actor->SetWireframeEnabled(true));
	TestFalse(TEXT("IsWireframeEnabled() stays false with no wireframe material at all"), Actor->IsWireframeEnabled());
	TestTrue(TEXT("SetWireframeEnabled(false) always succeeds"), Actor->SetWireframeEnabled(false));
	if (DefaultOverlay)
	{
		Actor->WireframeOverlayMaterial = DefaultOverlay;
		TestTrue(TEXT("The default overlay material alone makes Wireframe available (profile has no WireframeMaterial)"), Actor->IsWireframeAvailable());
		Actor->WireframeOverlayMaterial = nullptr;
	}

	// --- B: LEGACY slot path. Variant -> Wireframe -> off restores the variant
	// material exactly. Covers both ways into ReplaceSlots: no overlay
	// material (fallback), and bWireframeReplacesSlots with the overlay present.
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

	auto CheckLegacyRoundTrip = [&](const TCHAR* Label)
	{
		TestTrue(FString::Printf(TEXT("%s: SetWireframeEnabled(true) succeeds with a WireframeMaterial"), Label), Actor->SetWireframeEnabled(true));
		TestTrue(FString::Printf(TEXT("%s: IsWireframeEnabled() reports true"), Label), Actor->IsWireframeEnabled());
		TestEqual(FString::Printf(TEXT("%s: mode is ReplaceSlots"), Label), UEnum::GetValueAsString(Actor->GetActiveWireframeMode()), UEnum::GetValueAsString(EViewerWireframeMode::ReplaceSlots));
		TestEqual(FString::Printf(TEXT("%s: slot 0 material is the wireframe material while Wireframe is on"), Label), Actor->Mesh->GetMaterial(0), static_cast<UMaterialInterface*>(WireframeMat));
		TestNull(FString::Printf(TEXT("%s: no overlay material in ReplaceSlots mode"), Label), Actor->Mesh->GetOverlayMaterial());

		TestTrue(FString::Printf(TEXT("%s: SetWireframeEnabled(false) succeeds"), Label), Actor->SetWireframeEnabled(false));
		TestFalse(FString::Printf(TEXT("%s: IsWireframeEnabled() reports false"), Label), Actor->IsWireframeEnabled());
		TestEqual(FString::Printf(TEXT("%s: slot 0 material is restored to the Grid variant material exactly (not the raw override array)"), Label), Actor->Mesh->GetMaterial(0), static_cast<UMaterialInterface*>(VariantMat));
	};

	// B1: overlay material missing -> fallback to the profile's slot material.
	CheckLegacyRoundTrip(TEXT("No overlay material"));

	// B2: overlay material present, legacy opt-in on.
	if (DefaultOverlay)
	{
		Actor->WireframeOverlayMaterial = DefaultOverlay;
		Actor->bWireframeReplacesSlots = true;
		CheckLegacyRoundTrip(TEXT("bWireframeReplacesSlots"));
	}

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

// Per-part highlight, mode (a): a part listing one of SKM_Manny_Simple's two
// material slots gets PartHighlightMaterial on exactly that slot, and every
// Wireframe / Variant / selection / Clean View round-trip restores exact
// materials (ApplyMaterialState() recomputes all slots).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerPartHighlightSlotsTest,
	"CharacterShowcase.Viewer.PartHighlightMaterialSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerPartHighlightSlotsTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerInspectionTestsPrivate;

	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	if (!TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny)
		|| !TestEqual(TEXT("SKM_Manny_Simple has 2 material slots"), Manny->GetMaterials().Num(), 2))
	{
		return false;
	}

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}
	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	if (!TestNotNull(TEXT("Character actor exists"), Actor)
		|| !TestNotNull(TEXT("PartHighlightMaterial (M_ViewerPartHighlight) is loaded by default"), Actor->PartHighlightMaterial.Get()))
	{
		World->DestroyWorld(false);
		return false;
	}

	UMaterialInterface* PartMat = Actor->PartHighlightMaterial;
	UMaterialInterface* Default0 = Manny->GetMaterials()[0].MaterialInterface;
	UMaterialInterface* Default1 = Manny->GetMaterials()[1].MaterialInterface;
	const FName Slot1Name = Manny->GetMaterials()[1].MaterialSlotName;
	AddInfo(FString::Printf(TEXT("SKM_Manny_Simple slots: 0='%s' (%s), 1='%s' (%s)"),
		*Manny->GetMaterials()[0].MaterialSlotName.ToString(), *GetNameSafe(Default0), *Slot1Name.ToString(), *GetNameSafe(Default1)));

	UMaterial* WireframeMat = NewObject<UMaterial>(World);
	UMaterial* AltMat = NewObject<UMaterial>(World);

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->SkeletalMesh = Manny;
	Profile->WireframeMaterial = WireframeMat;
	// This test covers the slot-precedence rules of the LEGACY slot-replacing
	// wireframe (the default shaded overlay never touches slots and is covered
	// by Viewer.WireframeOverlay).
	Actor->bWireframeReplacesSlots = true;

	FViewerMaterialSlotOverride AltSlot;
	AltSlot.SlotIndex = 0;
	AltSlot.Material = AltMat;
	FViewerMaterialVariant AltVariant;
	AltVariant.Id = FName(TEXT("Alt"));
	AltVariant.Slots.Add(AltSlot);
	Profile->MaterialVariants.Add(AltVariant);

	// Slots take precedence over bones: this part also lists a real bone.
	const FName PartId(TEXT("SlotPart"));
	FViewerPartInfo SlotPart;
	SlotPart.Id = PartId;
	SlotPart.MaterialSlotNames = { Slot1Name, FName(TEXT("NoSuchSlot")) };
	SlotPart.BoneNames = { FName(TEXT("upperarm_l")) };
	Profile->Parts.Add(SlotPart);

	Actor->ApplyProfile(Profile);
	USkeletalMeshComponent* Mesh = Actor->Mesh;

	auto ExpectSlots = [this, Mesh](const TCHAR* Step, UMaterialInterface* Expected0, UMaterialInterface* Expected1)
	{
		TestEqual(FString::Printf(TEXT("%s: slot 0 is %s"), Step, *GetNameSafe(Expected0)), Mesh->GetMaterial(0), Expected0);
		TestEqual(FString::Printf(TEXT("%s: slot 1 is %s"), Step, *GetNameSafe(Expected1)), Mesh->GetMaterial(1), Expected1);
	};

	// 1. Select: exactly slot 1 is highlighted, no overlay, no markers.
	Actor->SetSelectedPart(PartId);
	TestEqual(TEXT("Selecting a part with resolvable MaterialSlotNames uses MaterialSlots mode"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::MaterialSlots));
	ExpectSlots(TEXT("Selected"), Default0, PartMat);
	TestNull(TEXT("No whole-mesh overlay in MaterialSlots mode"), Mesh->GetOverlayMaterial());
	TestEqual(TEXT("No bone markers in MaterialSlots mode"), Actor->GetBoneMarkerCount(), 0);

	// Measured part stats come from slot 1.
	FViewerSlotStats PartStats;
	if (TestTrue(TEXT("GetPartMeasuredStats succeeds for a part with a resolvable slot"), Actor->GetPartMeasuredStats(PartId, PartStats)))
	{
		const TArray<FViewerSlotStats> SlotStats = Actor->GetSlotStats();
		AddInfo(FString::Printf(TEXT("Part stats: slot %d '%s', %d tris, material '%s', %s"),
			PartStats.SlotIndex, *PartStats.SlotName.ToString(), PartStats.Triangles, *PartStats.MaterialName.ToString(), *PartStats.TextureSummary));
		TestEqual(TEXT("Part stats slot index is 1"), PartStats.SlotIndex, 1);
		TestTrue(TEXT("Part stats triangles > 0"), PartStats.Triangles > 0);
		if (TestEqual(TEXT("GetSlotStats has 2 entries"), SlotStats.Num(), 2))
		{
			TestEqual(TEXT("Part triangles equal slot 1 triangles"), PartStats.Triangles, SlotStats[1].Triangles);
		}
	}
	FViewerSlotStats Unused;
	TestFalse(TEXT("GetPartMeasuredStats fails for an unknown part"), Actor->GetPartMeasuredStats(FName(TEXT("Nope")), Unused));

	// 2. Wireframe on: highlighted slot stays highlighted, the other is wireframe.
	TestTrue(TEXT("SetWireframeEnabled(true)"), Actor->SetWireframeEnabled(true));
	ExpectSlots(TEXT("Wireframe on + selected"), WireframeMat, PartMat);

	// 3. Clearing the selection restores wireframe on the highlighted slot.
	Actor->ClearSelectedPart();
	TestEqual(TEXT("Mode None after ClearSelectedPart"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::None));
	ExpectSlots(TEXT("Wireframe on, selection cleared"), WireframeMat, WireframeMat);

	// 4. Re-select, then Variant change while Wireframe is on keeps the highlight.
	Actor->SetSelectedPart(PartId);
	ExpectSlots(TEXT("Wireframe on, re-selected"), WireframeMat, PartMat);
	TestTrue(TEXT("SetMaterialVariant('Alt') while Wireframe is on"), Actor->SetMaterialVariant(FName(TEXT("Alt"))));
	ExpectSlots(TEXT("Wireframe on, Alt variant, selected"), WireframeMat, PartMat);

	// 5. Wireframe off: current Variant on every slot, highlight re-applied.
	TestTrue(TEXT("SetWireframeEnabled(false)"), Actor->SetWireframeEnabled(false));
	ExpectSlots(TEXT("Wireframe off, Alt variant, selected"), AltMat, PartMat);

	// 6. Clean View round-trip.
	Actor->SetHighlightVisible(false);
	TestEqual(TEXT("Mode None while the highlight is hidden"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::None));
	ExpectSlots(TEXT("Highlight hidden"), AltMat, Default1);
	TestFalse(TEXT("Custom Depth off while hidden"), Mesh->bRenderCustomDepth != 0);
	Actor->SetHighlightVisible(true);
	TestEqual(TEXT("MaterialSlots mode again after un-hiding"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::MaterialSlots));
	ExpectSlots(TEXT("Highlight shown again"), AltMat, PartMat);

	// 7. Variant back to defaults keeps the highlight.
	TestTrue(TEXT("SetMaterialVariant(None)"), Actor->SetMaterialVariant(NAME_None));
	ExpectSlots(TEXT("Default variant, selected"), Default0, PartMat);

	// 8. Clearing the selection leaves no override at all.
	Actor->ClearSelectedPart();
	ExpectSlots(TEXT("Default variant, selection cleared"), Default0, Default1);
	TestEqual(TEXT("No material override remains after clearing"), Mesh->GetNumOverrideMaterials(), 0);

	// 9. Profile switch removes everything.
	Actor->SetSelectedPart(PartId);
	Actor->SetWireframeEnabled(true);
	Actor->ApplyProfile(nullptr);
	TestEqual(TEXT("ApplyProfile(nullptr): mode None"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::None));
	TestEqual(TEXT("ApplyProfile(nullptr): no material override"), Mesh->GetNumOverrideMaterials(), 0);
	TestNull(TEXT("ApplyProfile(nullptr): no overlay"), Mesh->GetOverlayMaterial());
	TestEqual(TEXT("ApplyProfile(nullptr): no bone markers"), Actor->GetBoneMarkerCount(), 0);

	World->DestroyWorld(false);
	return true;
}

// Per-part highlight, modes (b) and (c) on TutorialTPP (1 material slot):
// bone markers on the part's bones + direct children, hidden in Clean View,
// destroyed on clear / profile switch; an unknown bone falls back to the
// whole-mesh overlay.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerPartHighlightBonesTest,
	"CharacterShowcase.Viewer.PartHighlightBoneMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerPartHighlightBonesTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerInspectionTestsPrivate;

	USkeletalMesh* Tutorial = LoadObject<USkeletalMesh>(nullptr, TutorialMeshPath);
	if (!TestNotNull(TEXT("TutorialTPP loads"), Tutorial))
	{
		return false;
	}

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}
	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	if (!TestNotNull(TEXT("Character actor exists"), Actor)
		|| !TestNotNull(TEXT("PartHighlightMaterial is loaded by default"), Actor->PartHighlightMaterial.Get())
		|| !TestNotNull(TEXT("BoneMarkerMesh (/Engine/BasicShapes/Sphere) is loaded by default"), Actor->BoneMarkerMesh.Get()))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FName ArmBone(TEXT("upperarm_l"));
	const FReferenceSkeleton& RefSkeleton = Tutorial->GetRefSkeleton();
	const int32 ArmBoneIndex = RefSkeleton.FindBoneIndex(ArmBone);
	if (!TestTrue(TEXT("TutorialTPP has 'upperarm_l'"), ArmBoneIndex != INDEX_NONE))
	{
		World->DestroyWorld(false);
		return false;
	}
	TArray<int32> ChildIndices;
	RefSkeleton.GetDirectChildBones(ArmBoneIndex, ChildIndices);
	TArray<FName> ExpectedBones = { ArmBone };
	for (const int32 ChildIndex : ChildIndices)
	{
		ExpectedBones.AddUnique(RefSkeleton.GetBoneName(ChildIndex));
	}

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->SkeletalMesh = Tutorial;

	const FName ArmId(TEXT("LeftUpperArm"));
	FViewerPartInfo ArmPart;
	ArmPart.Id = ArmId;
	ArmPart.BoneNames = { ArmBone };
	// A slot name that does not exist must not switch to MaterialSlots mode.
	ArmPart.MaterialSlotNames = { FName(TEXT("NoSuchSlot")) };
	Profile->Parts.Add(ArmPart);

	const FName GhostId(TEXT("Ghost"));
	FViewerPartInfo GhostPart;
	GhostPart.Id = GhostId;
	GhostPart.BoneNames = { FName(TEXT("no_such_bone")) };
	Profile->Parts.Add(GhostPart);

	Actor->ApplyProfile(Profile);
	USkeletalMeshComponent* Mesh = Actor->Mesh;
	UMaterialInterface* Default0 = Tutorial->GetMaterials().Num() > 0 ? Tutorial->GetMaterials()[0].MaterialInterface.Get() : nullptr;

	// (b) Bone markers.
	Actor->SetSelectedPart(ArmId);
	const TArray<FName> MarkerBones = Actor->GetBoneMarkerBoneNames();
	AddInfo(FString::Printf(TEXT("upperarm_l markers (%d): %s"), MarkerBones.Num(),
		*FString::JoinBy(MarkerBones, TEXT(", "), [](const FName& Name) { return Name.ToString(); })));
	TestEqual(TEXT("Bone part uses BoneMarkers mode"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::BoneMarkers));
	TestEqual(TEXT("One visible marker per part bone + direct child"), Actor->GetVisibleBoneMarkerCount(), ExpectedBones.Num());
	TestTrue(TEXT("upperarm_l has at least one child bone (elbow)"), ExpectedBones.Num() >= 2);
	TestTrue(TEXT("Markers include the part bone"), MarkerBones.Contains(ArmBone));
	TestTrue(TEXT("Markers include the elbow (lowerarm_l)"), MarkerBones.Contains(FName(TEXT("lowerarm_l"))));
	TestNull(TEXT("No whole-mesh overlay with bone markers (opt-in tint is off)"), Mesh->GetOverlayMaterial());
	TestTrue(TEXT("Custom Depth on with bone markers"), Mesh->bRenderCustomDepth != 0);
	TestEqual(TEXT("Slot 0 keeps its own material in BoneMarkers mode"), Mesh->GetMaterial(0), Default0);

	// Clean View hides (keeps pooled) markers.
	Actor->SetHighlightVisible(false);
	TestEqual(TEXT("No visible marker while hidden"), Actor->GetVisibleBoneMarkerCount(), 0);
	TestEqual(TEXT("Markers stay pooled while hidden"), Actor->GetBoneMarkerCount(), ExpectedBones.Num());
	TestEqual(TEXT("Mode None while hidden"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::None));
	Actor->SetHighlightVisible(true);
	TestEqual(TEXT("Markers visible again"), Actor->GetVisibleBoneMarkerCount(), ExpectedBones.Num());

	// (c) Unknown bone -> whole-mesh fallback; markers hidden.
	Actor->SetSelectedPart(GhostId);
	TestEqual(TEXT("Part with no resolvable slot/bone falls back to WholeMesh"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::WholeMesh));
	TestEqual(TEXT("No visible marker in WholeMesh mode"), Actor->GetVisibleBoneMarkerCount(), 0);
	if (Actor->HighlightOverlayMaterial)
	{
		TestEqual(TEXT("WholeMesh mode sets the overlay tint"), Mesh->GetOverlayMaterial(), Actor->HighlightOverlayMaterial.Get());
	}

	// Back to the arm: pooled markers are reused, overlay removed.
	Actor->SetSelectedPart(ArmId);
	TestEqual(TEXT("Markers reused after switching back"), Actor->GetVisibleBoneMarkerCount(), ExpectedBones.Num());
	TestEqual(TEXT("Pool did not grow"), Actor->GetBoneMarkerCount(), ExpectedBones.Num());
	TestNull(TEXT("Overlay removed when leaving WholeMesh mode"), Mesh->GetOverlayMaterial());

	// Clear destroys markers.
	Actor->ClearSelectedPart();
	TestEqual(TEXT("ClearSelectedPart destroys every marker"), Actor->GetBoneMarkerCount(), 0);
	TestFalse(TEXT("Custom Depth off after clear"), Mesh->bRenderCustomDepth != 0);

	// Profile switch destroys markers.
	Actor->SetSelectedPart(ArmId);
	TestTrue(TEXT("Markers exist before ApplyProfile(nullptr)"), Actor->GetBoneMarkerCount() > 0);
	Actor->ApplyProfile(nullptr);
	TestEqual(TEXT("ApplyProfile(nullptr) destroys every marker"), Actor->GetBoneMarkerCount(), 0);
	TestEqual(TEXT("ApplyProfile(nullptr): mode None"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::None));

	World->DestroyWorld(false);
	return true;
}

// Measured mesh stats (LOD0 render data) replace the hand-authored guesses.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerMeshStatsTest,
	"CharacterShowcase.Viewer.MeshStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerMeshStatsTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerInspectionTestsPrivate;

	USkeletalMesh* Tutorial = LoadObject<USkeletalMesh>(nullptr, TutorialMeshPath);
	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	if (!TestNotNull(TEXT("TutorialTPP loads"), Tutorial) || !TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny))
	{
		return false;
	}

	UWorld* World = CreateTestWorld();
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

	// No mesh: zeros, not valid.
	const FViewerMeshStats Empty = Actor->GetMeshStats();
	TestFalse(TEXT("No mesh: bValid is false"), Empty.bValid);
	TestEqual(TEXT("No mesh: 0 triangles"), Empty.Triangles, 0);
	TestEqual(TEXT("No mesh: no slot stats"), Actor->GetSlotStats().Num(), 0);

	auto LogMeshStats = [this](const TCHAR* Label, const FViewerMeshStats& Stats, const TArray<FViewerSlotStats>& Slots)
	{
		AddInfo(FString::Printf(TEXT("%s: valid=%d tris=%d verts=%d bones=%d slots=%d LODs=%d morphs=%d skeleton=%s physics=%s"),
			Label, Stats.bValid ? 1 : 0, Stats.Triangles, Stats.Vertices, Stats.Bones, Stats.MaterialSlots, Stats.LODs, Stats.MorphTargets,
			*Stats.SkeletonName.ToString(), *Stats.PhysicsAssetName.ToString()));
		for (const FViewerSlotStats& Slot : Slots)
		{
			AddInfo(FString::Printf(TEXT("  slot %d '%s': %d tris, material %s, %s (count %d, max %d)"),
				Slot.SlotIndex, *Slot.SlotName.ToString(), Slot.Triangles, *Slot.MaterialName.ToString(), *Slot.TextureSummary, Slot.TextureCount, Slot.MaxTextureSize));
		}
	};

	UCharacterProfileData* TutorialProfile = NewObject<UCharacterProfileData>(World);
	TutorialProfile->SkeletalMesh = Tutorial;
	Actor->ApplyProfile(TutorialProfile);
	const FViewerMeshStats TutorialStats = Actor->GetMeshStats();
	const TArray<FViewerSlotStats> TutorialSlots = Actor->GetSlotStats();
	LogMeshStats(TEXT("TutorialTPP"), TutorialStats, TutorialSlots);
	TestTrue(TEXT("TutorialTPP: bValid"), TutorialStats.bValid);
	TestEqual(TEXT("TutorialTPP: 6118 LOD0 triangles"), TutorialStats.Triangles, 6118);
	TestEqual(TEXT("TutorialTPP: 1 material slot"), TutorialStats.MaterialSlots, 1);
	TestTrue(TEXT("TutorialTPP: has bones"), TutorialStats.Bones > 0);
	TestTrue(TEXT("TutorialTPP: has a physics asset"), TutorialStats.PhysicsAssetName != NAME_None);
	if (TestEqual(TEXT("TutorialTPP: 1 slot stats entry"), TutorialSlots.Num(), 1))
	{
		TestEqual(TEXT("TutorialTPP: slot 0 has every triangle"), TutorialSlots[0].Triangles, 6118);
	}

	UCharacterProfileData* MannyProfile = NewObject<UCharacterProfileData>(World);
	MannyProfile->SkeletalMesh = Manny;
	Actor->ApplyProfile(MannyProfile);
	const FViewerMeshStats MannyStats = Actor->GetMeshStats();
	const TArray<FViewerSlotStats> MannySlots = Actor->GetSlotStats();
	LogMeshStats(TEXT("SKM_Manny_Simple"), MannyStats, MannySlots);
	TestTrue(TEXT("Manny: bValid"), MannyStats.bValid);
	TestEqual(TEXT("Manny: 2 material slots"), MannyStats.MaterialSlots, 2);
	TestTrue(TEXT("Manny: triangles > 0"), MannyStats.Triangles > 0);
	if (TestEqual(TEXT("Manny: 2 slot stats entries"), MannySlots.Num(), 2))
	{
		TestEqual(TEXT("Manny: slot triangles add up to the mesh total"), MannySlots[0].Triangles + MannySlots[1].Triangles, MannyStats.Triangles);
		// MI_Manny_01_New/02_New are textured (T_Manny_0x_D/_BN/_MRA in this project).
		TestTrue(TEXT("Manny: slot 0 material reports at least one texture"), MannySlots[0].TextureCount > 0 && MannySlots[0].MaxTextureSize > 0);
		TestTrue(TEXT("Manny: slot 1 material reports at least one texture"), MannySlots[1].TextureCount > 0 && MannySlots[1].MaxTextureSize > 0);
	}

	World->DestroyWorld(false);
	return true;
}

// SetAnimation's skeleton check: same skeleton or runtime-compatible accepted,
// a genuinely different rig rejected, null inputs safe.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerAnimationSkeletonTest,
	"CharacterShowcase.Viewer.AnimationSkeletonCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerAnimationSkeletonTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerInspectionTestsPrivate;

	// Null safety needs no content.
	TestFalse(TEXT("IsAnimationSkeletonCompatible(null, null) is false"), APortfolioCharacterActor::IsAnimationSkeletonCompatible(nullptr, nullptr));

	USkeletalMesh* Tutorial = LoadObject<USkeletalMesh>(nullptr, TutorialMeshPath);
	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	UAnimSequence* TutorialIdle = LoadObject<UAnimSequence>(nullptr, TutorialIdlePath);
	UAnimSequence* MannyIdle = LoadObject<UAnimSequence>(nullptr, MannyIdlePath);
	if (!TestNotNull(TEXT("TutorialTPP loads"), Tutorial) || !TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny)
		|| !TestNotNull(TEXT("Tutorial_Idle loads"), TutorialIdle) || !TestNotNull(TEXT("MM_Idle loads"), MannyIdle))
	{
		return false;
	}

	TestFalse(TEXT("Null skeleton is rejected"), APortfolioCharacterActor::IsAnimationSkeletonCompatible(nullptr, Tutorial));
	TestFalse(TEXT("Null mesh is rejected"), APortfolioCharacterActor::IsAnimationSkeletonCompatible(Tutorial->GetSkeleton(), nullptr));
	TestTrue(TEXT("Same skeleton is compatible"), APortfolioCharacterActor::IsAnimationSkeletonCompatible(Tutorial->GetSkeleton(), Tutorial));
	TestTrue(TEXT("Mannequin skeleton is compatible with SKM_Manny_Simple"), APortfolioCharacterActor::IsAnimationSkeletonCompatible(MannyIdle->GetSkeleton(), Manny));
	TestFalse(TEXT("UE5 mannequin skeleton is NOT compatible with the UE4 TutorialTPP rig"), APortfolioCharacterActor::IsAnimationSkeletonCompatible(MannyIdle->GetSkeleton(), Tutorial));

	UWorld* World = CreateTestWorld();
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

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->SkeletalMesh = Tutorial;
	FViewerAnimationEntry Own;
	Own.Id = FName(TEXT("Own"));
	Own.Sequence = TutorialIdle;
	Profile->Animations.Add(Own);
	FViewerAnimationEntry Foreign;
	Foreign.Id = FName(TEXT("Foreign"));
	Foreign.Sequence = MannyIdle;
	Profile->Animations.Add(Foreign);
	FViewerAnimationEntry NoSequence;
	NoSequence.Id = FName(TEXT("NoSequence"));
	Profile->Animations.Add(NoSequence);

	Actor->ApplyProfile(Profile);
	TestTrue(TEXT("SetAnimation accepts a sequence of the mesh's own skeleton"), Actor->SetAnimation(FName(TEXT("Own"))));
	TestEqual(TEXT("Current animation is 'Own'"), Actor->GetCurrentAnimationId(), FName(TEXT("Own")));
	AddExpectedMessagePlain(TEXT("sequence skeleton is not compatible"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("SetAnimation rejects an incompatible rig's sequence"), Actor->SetAnimation(FName(TEXT("Foreign"))));
	TestEqual(TEXT("Rejected animation keeps the previous id"), Actor->GetCurrentAnimationId(), FName(TEXT("Own")));
	TestFalse(TEXT("SetAnimation rejects an entry without a Sequence"), Actor->SetAnimation(FName(TEXT("NoSequence"))));

	World->DestroyWorld(false);
	return true;
}

// Shaded wireframe (default Overlay mode) on SKM_Manny_Simple: the overlay
// material is set/cleared, material slots are never touched, Variant and the
// per-part highlight stay visible under it, the WholeMesh tint yields to it,
// and every round-trip is exact.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerWireframeOverlayTest,
	"CharacterShowcase.Viewer.WireframeOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerWireframeOverlayTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerInspectionTestsPrivate;

	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	if (!TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny)
		|| !TestEqual(TEXT("SKM_Manny_Simple has 2 material slots"), Manny->GetMaterials().Num(), 2))
	{
		return false;
	}

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}
	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	if (!TestNotNull(TEXT("Character actor exists"), Actor)
		|| !TestNotNull(TEXT("WireframeOverlayMaterial (M_WireframeOverlay) is loaded by default"), Actor->WireframeOverlayMaterial.Get())
		|| !TestNotNull(TEXT("HighlightOverlayMaterial (M_ViewerHighlight) is loaded by default"), Actor->HighlightOverlayMaterial.Get())
		|| !TestNotNull(TEXT("PartHighlightMaterial is loaded by default"), Actor->PartHighlightMaterial.Get()))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestFalse(TEXT("bWireframeReplacesSlots is off by default"), Actor->bWireframeReplacesSlots);

	UMaterialInterface* WireOverlay = Actor->WireframeOverlayMaterial;
	UMaterialInterface* Tint = Actor->HighlightOverlayMaterial;
	UMaterialInterface* PartMat = Actor->PartHighlightMaterial;
	UMaterialInterface* Default0 = Manny->GetMaterials()[0].MaterialInterface;
	UMaterialInterface* Default1 = Manny->GetMaterials()[1].MaterialInterface;

	if (UMaterial* OverlayBase = WireOverlay->GetMaterial())
	{
		TestTrue(TEXT("M_WireframeOverlay has Wireframe on"), (bool)OverlayBase->Wireframe);
		TestTrue(TEXT("M_WireframeOverlay is usable on skeletal meshes"), (bool)OverlayBase->bUsedWithSkeletalMesh);
		TestEqual(TEXT("M_WireframeOverlay is opaque"), (int32)OverlayBase->BlendMode, (int32)BLEND_Opaque);
	}

	UMaterial* LegacyWireframe = NewObject<UMaterial>(World);
	UMaterial* AltMat = NewObject<UMaterial>(World);
	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->SkeletalMesh = Manny;
	// The legacy slot material is present but must NOT be used by default.
	Profile->WireframeMaterial = LegacyWireframe;

	FViewerMaterialSlotOverride AltSlot;
	AltSlot.SlotIndex = 0;
	AltSlot.Material = AltMat;
	FViewerMaterialVariant AltVariant;
	AltVariant.Id = FName(TEXT("Alt"));
	AltVariant.Slots.Add(AltSlot);
	Profile->MaterialVariants.Add(AltVariant);

	const FName SlotPartId(TEXT("SlotPart"));
	FViewerPartInfo SlotPart;
	SlotPart.Id = SlotPartId;
	SlotPart.MaterialSlotNames = { Manny->GetMaterials()[1].MaterialSlotName };
	Profile->Parts.Add(SlotPart);

	const FName BonePartId(TEXT("BonePart"));
	FViewerPartInfo BonePart;
	BonePart.Id = BonePartId;
	BonePart.BoneNames = { FName(TEXT("upperarm_l")) };
	Profile->Parts.Add(BonePart);

	const FName GhostId(TEXT("Ghost"));
	FViewerPartInfo GhostPart;
	GhostPart.Id = GhostId;
	GhostPart.BoneNames = { FName(TEXT("no_such_bone")) };
	Profile->Parts.Add(GhostPart);

	Actor->ApplyProfile(Profile);
	USkeletalMeshComponent* Mesh = Actor->Mesh;

	auto Expect = [this, Mesh](const FString& Step, UMaterialInterface* Expected0, UMaterialInterface* Expected1, UMaterialInterface* ExpectedOverlay)
	{
		TestEqual(FString::Printf(TEXT("%s: slot 0 is %s"), *Step, *GetNameSafe(Expected0)), Mesh->GetMaterial(0), Expected0);
		TestEqual(FString::Printf(TEXT("%s: slot 1 is %s"), *Step, *GetNameSafe(Expected1)), Mesh->GetMaterial(1), Expected1);
		TestEqual(FString::Printf(TEXT("%s: overlay is %s"), *Step, *GetNameSafe(ExpectedOverlay)), Mesh->GetOverlayMaterial(), ExpectedOverlay);
	};
	auto WireMode = [Actor]() { return UEnum::GetValueAsString(Actor->GetActiveWireframeMode()); };

	// 1. On: overlay set, slots untouched (no override at all).
	TestTrue(TEXT("Wireframe is available"), Actor->IsWireframeAvailable());
	TestTrue(TEXT("SetWireframeEnabled(true)"), Actor->SetWireframeEnabled(true));
	TestEqual(TEXT("Default wireframe mode is Overlay"), WireMode(), UEnum::GetValueAsString(EViewerWireframeMode::Overlay));
	Expect(TEXT("Wireframe on"), Default0, Default1, WireOverlay);
	TestEqual(TEXT("Wireframe on: no material override (slots untouched)"), Mesh->GetNumOverrideMaterials(), 0);

	// 2. Variant under the overlay is visible.
	TestTrue(TEXT("SetMaterialVariant('Alt') while Wireframe is on"), Actor->SetMaterialVariant(FName(TEXT("Alt"))));
	Expect(TEXT("Wireframe on + Alt"), AltMat, Default1, WireOverlay);

	// 3. MaterialSlots highlight under the overlay.
	Actor->SetSelectedPart(SlotPartId);
	TestEqual(TEXT("Slot part: MaterialSlots mode"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::MaterialSlots));
	Expect(TEXT("Wireframe on + Alt + slot part"), AltMat, PartMat, WireOverlay);

	// 4. BoneMarkers under the overlay.
	Actor->SetSelectedPart(BonePartId);
	TestEqual(TEXT("Bone part: BoneMarkers mode"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::BoneMarkers));
	TestTrue(TEXT("Bone part: markers visible with Wireframe on"), Actor->GetVisibleBoneMarkerCount() > 0);
	Expect(TEXT("Wireframe on + Alt + bone part"), AltMat, Default1, WireOverlay);

	// 5. WholeMesh: the wireframe keeps the single overlay slot, Custom Depth only.
	Actor->SetSelectedPart(GhostId);
	TestEqual(TEXT("Ghost part: WholeMesh mode"), ModeName(Actor->GetActiveHighlightMode()), ModeName(EViewerHighlightMode::WholeMesh));
	Expect(TEXT("Wireframe on + WholeMesh"), AltMat, Default1, WireOverlay);
	TestTrue(TEXT("Wireframe on + WholeMesh: Custom Depth on"), Mesh->bRenderCustomDepth != 0);

	// 6. Clean View hides the selection, not the wireframe.
	Actor->SetHighlightVisible(false);
	Expect(TEXT("Wireframe on + highlight hidden"), AltMat, Default1, WireOverlay);
	TestFalse(TEXT("Highlight hidden: Custom Depth off"), Mesh->bRenderCustomDepth != 0);
	Actor->SetHighlightVisible(true);

	// 7. Off: the WholeMesh tint takes the overlay slot back.
	TestTrue(TEXT("SetWireframeEnabled(false)"), Actor->SetWireframeEnabled(false));
	TestEqual(TEXT("Wireframe off: mode None"), WireMode(), UEnum::GetValueAsString(EViewerWireframeMode::None));
	Expect(TEXT("Wireframe off + WholeMesh"), AltMat, Default1, Tint);

	// 8. Clear selection and variant: exact defaults, nothing left over.
	Actor->ClearSelectedPart();
	TestTrue(TEXT("SetMaterialVariant(None)"), Actor->SetMaterialVariant(NAME_None));
	Expect(TEXT("All off"), Default0, Default1, nullptr);
	TestEqual(TEXT("All off: no material override"), Mesh->GetNumOverrideMaterials(), 0);

	// 9. Repeated round-trip stays exact.
	for (int32 Round = 0; Round < 2; ++Round)
	{
		Actor->SetWireframeEnabled(true);
		Actor->SetWireframeEnabled(false);
	}
	Expect(TEXT("After 2 more on/off rounds"), Default0, Default1, nullptr);
	TestEqual(TEXT("After 2 more on/off rounds: no material override"), Mesh->GetNumOverrideMaterials(), 0);

	// 10. Legacy opt-in on the same profile: slots replaced, no overlay.
	Actor->bWireframeReplacesSlots = true;
	TestTrue(TEXT("Legacy: SetWireframeEnabled(true)"), Actor->SetWireframeEnabled(true));
	TestEqual(TEXT("Legacy: mode ReplaceSlots"), WireMode(), UEnum::GetValueAsString(EViewerWireframeMode::ReplaceSlots));
	Expect(TEXT("Legacy on"), LegacyWireframe, LegacyWireframe, nullptr);
	Actor->SetWireframeEnabled(false);
	Actor->bWireframeReplacesSlots = false;
	Expect(TEXT("Legacy off"), Default0, Default1, nullptr);

	// 11. Profile switch clears the overlay and the flag.
	Actor->SetWireframeEnabled(true);
	Actor->ApplyProfile(nullptr);
	TestFalse(TEXT("ApplyProfile(nullptr) turns Wireframe off"), Actor->IsWireframeEnabled());
	TestNull(TEXT("ApplyProfile(nullptr) clears the wireframe overlay"), Mesh->GetOverlayMaterial());
	TestFalse(TEXT("No profile: Wireframe unavailable"), Actor->IsWireframeAvailable());

	World->DestroyWorld(false);
	return true;
}

// INSPECTION panel text (C++ BuildInspectionSection, shared by the fallback and
// designer layouts) shows measured numbers for SKM_Manny_Simple.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerInspectionPanelTextTest,
	"CharacterShowcase.Viewer.InspectionPanelText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerInspectionPanelTextTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerInspectionTestsPrivate;

	TestEqual(TEXT("FormatThousands(92178)"), UCharacterViewerWidget::FormatThousands(92178), FString(TEXT("92,178")));
	TestEqual(TEXT("FormatThousands(999)"), UCharacterViewerWidget::FormatThousands(999), FString(TEXT("999")));
	TestEqual(TEXT("FormatThousands(1000000)"), UCharacterViewerWidget::FormatThousands(1000000), FString(TEXT("1,000,000")));
	TestEqual(TEXT("FormatThousands(0)"), UCharacterViewerWidget::FormatThousands(0), FString(TEXT("0")));

	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	if (!TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny))
	{
		return false;
	}

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}
	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
	UCharacterViewerWidget* Widget = CreateWidget<UCharacterViewerWidget>(World, UCharacterViewerWidget::StaticClass());
	if (!TestNotNull(TEXT("Character actor exists"), Actor) || !TestNotNull(TEXT("Viewer controller exists"), Controller)
		|| !TestNotNull(TEXT("Fallback widget exists"), Widget))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FString Dot = TEXT(" · ");

	// Without a mesh: measured n/a.
	UCharacterProfileData* NoMeshProfile = NewObject<UCharacterProfileData>(World);
	Actor->ApplyProfile(NoMeshProfile);
	Widget->BindToViewer(Controller, Actor, nullptr);
	const FString NoMeshText = Widget->BuildInspectionText().ToString();
	TestTrue(FString::Printf(TEXT("No mesh: 'measured: n/a' (got '%s')"), *NoMeshText), NoMeshText.Contains(TEXT("measured: n/a")));
	TestTrue(TEXT("No mesh: still 'Click a part'"), NoMeshText.Contains(TEXT("Click a part")));

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->SkeletalMesh = Manny;

	const FName SlotPartId(TEXT("TorsoSlots"));
	FViewerPartInfo SlotPart;
	SlotPart.Id = SlotPartId;
	SlotPart.DisplayName = FText::FromString(TEXT("Torso"));
	SlotPart.PartType = FText::FromString(TEXT("Body"));
	SlotPart.Description = FText::FromString(TEXT("Jacket and torso."));
	SlotPart.MaterialSlotNames = { FName(TEXT("M_Torso")) };
	SlotPart.TriangleCount = 1; // authored note must be replaced by the measurement
	Profile->Parts.Add(SlotPart);

	const FName BonePartId(TEXT("LeftArm"));
	FViewerPartInfo BonePart;
	BonePart.Id = BonePartId;
	BonePart.DisplayName = FText::FromString(TEXT("Left Arm"));
	BonePart.PartType = FText::FromString(TEXT("Limb"));
	BonePart.BoneNames = { FName(TEXT("upperarm_l")) };
	BonePart.TriangleCount = 19680;
	BonePart.MaterialName = FText::FromString(TEXT("MI_Manny_02_New 81%"));
	BonePart.TextureResolution = FText::FromString(TEXT("1024-4096"));
	Profile->Parts.Add(BonePart);

	const FName GhostId(TEXT("Ghost"));
	FViewerPartInfo GhostPart;
	GhostPart.Id = GhostId;
	GhostPart.DisplayName = FText::FromString(TEXT("Ghost"));
	GhostPart.BoneNames = { FName(TEXT("no_such_bone")) };
	Profile->Parts.Add(GhostPart);

	Actor->ApplyProfile(Profile);
	Controller->SetInspectionEnabled(true);
	// RebuildWidget() -> BuildFallbackUI(). Keep the Slate widget alive: when
	// the last reference to it is dropped, UUserWidget::NativeDestruct() runs
	// and unbinds the viewer (weak pointers reset).
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	Widget->BindToViewer(Controller, Actor, nullptr);
	Widget->NotifySelectionChanged();

	const FViewerMeshStats Stats = Actor->GetMeshStats();
	const TArray<FViewerSlotStats> Slots = Actor->GetSlotStats();
	if (!TestTrue(TEXT("Manny stats are valid"), Stats.bValid) || !TestEqual(TEXT("Manny has 2 slot stats"), Slots.Num(), 2))
	{
		SlateWidget.Reset();
		World->DestroyWorld(false);
		return false;
	}

	auto ExpectContains = [this](const TCHAR* Label, const FString& Body, const FString& Needle)
	{
		TestTrue(FString::Printf(TEXT("%s: contains '%s'"), Label, *Needle), Body.Contains(Needle));
	};

	// Nothing selected yet: mesh summary + slots + "Click a part", on the panel itself.
	const FString Idle = Widget->GetFallbackInspectionBodyText().ToString();
	AddInfo(FString::Printf(TEXT("INSPECTION (no selection):\n%s"), *Idle));
	TestEqual(TEXT("INSPECTION section is visible while Inspection is on"), Widget->GetFallbackInspectionSectionVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Panel body equals BuildInspectionText()"), Idle, Widget->BuildInspectionText().ToString());
	ExpectContains(TEXT("Idle"), Idle, FString(TEXT("Triangles 92,178")) + Dot + TEXT("Verts 48,705") + Dot + TEXT("Bones 89") + Dot + TEXT("Slots 2") + Dot + TEXT("LODs 3") + Dot + TEXT("Morphs 0"));
	ExpectContains(TEXT("Idle"), Idle, FString(TEXT("Skeleton SK_Mannequin")) + Dot + TEXT("Physics PA_Mannequin"));
	ExpectContains(TEXT("Idle"), Idle, FString(TEXT("M_HeadLegs: 38,166 tris")) + Dot + Slots[0].MaterialName.ToString() + Dot + Slots[0].TextureSummary);
	ExpectContains(TEXT("Idle"), Idle, FString(TEXT("M_Torso: 54,012 tris")) + Dot + TEXT("MI_Manny_02_New") + Dot + Slots[1].TextureSummary);
	ExpectContains(TEXT("Idle"), Idle, TEXT("max 4096x4096"));
	ExpectContains(TEXT("Idle"), Idle, TEXT("Click a part"));

	// Slot part: authored identity + measured numbers + MaterialSlots.
	Actor->SetSelectedPart(SlotPartId);
	Widget->NotifySelectionChanged();
	const FString SlotText = Widget->GetFallbackInspectionBodyText().ToString();
	AddInfo(FString::Printf(TEXT("INSPECTION (slot part):\n%s"), *SlotText));
	ExpectContains(TEXT("Slot part"), SlotText, TEXT("Torso (Body)"));
	ExpectContains(TEXT("Slot part"), SlotText, TEXT("Jacket and torso."));
	ExpectContains(TEXT("Slot part"), SlotText, FString(TEXT("Measured: 54,012 tris")) + Dot + TEXT("M_Torso") + Dot + TEXT("MI_Manny_02_New") + Dot + Slots[1].TextureSummary);
	ExpectContains(TEXT("Slot part"), SlotText, TEXT("Highlight: Material slots"));
	TestFalse(TEXT("Slot part: no 'Authored:' line when measured"), SlotText.Contains(TEXT("Authored:")));
	TestFalse(TEXT("Slot part: no 'Click a part'"), SlotText.Contains(TEXT("Click a part")));
	ExpectContains(TEXT("Slot part"), SlotText, TEXT("Triangles 92,178"));

	// Bone part: no resolvable slot -> authored notes + bone marker count.
	Actor->SetSelectedPart(BonePartId);
	Widget->NotifySelectionChanged();
	const FString BoneText = Widget->GetFallbackInspectionBodyText().ToString();
	const int32 MarkerCount = Actor->GetBoneMarkerBoneNames().Num();
	AddInfo(FString::Printf(TEXT("INSPECTION (bone part):\n%s"), *BoneText));
	TestTrue(TEXT("Bone part has markers"), MarkerCount > 0);
	ExpectContains(TEXT("Bone part"), BoneText, TEXT("Left Arm (Limb)"));
	ExpectContains(TEXT("Bone part"), BoneText, FString(TEXT("Authored: 19,680 tris")) + Dot + TEXT("MI_Manny_02_New 81%") + Dot + TEXT("1024-4096"));
	ExpectContains(TEXT("Bone part"), BoneText, FString::Printf(TEXT("Highlight: Bone markers (%d)"), MarkerCount));
	TestFalse(TEXT("Bone part: no 'Measured:' line"), BoneText.Contains(TEXT("Measured:")));

	// Ghost part: whole mesh; with the Wireframe overlay on the tint is hidden.
	Actor->SetSelectedPart(GhostId);
	Widget->NotifySelectionChanged();
	ExpectContains(TEXT("Ghost part"), Widget->GetFallbackInspectionBodyText().ToString(), TEXT("Highlight: Whole mesh"));
	if (Actor->SetWireframeEnabled(true))
	{
		Widget->NotifySelectionChanged();
		ExpectContains(TEXT("Ghost part + Wireframe"), Widget->GetFallbackInspectionBodyText().ToString(), TEXT("Highlight: Whole mesh (tint hidden while Wireframe is on)"));
		Actor->SetWireframeEnabled(false);
	}

	// Inspection off: section collapsed.
	Controller->SetInspectionEnabled(false);
	Widget->NotifySelectionChanged();
	TestEqual(TEXT("INSPECTION section collapses when Inspection is off"), Widget->GetFallbackInspectionSectionVisibility(), ESlateVisibility::Collapsed);

	Widget->RemoveFromParent();
	SlateWidget.Reset(); // NativeDestruct while the world still exists
	World->DestroyWorld(false);
	return true;
}

#endif
