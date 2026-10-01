#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterProfileValidator.generated.h"

class UCharacterProfileData;

// How serious one profile issue is.
//   Error   -- the feature is broken for this profile (a button does nothing,
//              an animation is refused, a slot never changes). Fix before showing.
//   Warning -- works, but probably not what the artist meant (a fallback is used,
//              a part is not clickable, a value looks wrong).
//   Info    -- a note (a feature is intentionally off, a large texture).
UENUM(BlueprintType)
enum class EViewerIssueSeverity : uint8
{
	Error,
	Warning,
	Info
};

// One finding of UCharacterProfileValidator::ValidateProfile(). Category is
// one of Mesh / Camera / Animation / Expression / Material / Part / Play
// (UCharacterProfileValidator::Category*). Message is Korean and names what
// is wrong, where (the Details field), and how to fix it. Field is the
// Details-panel path of the offending value, e.g. "Parts[2].BoneNames[0]".
USTRUCT(BlueprintType)
struct FViewerProfileIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	EViewerIssueSeverity Severity = EViewerIssueSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	FString Field;
};

// Read-only checks of a UCharacterProfileData against its Skeletal Mesh, so a
// junior artist sees exactly what is wrong with a profile before running the
// viewer (Docs/CHARACTER_VIEWER_SETUP.md section 2.10). Runtime-safe: works in
// the Editor, in -game and in a packaged build; the only editor-only check
// (mesh import scale) is compiled under WITH_EDITOR. Never modifies the
// profile, the mesh or any other asset. Called from C++ (viewer log on profile
// switch, tests), Blueprint and Python
// (unreal.CharacterProfileValidator.validate_profile, Scripts/ValidateProfiles.py).
UCLASS()
class CHARACTERSHOWCASE_API UCharacterProfileValidator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// All issues found in Profile, in a stable order (Mesh, Camera, Animation,
	// Expression, Material, Part, Play). A null Profile yields one Error.
	UFUNCTION(BlueprintCallable, Category = "Character|Validation")
	static TArray<FViewerProfileIssue> ValidateProfile(const UCharacterProfileData* Profile);

	// Multi-line report: a summary line "E=<n> W=<n> I=<n>" followed by one
	// line per issue "[Error][Mesh] <message> (<field>)".
	UFUNCTION(BlueprintPure, Category = "Character|Validation")
	static FString FormatReport(const TArray<FViewerProfileIssue>& Issues);

	UFUNCTION(BlueprintPure, Category = "Character|Validation")
	static int32 CountBySeverity(const TArray<FViewerProfileIssue>& Issues, EViewerIssueSeverity Severity);

	// Issues of one Category and Severity (used by tests and by callers that
	// want e.g. "Part errors only").
	UFUNCTION(BlueprintPure, Category = "Character|Validation")
	static int32 CountByCategory(const TArray<FViewerProfileIssue>& Issues, FName Category, EViewerIssueSeverity Severity);

	// "Error" / "Warning" / "Info".
	UFUNCTION(BlueprintPure, Category = "Character|Validation")
	static FString SeverityToString(EViewerIssueSeverity Severity);

	// Validates Profile and writes the report to the log (LogTemp; Log
	// verbosity only, so a profile with issues never fails an automation test
	// that switches profiles). Context is a short caller tag, e.g. "SwitchProfile".
	static void LogProfileReport(const UCharacterProfileData* Profile, const TCHAR* Context);

	// Category names used in FViewerProfileIssue::Category.
	static const FName CategoryMesh;
	static const FName CategoryCamera;
	static const FName CategoryAnimation;
	static const FName CategoryExpression;
	static const FName CategoryMaterial;
	static const FName CategoryPart;
	static const FName CategoryPlay;

	// Thresholds (documented in Docs/CHARACTER_VIEWER_SETUP.md section 2.10).
	static constexpr float MinFOV = 10.f;
	static constexpr float MaxFOV = 120.f;
	static constexpr int32 LargeTextureSize = 4096;
	static constexpr float MinMeshHeightCm = 50.f;
	static constexpr float MaxMeshHeightCm = 300.f;
	static constexpr int32 MaxListedNames = 10;
};
