using UnrealBuildTool;

public class CharacterShowcaseEditorTarget : TargetRules
{
	public CharacterShowcaseEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.Add("CharacterShowcase");
	}
}
