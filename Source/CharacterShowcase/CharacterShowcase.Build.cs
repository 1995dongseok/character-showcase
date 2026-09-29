using UnrealBuildTool;

public class CharacterShowcase : ModuleRules
{
	public CharacterShowcase(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// This module has no Public/Private subfolder split; all headers are
		// included module-relative (e.g. "Character/CharacterProfileData.h"),
		// so the module root itself must be on the include path.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Slate",
			"SlateCore"
		});

		// Editor-only (Docs/CHARACTER_VIEWER_SETUP.md section 13.10.1,
		// Source/CharacterShowcase/Editor/CharacterViewerEditorTools.h/.cpp,
		// entirely #if WITH_EDITOR-guarded): never linked into a Game/packaged
		// build.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"UMGEditor",
				"UnrealEd",
				"Kismet"
			});
		}
	}
}
